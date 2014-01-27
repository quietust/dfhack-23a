// Display Outside/Inside, Light/Dark, and Above Ground/Subterranean indicators

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

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::gps;

void OutputString(int8_t color, int &x, int y, const std::string &text)
{
    Screen::paintString(Screen::Pen(' ', color, 0), x, y, text);
    x += text.length();
}

bool inHook_dwarfmode_look = false;
struct dwarfmode_hook : df::viewscreen_dwarfmodest
{
    typedef df::viewscreen_dwarfmodest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        if (ui->main.mode == ui_sidebar_mode::LookAround)
            inHook_dwarfmode_look = true;
        INTERPOSE_NEXT(view)();
    }
};

DFhackCExport command_result plugin_onrender ( color_ostream &out)
{
    auto dims = Gui::getDwarfmodeViewDims();
    int x,  y;
    if (inHook_dwarfmode_look)
    {
        df::tile_designation flags = *Maps::getTileDesignation(Gui::getCursorPos());

        y = 23;
        x = dims.menu_x1 + 1;
        if (flags.bits.outside)
            OutputString(11, x, y, "Outside");
        else
            OutputString(6, x, y, "Inside");

        x = dims.menu_x1 + 9;
        if (flags.bits.light)
            OutputString(14, x, y, "Light");
        else
            OutputString(8, x, y, "Dark");

        x = dims.menu_x1 + 16;
        if (flags.bits.subterranean)
            OutputString(8, x, y, "Subterranean");
        else
            OutputString(10, x, y, "Above Ground");

        inHook_dwarfmode_look = false;
    }
    return CR_OK;
}

IMPLEMENT_VMETHOD_INTERPOSE(dwarfmode_hook, view);

DFHACK_PLUGIN("tile_status");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

DFhackCExport command_result plugin_enable(color_ostream &out, bool enable)
{
    if (!gps)
        return CR_FAILURE;

    if (enable != is_enabled)
    {
        if (!INTERPOSE_HOOK(dwarfmode_hook, view).apply(enable))
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
    return CR_OK;
}
