#include <stdint.h>
#include <iostream>
#include <map>
#include <vector>
#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "modules/Maps.h"
#include "modules/World.h"
#include "modules/MapCache.h"
#include "modules/Gui.h"
using MapExtras::MapCache;

using std::string;
using std::vector;

using namespace DFHack;
using namespace df::enums;

using df::global::world;

struct hideblock
{
    df::coord c;
    uint8_t hiddens [16][16];
};

// the saved data. we keep map size to check if things still match
uint32_t x_max, y_max, z_max;
vector <hideblock> hidesaved;
bool nopause_state = false;

enum revealstate
{
    NOT_REVEALED,
    REVEALED
};

revealstate revealed = NOT_REVEALED;

command_result reveal(color_ostream &out, vector<string> & params);
command_result unreveal(color_ostream &out, vector<string> & params);
command_result revtoggle(color_ostream &out, vector<string> & params);
command_result revflood(color_ostream &out, vector<string> & params);
command_result revforget(color_ostream &out, vector<string> & params);
command_result nopause(color_ostream &out, vector<string> & params);

DFHACK_PLUGIN("reveal");

DFhackCExport command_result plugin_init ( color_ostream &out, vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand("reveal","Reveal the map.",reveal));
    commands.push_back(PluginCommand("unreveal","Revert the map to its previous state.",unreveal));
    commands.push_back(PluginCommand("revtoggle","Reveal/unreveal depending on state.",revtoggle));
    commands.push_back(PluginCommand("revflood","Hide all, reveal all tiles reachable from cursor position.",revflood));
    commands.push_back(PluginCommand("revforget", "Forget the current reveal data, allowing to use reveal again.",revforget));
    commands.push_back(PluginCommand("nopause","Disable pausing (doesn't affect pause forced by reveal).",nopause));
    return CR_OK;
}

DFhackCExport command_result plugin_onupdate ( color_ostream &out )
{
    t_gamemodes gm;
    World::ReadGameMode(gm);
    if(gm.g_mode == game_mode::DWARF)
    {
        // if the map is revealed and we're in fortress mode, force the game to pause.
        if(revealed == REVEALED)
        {
            World::SetPauseState(true);
        }
        else if(nopause_state)
        {
            World::SetPauseState(false);
        }
    }
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}

command_result nopause (color_ostream &out, vector <string> & parameters)
{
    if (parameters.size() == 1 && (parameters[0] == "0" || parameters[0] == "1"))
    {
        if (parameters[0] == "0")
            nopause_state = 0;
        else
            nopause_state = 1;
        out.print("nopause %sactivated.\n", (nopause_state ? "" : "de"));
    }
    else
    {
        out.print("Disable pausing (doesn't affect pause forced by reveal).\nActivate with 'nopause 1', deactivate with 'nopause 0'.\nCurrent state: %d.\n", nopause_state);
    }

    return CR_OK;
}

void revealAdventure(color_ostream &out)
{
    for (size_t i = 0; i < world->map.map_blocks.size(); i++)
    {
        df::map_block *block = world->map.map_blocks[i];
        designations40d & designations = block->designation;
        // for each tile in block
        for (uint32_t x = 0; x < 16; x++) for (uint32_t y = 0; y < 16; y++)
        {
            // set to revealed
            designations[x][y].bits.hidden = 0;
            // and visible
            designations[x][y].bits.pile = 1;
        }
    }
    out.print("Local map revealed.\n");
}

command_result reveal(color_ostream &out, vector<string> & params)
{
    bool pause = true;
    for(size_t i = 0; i < params.size();i++)
    {
        if(params[i] == "help" || params[i] == "?")
        {
            out.print("Reveals the map.\n");
            return CR_OK;
        }
    }
    auto & con = out;
    if(revealed != NOT_REVEALED)
    {
        con.printerr("Map is already revealed or this is a different map.\n");
        return CR_FAILURE;
    }

    CoreSuspender suspend;

    if (!Maps::IsValid())
    {
        out.printerr("Map is not available!\n");
        return CR_FAILURE;
    }
    t_gamemodes gm;
    World::ReadGameMode(gm);
    if(gm.g_mode == game_mode::ADVENTURE)
    {
        revealAdventure(out);
        return CR_OK;
    }
    if(gm.g_mode != game_mode::DWARF)
    {
        con.printerr("Only in fortress mode.\n");
        return CR_FAILURE;
    }

    Maps::getSize(x_max,y_max,z_max);
    hidesaved.reserve(x_max * y_max * z_max);
    for (size_t i = 0; i < world->map.map_blocks.size(); i++)
    {
        df::map_block *block = world->map.map_blocks[i];
        hideblock hb;
        hb.c = block->map_pos;
        designations40d & designations = block->designation;
        // for each tile in block
        for (uint32_t x = 0; x < 16; x++) for (uint32_t y = 0; y < 16; y++)
        {
            // save hidden state of tile
            hb.hiddens[x][y] = designations[x][y].bits.hidden;
            // set to revealed
            designations[x][y].bits.hidden = 0;
        }
        hidesaved.push_back(hb);
    }
    revealed = REVEALED;
    con.print("Map revealed.\n");
    con.print("Run 'unreveal' to revert to previous state.\n");
    return CR_OK;
}

command_result unreveal(color_ostream &out, vector<string> & params)
{
    auto & con = out;
    for(size_t i = 0; i < params.size();i++)
    {
        if(params[i] == "help" || params[i] == "?")
        {
            out.print("Reverts the previous reveal operation, hiding the map again.\n");
            return CR_OK;
        }
    }
    if(!revealed)
    {
        con.printerr("There's nothing to revert!\n");
        return CR_FAILURE;
    }
    CoreSuspender suspend;

    if (!Maps::IsValid())
    {
        out.printerr("Map is not available!\n");
        return CR_FAILURE;
    }
    t_gamemodes gm;
    World::ReadGameMode(gm);
    if(gm.g_mode != game_mode::DWARF)
    {
        con.printerr("Only in fortress mode.\n");
        return CR_FAILURE;
    }

    // Sanity check: map size
    uint32_t x_max_b, y_max_b, z_max_b;
    Maps::getSize(x_max_b,y_max_b,z_max_b);
    if(x_max != x_max_b || y_max != y_max_b || z_max != z_max_b)
    {
        con.printerr("The map is not of the same size...\n");
        return CR_FAILURE;
    }

    for(size_t i = 0; i < hidesaved.size();i++)
    {
        hideblock & hb = hidesaved[i];
        df::map_block * b = Maps::getTileBlock(hb.c.x,hb.c.y,hb.c.z);
        for (uint32_t x = 0; x < 16;x++) for (uint32_t y = 0; y < 16;y++)
        {
            b->designation[x][y].bits.hidden = hb.hiddens[x][y];
        }
    }
    // give back memory.
    hidesaved.clear();
    revealed = NOT_REVEALED;
    con.print("Map hidden!\n");
    return CR_OK;
}

command_result revtoggle (color_ostream &out, vector<string> & params)
{
    for(size_t i = 0; i < params.size();i++)
    {
        if(params[i] == "help" || params[i] == "?")
        {
            out.print("Toggles between reveal and unreveal.\nCurrently it: ");
            break;
        }
    }
    if(revealed)
    {
        return unreveal(out,params);
    }
    else
    {
        return reveal(out,params);
    }
}

command_result revflood(color_ostream &out, vector<string> & params)
{
    for(size_t i = 0; i < params.size();i++)
    {
        if(params[i] == "help" || params[i] == "?")
        {
            out.print("This command hides the whole map. Then, starting from the cursor,\n"
                         "reveals all accessible tiles. Allows repairing parma-revealed maps.\n"
            );
            return CR_OK;
        }
    }
    CoreSuspender suspend;
    uint32_t x_max,y_max,z_max;
    if (!Maps::IsValid())
    {
        out.printerr("Map is not available!\n");
        return CR_FAILURE;
    }
    if(revealed != NOT_REVEALED)
    {
        out.printerr("This is only safe to use with non-revealed map.\n");
        return CR_FAILURE;
    }
    t_gamemodes gm;
    World::ReadGameMode(gm);
    if(gm.g_type != game_type::DWARF_MAIN && gm.g_mode != game_mode::DWARF )
    {
        out.printerr("Only in proper dwarf mode.\n");
        return CR_FAILURE;
    }
    int32_t cx, cy, cz;
    Maps::getSize(x_max,y_max,z_max);
    uint32_t tx_max = x_max * 16;
    uint32_t ty_max = y_max * 16;

    Gui::getCursorCoords(cx,cy,cz);
    if(cx == -30000)
    {
        out.printerr("Cursor is not active. Point the cursor at some empty space you want to be unhidden.\n");
        return CR_FAILURE;
    }
    DFCoord xy ((uint32_t)cx,(uint32_t)cy,cz);
    MapCache * MCache = new MapCache;
    df::tiletype tt = MCache->tiletypeAt(xy);
    if(isWallTerrain(tt))
    {
        out.printerr("Point the cursor at some empty space you want to be unhidden.\n");
        delete MCache;
        return CR_FAILURE;
    }
    // hide all tiles, flush cache
    Maps::getSize(x_max,y_max,z_max);

    for(size_t i = 0; i < world->map.map_blocks.size(); i++)
    {
        df::map_block * b = world->map.map_blocks[i];
        // change the hidden flag to 0
        for (uint32_t x = 0; x < 16; x++) for (uint32_t y = 0; y < 16; y++)
        {
            b->designation[x][y].bits.hidden = 1;
        }
    }
    MCache->trash();

    typedef std::pair <DFCoord, bool> foo;
    std::stack < foo > flood;
    flood.push( foo(xy,false) );

    while( !flood.empty() )
    {
        foo tile = flood.top();
        DFCoord & current = tile.first;
        bool & from_below = tile.second;
        flood.pop();

        if(!MCache->testCoord(current))
            continue;
        df::tiletype tt = MCache->baseTiletypeAt(current);
        df::tile_designation des = MCache->designationAt(current);
        if(!des.bits.hidden)
        {
            continue;
        }
        bool below = 0;
        bool above = 0;
        bool sides = 0;
        bool unhide = 1;
        // by tile shape, determine behavior and action
        switch (tileShape(tt))
        {
        // walls:
        case tiletype_shape::WALL:
            if(from_below)
                unhide = 0;
            break;
        // air/free space
        case tiletype_shape::EMPTY:
        case tiletype_shape::RAMP_TOP:
        case tiletype_shape::STAIR_UPDOWN:
        case tiletype_shape::STAIR_DOWN:
            above = below = sides = true;
            break;
        // has floor
        case tiletype_shape::FORTIFICATION:
        case tiletype_shape::STAIR_UP:
        case tiletype_shape::RAMP:
        case tiletype_shape::FLOOR:
        case tiletype_shape::TREE:
        case tiletype_shape::SAPLING:
        case tiletype_shape::SHRUB:
        case tiletype_shape::ENDLESS_PIT:
        case tiletype_shape::LIQUID:
        case tiletype_shape::CHANNEL:
            if(from_below)
                unhide = 0;
            above = sides = true;
            break;
        }
        if(unhide)
        {
            des.bits.hidden = false;
            MCache->setDesignationAt(current,des);
        }
        if(sides)
        {
            flood.push(foo(DFCoord(current.x + 1, current.y ,current.z),0));
            flood.push(foo(DFCoord(current.x, current.y + 1 ,current.z),0));
            flood.push(foo(DFCoord(current.x - 1, current.y ,current.z),0));
            flood.push(foo(DFCoord(current.x, current.y - 1 ,current.z),0));
        }
        if(above)
        {
            flood.push(foo(DFCoord(current.x, current.y ,current.z + 1),1));
        }
        if(below)
        {
            flood.push(foo(DFCoord(current.x, current.y ,current.z - 1),0));
        }
    }
    MCache->WriteAll();
    delete MCache;
    return CR_OK;
}

command_result revforget(color_ostream &out, vector<string> & params)
{
    auto & con = out;
    for(size_t i = 0; i < params.size();i++)
    {
        if(params[i] == "help" || params[i] == "?")
        {
            out.print("Forget the current reveal data, allowing to use reveal again.\n");
            return CR_OK;
        }
    }
    if(!revealed)
    {
        con.printerr("There's nothing to forget!\n");
        return CR_FAILURE;
    }
    // give back memory.
    hidesaved.clear();
    revealed = NOT_REVEALED;
    con.print("Reveal data forgotten!\n");
    return CR_OK;
}
