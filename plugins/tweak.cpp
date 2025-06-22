// A container for random minor tweaks that don't fit anywhere else,
// in order to avoid creating lots of plugins and polluting the namespace.

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"

#include "modules/Materials.h"

#include "MiscUtils.h"

#include "DataDefs.h"
#include <VTableInterpose.h>
#include "df/item_actual.h"
#include "df/building_drawbuffer.h"
#include "df/building_bridgest.h"

#include <stdlib.h>

using std::vector;
using std::string;
using namespace DFHack;
using namespace df::enums;

static command_result tweak(color_ostream &out, vector <string> & parameters);

DFHACK_PLUGIN("tweak");

DFhackCExport command_result plugin_init (color_ostream &out, std::vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "tweak", "Various tweaks for minor bugs.", tweak, false,
        "  tweak stable-temp [disable]\n"
        "    Fixes performance bug 6012 by squashing jitter in temperature updates.\n"
        "  tweak drawbridge-tiles [disable]\n"
        "    Draws raised bridges with distinct tiles.\n"
    ));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown (color_ostream &out)
{
    return CR_OK;
}


struct stable_temp_hook : df::item_actual {
    typedef df::item_actual interpose_base;

    DEFINE_VMETHOD_INTERPOSE(bool, adjustTemperature, (uint16_t temp))
    {
        if (temperature.whole != temp)
        {
            // Bug 6012 is caused by fixed-point precision mismatch jitter
            // when an item is being pushed by two sources at N and N+1.
            // This check suppresses it altogether.
            if (temp == temperature.whole+1 ||
                (temp == temperature.whole-1 && temperature.fraction == 0))
                temp = temperature.whole;
            // When SPEC_HEAT is NONE, the original function seems to not
            // change the temperature, yet return true, which is silly.
            else if (getSpecHeat() == 60001)
                temp = temperature.whole;
        }

        return INTERPOSE_NEXT(adjustTemperature)(temp);
    }
};

IMPLEMENT_VMETHOD_INTERPOSE(stable_temp_hook, adjustTemperature);

struct drawbridge_tiles_hook : df::building_bridgest {
    typedef df::building_bridgest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, drawBuilding, (df::building_drawbuffer *buf))
    {
        static const unsigned char tiles[4][3] =
        {
            { 0xB7, 0xB6, 0xBD },
            { 0xD6, 0xC7, 0xD3 },
            { 0xD4, 0xCF, 0xBE },
            { 0xD5, 0xD1, 0xB8 },
        };
        INTERPOSE_NEXT(drawBuilding)(buf);

        // Only redraw "closed" drawbridges
        if (!gate_flags.bits.closed || direction == -1)
            return;

        // Figure out the extents and the axis
        int p1, p2;
        bool iy = false;
        switch (direction)
        {
        case 0: // Left
        case 1: // Right
            p1 = buf->y1; p2 = buf->y2; iy = true;
            break;
        case 2: // Up
        case 3: // Down
            p1 = buf->x1; p2 = buf->x2;
            break;
        }

        int x = 0, y = 0;
        if (p1 == p2)
            buf->tile[0][0] = tiles[direction][1];
        else for (int p = p1; p <= p2; p++)
        {
            if (p == p1)
                buf->tile[x][y] = tiles[direction][0];
            else if (p == p2)
                buf->tile[x][y] = tiles[direction][2];
            else
                buf->tile[x][y] = tiles[direction][1];
            if (iy)
                y++;
            else
                x++;
        }
    }
};
IMPLEMENT_VMETHOD_INTERPOSE(drawbridge_tiles_hook, drawBuilding);

static void enable_hook(color_ostream &out, VMethodInterposeLinkBase &hook, vector <string> &parameters)
{
    if (vector_get(parameters, 1) == "disable")
    {
        hook.remove();
        out.print("Disabled tweak %s (%s)\n", parameters[0].c_str(), hook.name());
    }
    else
    {
        if (hook.apply())
            out.print("Enabled tweak %s (%s)\n", parameters[0].c_str(), hook.name());
        else
            out.printerr("Could not activate tweak %s (%s)\n", parameters[0].c_str(), hook.name());
    }
}

static command_result tweak(color_ostream &out, vector <string> &parameters)
{
    CoreSuspender suspend;

    if (parameters.empty())
        return CR_WRONG_USAGE;

    string cmd = parameters[0];

    if (cmd == "stable-temp")
    {
        enable_hook(out, INTERPOSE_HOOK(stable_temp_hook, adjustTemperature), parameters);
    }
    else if (cmd == "drawbridge-tiles")
    {
        enable_hook(out, INTERPOSE_HOOK(drawbridge_tiles_hook, drawBuilding), parameters);
    }
    else
        return CR_WRONG_USAGE;

    return CR_OK;
}
