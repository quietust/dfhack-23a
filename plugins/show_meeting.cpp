// Display "Attend Meeting" and "Conduct Meeting" job indicators

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <MiscUtils.h>
#include <modules/Gui.h>
#include <modules/Screen.h>
#include <modules/Maps.h>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

#include <VTableInterpose.h>
#include "df/world.h"
#include "df/ui.h"
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
        if ((ui->main.mode == ui_sidebar_mode::ViewUnits) && (ui_unit_view_mode->value == ui_unit_view_mode::T_value::General))
            inHook_dwarfmode = true;
        INTERPOSE_NEXT(view)();
    }
};

bool inHook_unitjobs = false;
struct unitjobs_hook : df::viewscreen_unitjobsst
{
    typedef df::viewscreen_unitjobsst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        inHook_unitjobs = true;
        INTERPOSE_NEXT(view)();
    }
};

DFhackCExport command_result plugin_onrender ( color_ostream &out)
{
    auto dims = Gui::getDwarfmodeViewDims();
    int x,  y;
    if (inHook_dwarfmode)
    {
        // TODO
        // ensure that unit is valid (fort member)
        // ensure unit has no job
        // check if dwarf is attending meeting

        x = dims.menu_x1 + 2;
        y = 6;
        // OutputString(12, x, y, "Conduct Meeting");
        inHook_dwarfmode = false;
    }

    if (inHook_unitjobs)
    {
        // TODO
        // iterate across units on current page
        // ensure units are valid (fort member)
        // ensure unit has no job
        // check if dwarf is attending meeting
        inHook_dwarfmode_look = false;

        if (mode)
            x = 40;
        else
            x = 2;
        y = 2;
        // OutputString(12, x, y, "Conduct Meeting");
        inHook_unitjobs = false;
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
