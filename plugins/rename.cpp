#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"

#include <Error.h>
#include <LuaTools.h>

#include "modules/Gui.h"
#include "modules/Translation.h"
#include "modules/Units.h"
#include "modules/World.h"
#include "modules/Screen.h"

#include <VTableInterpose.h>
#include "df/ui.h"
#include "df/ui_sidebar_menus.h"
#include "df/world.h"
#include "df/unit.h"
#include "df/historical_entity.h"
#include "df/historical_figure.h"
#include "df/language_name.h"
#include "df/viewscreen_dwarfmodest.h"

#include "RemoteServer.h"
#include "rename.pb.h"

#include "MiscUtils.h"

#include <stdlib.h>

using std::vector;
using std::string;
using std::endl;
using namespace DFHack;
using namespace df::enums;
using namespace dfproto;

using df::global::ui;
using df::global::ui_sidebar_menus;
using df::global::world;

DFhackCExport command_result plugin_onstatechange(color_ostream &out, state_change_event event);

static command_result rename(color_ostream &out, vector <string> & parameters);

DFHACK_PLUGIN("rename");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

DFhackCExport command_result plugin_init (color_ostream &out, std::vector <PluginCommand> &commands)
{
    if (world && ui) {
        commands.push_back(PluginCommand(
            "rename", "Rename various things.", rename, false,
            "  rename unit \"nickname\"\n"
            "  rename unit-profession \"custom profession\"\n"
            "    (a unit must be highlighted in the ui)\n"
        ));

        if (Core::getInstance().isWorldLoaded())
            plugin_onstatechange(out, SC_WORLD_LOADED);
    }

    return CR_OK;
}

DFhackCExport command_result plugin_onstatechange(color_ostream &out, state_change_event event)
{
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}

static command_result RenameUnit(color_ostream &stream, const RenameUnitIn *in)
{
    df::unit *unit = df::unit::find(in->unit_id());
    if (!unit)
        return CR_NOT_FOUND;

    if (in->has_nickname())
        Units::setNickname(unit, UTF2DF(in->nickname()));
    if (in->has_profession())
        unit->custom_profession = UTF2DF(in->profession());

    return CR_OK;
}

DFhackCExport RPCService *plugin_rpcconnect(color_ostream &)
{
    RPCService *svc = new RPCService();
    svc->addFunction("RenameUnit", RenameUnit);
    return svc;
}

static command_result rename(color_ostream &out, vector <string> &parameters)
{
    CoreSuspender suspend;

    string cmd;
    if (!parameters.empty())
        cmd = parameters[0];

    if (cmd == "unit")
    {
        if (parameters.size() != 2)
            return CR_WRONG_USAGE;

        df::unit *unit = Gui::getSelectedUnit(out, true);
        if (!unit)
            return CR_WRONG_USAGE;

        Units::setNickname(unit, parameters[1]);
    }
    else if (cmd == "unit-profession")
    {
        if (parameters.size() != 2)
            return CR_WRONG_USAGE;

        df::unit *unit = Gui::getSelectedUnit(out, true);
        if (!unit)
            return CR_WRONG_USAGE;

        unit->custom_profession = parameters[1];
    }
    else
    {
        if (!parameters.empty() && cmd != "?")
            out.printerr("Invalid command: %s\n", cmd.c_str());
        return CR_WRONG_USAGE;
    }

    return CR_OK;
}
