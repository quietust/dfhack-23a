// Make 23a more 3-dimensional

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"

#include "DataDefs.h"
#include "TileTypes.h"
#include "modules/Maps.h"
#include "modules/Gui.h"
#include "df/world.h"
#include "df/ui.h"
#include "df/map_block.h"
#include "df/matgloss_stone.h"
#include "df/matgloss_gem.h"

using std::string;
using std::vector;
using std::set;
using namespace DFHack;
using namespace df::enums;

using df::global::cursor;
using df::global::world;
using df::global::ui;

command_result df_addzlevel (color_ostream &out, vector <string> & parameters)
{
    CoreSuspender suspend;

    if (!Maps::IsValid())
    {
        out.printerr("Map is not available!\n");
        return CR_FAILURE;
    }
    if (world->surface_z != 0)
    {
        out.printerr("Can only be used on player fortresses!\n");
        return CR_FAILURE;
    }

    // add Z-level
    world->map.z_count_block++;
    world->map.z_count++;

    // rebuild block lookup
    for (size_t x = 0; x < world->map.x_count_block; x++)
    {
        for (size_t y = 0; y < world->map.y_count_block; y++)
        {
            size_t z = world->map.z_count_block - 1;
            delete[] world->map.block_index[x][y];
            world->map.block_index[x][y] = new df::map_block *[z];
        }
    }

    // create new map blocks
    for (size_t x = 0; x < world->map.x_count; x += 16)
    {
        for (size_t y = 0; y < world->map.y_count; y += 16)
        {
            size_t z = world->map.z_count - 1;
            df::map_block *blk = new df::map_block;
            blk->flags.resize(1);
            blk->map_pos.x = x;
            blk->map_pos.y = y;
            blk->map_pos.z = z;
            world->map.map_blocks.push_back(blk);
        }
    }

    // repopulate block lookup
    for (size_t i = 0; i < world->map.map_blocks.size(); i++)
    {
        df::map_block *blk = world->map.map_blocks[i];
        world->map.block_index[blk->map_pos.x >> 4][blk->map_pos.y >> 4][blk->map_pos.z] = blk;
    }

    for (size_t x = 0; x < world->map.x_count_block; x++)
    {
        for (size_t y = 0; y < world->map.y_count_block; y++)
        {
            size_t z = world->map.z_count - 1;
            df::map_block *cur = world->map.block_index[x][y][z];
            df::map_block *base = world->map.block_index[x][y][0];
            for (size_t tx = 0; tx < 16; tx++)
            {
                for (size_t ty = 0; ty < 16; ty++)
                {
                    size_t _x = (x << 4 | tx);
                    if (_x < ui->min_dig_depth)
                    {
                        cur->chr[tx][ty] = tile_chr::OpenSpace;
                        cur->designation[tx][ty].bits.light = 1;
                        cur->designation[tx][ty].bits.outside = 1;
                    }
                    else
                    {
                        cur->chr[tx][ty] = tile_chr::Wall;
                        cur->color[tx][ty].bits.color = 7;
                        cur->designation[tx][ty].bits.subterranean = 1;
                        if (_x > ui->min_dig_depth)
                            cur->designation[tx][ty].bits.hidden = 1;
                        // open space above rivers and chasms
                        if (base->chr[tx][ty] == tile_chr::River || base->chr[tx][ty] == tile_chr::Waterfall || base->chr[tx][ty] == tile_chr::Chasm)
                        {
                            cur->chr[tx][ty] = tile_chr::OpenSpace;
                            cur->color[tx][ty].bits.color = 0;
                        }
                        // extend any unmined adamantine upwards
                        if (base->chr[tx][ty] == tile_chr::Ore && base->color[tx][ty].bits.color == 11)
                        {
                            cur->chr[tx][ty] = tile_chr::Ore;
                            cur->color[tx][ty].bits.color = 11;
                        }
                    }
                    cur->temperature_1[tx][ty] = base->temperature_1[tx][ty];
                    cur->temperature_2[tx][ty] = base->temperature_2[tx][ty];
                }
            }
        }
    }
    world->reindex_pathfinding = true;
    out.print("Successfully added new Z-level.\n");
    return CR_OK;
}

command_result df_addstair (color_ostream &out, vector <string> & parameters)
{
    CoreSuspender suspend;

    if (!Maps::IsValid())
    {
        out.printerr("Map is not available!\n");
        return CR_FAILURE;
    }
    if (world->surface_z != 0)
    {
        out.printerr("Can only be used on player fortresses!\n");
        return CR_FAILURE;
    }

    int32_t cx, cy, cz;
    uint32_t x_max,y_max,z_max;
    Maps::getSize(x_max,y_max,z_max);

    Gui::getCursorCoords(cx,cy,cz);
    if (cx < 0)
    {
        out.printerr("Cursor is not active. Point the cursor where you want to place the staircase.\n");
        return CR_FAILURE;
    }

    if (cz == z_max - 1)
    {
        out.printerr("Cannot place stairs at the topmost Z-level!\n");
        return CR_FAILURE;
    }
    if (cx == 0 || cx == x_max - 1 || cy == 0 || cy == y_max - 1)
    {
        out.printerr("Cannot place stairs at edge of the map!\n");
        return CR_FAILURE;
    }
    df::tiletype_shape shape = tileShape(Maps::getTileType(cx, cy, cz));
    if (shape != tiletype_shape::FLOOR && shape != tiletype_shape::STAIR_DOWN)
    {
        out.printerr("Stairs can only be placed on floors or downward stairs!\n");
        return CR_FAILURE;
    }
    if (Maps::getTileOccupancy(cx, cy, cz)->bits.building)
    {
        out.printerr("There is a building in the way!\n");
        return CR_FAILURE;
    }

    // add upward stair on current Z-level
    if (*Maps::getTileChr(cx, cy, cz) == tile_chr::StairU)
        *Maps::getTileChr(cx, cy, cz) = tile_chr::StairUD;
    else
        *Maps::getTileChr(cx, cy, cz) = tile_chr::StairU;

    // add downward stair on Z-level above
    *Maps::getTileChr(cx, cy, cz + 1) = tile_chr::StairD;
    Maps::getTileColor(cx, cy, cz + 1)->color = Maps::getTileColor(cx, cy, cz + 1)->color;

    // reveal it
    Maps::getTileDesignation(cx, cy, cz + 1)->bits.hidden = 0;

    // and reveal the surrounding tiles
    Maps::getTileDesignation(cx-1, cy, cz + 1)->bits.hidden = 0;
    Maps::getTileDesignation(cx+1, cy, cz + 1)->bits.hidden = 0;
    Maps::getTileDesignation(cx, cy-1, cz + 1)->bits.hidden = 0;
    Maps::getTileDesignation(cx, cy+1, cz + 1)->bits.hidden = 0;

    world->reindex_pathfinding = true;
    out.print("Successfully added upward stairs.\n");
    return CR_OK;
}

command_result df_addramp (color_ostream &out, vector <string> & parameters)
{
    CoreSuspender suspend;

    if (!Maps::IsValid())
    {
        out.printerr("Map is not available!\n");
        return CR_FAILURE;
    }
    if (world->surface_z != 0)
    {
        out.printerr("Can only be used on player fortresses!\n");
        return CR_FAILURE;
    }

    int32_t cx, cy, cz;
    uint32_t x_max,y_max,z_max;
    Maps::getSize(x_max,y_max,z_max);

    Gui::getCursorCoords(cx,cy,cz);
    if (cx < 0)
    {
        out.printerr("Cursor is not active. Point the cursor where you want to place the ramp.\n");
        return CR_FAILURE;
    }

    if (cz == z_max - 1)
    {
        out.printerr("Cannot place ramp at the topmost Z-level!\n");
        return CR_FAILURE;
    }
    if (cx == 0 || cx == x_max - 1 || cy == 0 || cy == y_max - 1)
    {
        out.printerr("Cannot place ramp at edge of the map!\n");
        return CR_FAILURE;
    }
    df::tiletype_shape shape = tileShape(Maps::getTileType(cx, cy, cz));
    if (shape != tiletype_shape::FLOOR)
    {
        out.printerr("Ramps can only be placed on floors!\n");
        return CR_FAILURE;
    }
    if (Maps::getTileOccupancy(cx, cy, cz)->bits.building)
    {
        out.printerr("There is a building in the way!\n");
        return CR_FAILURE;
    }

    // add upward ramp on current Z-level
    *Maps::getTileChr(cx, cy, cz) = tile_chr::Ramp;

    // add downward ramp on Z-level above
    *Maps::getTileChr(cx, cy, cz + 1) = tile_chr::RampTop;
    Maps::getTileColor(cx, cy, cz + 1)->color = Maps::getTileColor(cx, cy, cz + 1)->color;

    // reveal it
    Maps::getTileDesignation(cx, cy, cz + 1)->bits.hidden = 0;

    // and reveal the surrounding tiles
    Maps::getTileDesignation(cx-1, cy, cz + 1)->bits.hidden = 0;
    Maps::getTileDesignation(cx+1, cy, cz + 1)->bits.hidden = 0;
    Maps::getTileDesignation(cx, cy-1, cz + 1)->bits.hidden = 0;
    Maps::getTileDesignation(cx, cy+1, cz + 1)->bits.hidden = 0;

    world->reindex_pathfinding = true;
    out.print("Successfully added ramp.\n");
    return CR_OK;
}

DFHACK_PLUGIN("df3d");

DFhackCExport command_result plugin_init ( color_ostream &out, vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "addzlevel", "Add a new Z-level to the fortress",
        df_addzlevel, false,
        "  Adds a new Z-level to your fortress. Best used immediately after embark.\n"
    ));
    commands.push_back(PluginCommand(
        "addstair", "Insert upward stairs",
        df_addstair, false,
        "  Adds a staircase up to the Z-level above you.\n"
    ));
    commands.push_back(PluginCommand(
        "addramp", "Insert ramp",
        df_addramp, false,
        "  Adds a ramp up to the Z-level above you.\n"
    ));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}
