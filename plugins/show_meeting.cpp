// Display "Attend Meeting" and "Conduct Meeting" job indicators

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <MiscUtils.h>
#include <modules/Gui.h>
#include <modules/Screen.h>
#include <modules/Units.h>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

#include <VTableInterpose.h>
#include "df/world.h"
#include "df/ui.h"
#include "df/ui_unit_view_mode.h"
#include "df/activity_info.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/viewscreen_unitjobsst.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::ui_unit_view_mode;
using df::global::ui_selected_unit;
using df::global::gps;

void OutputString(int8_t color, int &x, int y, const std::string &text)
{
    Screen::paintString(Screen::Pen(' ', color, 0), x, y, text);
    x += text.length();
}

bool inHook_dwarfmode = false;
struct dwarfmode_hook : df::viewscreen_dwarfmodest
{
    typedef df::viewscreen_dwarfmodest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        if ((ui->main.mode == ui_sidebar_mode::ViewUnits) && (*ui_selected_unit != -1) && (ui_unit_view_mode->value == df::ui_unit_view_mode::General))
            inHook_dwarfmode = true;
        INTERPOSE_NEXT(view)();
    }
};

bool inHook_unitjobs = false;
df::viewscreen_unitjobsst *inHook_viewscreen = NULL;
struct unitjobs_hook : df::viewscreen_unitjobsst
{
    typedef df::viewscreen_unitjobsst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        inHook_unitjobs = true;
        inHook_viewscreen = this;
        INTERPOSE_NEXT(view)();
    }
};

void printMeetingJob (int color, int x, int y, df::unit *unit)
{
    if (!unit)
        return;
    if (!Units::isCitizen(unit))
        return;
    if (unit->job.current_job)
        return;
    df::specific_ref *ref = Units::getSpecificRef(unit, specific_ref_type::ACTIVITY);
    if (!ref)
        return;
    df::activity_info *activity = ref->activity;
    if (activity->unit_actor != unit && activity->unit_actor && activity->unit_actor->meeting.state == 2)
    {
        OutputString(11, x, y, "Conduct Meeting");
        return;
    }
    if (!unit->status.guild_complaints.size())
        return;
    if (unit->mood != -1)
        return;

    {
        auto prof = unit->meeting.target_profession;
        if (prof == -1)
            return;
        if (ui->nobles_arrived[prof] < ui->units_killed[prof])
            return;
        if (unit->profession == prof)
            return;
    }

    if (unit->meeting.state < 1)
        return;
    OutputString(color, x, y, "Attend Meeting");
}

DFhackCExport command_result plugin_onrender ( color_ostream &out)
{
    auto dims = Gui::getDwarfmodeViewDims();
    int x, y, color;
    if (inHook_dwarfmode)
    {
        x = dims.menu_x1 + 1;
        y = 6;
        color = 11;
        df::unit *unit = world->units.active[*ui_selected_unit];
        printMeetingJob(color, x, y, unit);
        inHook_dwarfmode = false;
    }

    if (inHook_unitjobs && inHook_viewscreen)
    {
        inHook_dwarfmode = false;

        if (inHook_viewscreen->mode)
        {
            x = 40;
            color = 11;
        }
        else
        {
            x = 2;
            color = 3;
        }
        for (int i = 0; i < 19; i++)
        {
            int idx = inHook_viewscreen->cursor_pos - (inHook_viewscreen->cursor_pos % 19) + i;
            if (idx >= inHook_viewscreen->jobs.size())
                break;
            printMeetingJob(color, x, y, inHook_viewscreen->units[idx]);
        }
        inHook_unitjobs = false;
        inHook_viewscreen = NULL;
    }
    return CR_OK;
}

IMPLEMENT_VMETHOD_INTERPOSE(dwarfmode_hook, view);
IMPLEMENT_VMETHOD_INTERPOSE(unitjobs_hook, view);

DFHACK_PLUGIN("show_meeting");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

DFhackCExport command_result plugin_enable(color_ostream &out, bool enable)
{
    if (!gps)
        return CR_FAILURE;

    if (enable != is_enabled)
    {
        if (!INTERPOSE_HOOK(dwarfmode_hook, view).apply(enable))
            return CR_FAILURE;
        if (!INTERPOSE_HOOK(unitjobs_hook, view).apply(enable))
            return CR_FAILURE;

        is_enabled = enable;
    }

    return CR_OK;
}

DFhackCExport command_result plugin_init ( color_ostream &out, vector <PluginCommand> &commands)
{
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    INTERPOSE_HOOK(dwarfmode_hook, view).remove();
    INTERPOSE_HOOK(unitjobs_hook, view).remove();
    return CR_OK;
}
