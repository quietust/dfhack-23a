#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"

#include "DataDefs.h"
#include "df/init.h"

using std::vector;
using std::string;
using std::endl;
using namespace DFHack;
using namespace df::enums;

using df::global::init;

command_result twaterlvl(color_ostream &out, vector <string> & parameters);
command_result tidlers(color_ostream &out, vector <string> & parameters);

DFHACK_PLUGIN("initflags");

DFhackCExport command_result plugin_init (color_ostream &out, std::vector <PluginCommand> &commands)
{
    if (init) {
        commands.push_back(PluginCommand("twaterlvl", "Toggle display of water/magma depth.",
                                         twaterlvl, Gui::dwarfmode_hotkey));
        commands.push_back(PluginCommand("tidlers", "Toggle display of idlers.",
                                         tidlers, Gui::dwarfmode_hotkey));
    }
    std::cerr << "d_init: " << sizeof(df::init) << endl;
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}

command_result twaterlvl(color_ostream &out, vector <string> & parameters)
{
    // HOTKEY COMMAND: CORE ALREADY SUSPENDED
    init->display.flags.toggle(init_display_flags::SHOW_FLOW_AMOUNTS);
    out << "Toggled the display of water/magma depth." << endl;
    return CR_OK;
}

command_result tidlers(color_ostream &out, vector <string> & parameters)
{
    // HOTKEY COMMAND: CORE ALREADY SUSPENDED
    init->display.idlers = ENUM_NEXT_ITEM(init_idlers, init->display.idlers);
    out << "Toggled the display of idlers to " << ENUM_KEY_STR(init_idlers, init->display.idlers) << endl;
    return CR_OK;
}
