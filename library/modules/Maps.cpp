/*
https://github.com/peterix/dfhack
Copyright (c) 2009-2012 Petr Mrázek (peterix@gmail.com)

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/


#include "Internal.h"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdlib>
#include <iostream>
using namespace std;

#include "modules/Maps.h"
#include "modules/MapCache.h"
#include "ColorText.h"
#include "Error.h"
#include "VersionInfo.h"
#include "MemAccess.h"
#include "ModuleFactory.h"
#include "Core.h"
#include "MiscUtils.h"

#include "modules/Buildings.h"

#include "DataDefs.h"
#include "df/world_data.h"
#include "df/world_data.h"
#include "df/region_map_entry.h"
#include "df/flow_info.h"
#include "df/matgloss_stone.h"
#include "df/matgloss_gem.h"
#include "df/mineral_cluster.h"
#include "df/plant.h"
#include "df/building_type.h"
#include "df/plant.h"

using namespace DFHack;
using namespace df::enums;
using df::global::world;

bool Maps::IsValid ()
{
    return (world->map.block_index != NULL);
}

// getter for map size
void Maps::getSize (uint32_t& x, uint32_t& y, uint32_t& z)
{
    if (!IsValid())
    {
        x = y = z = 0;
        return;
    }
    x = world->map.x_count_block;
    y = world->map.y_count_block;
    z = world->map.z_count_block;
}

/*
 * Block reading
 */

df::map_block *Maps::getBlock (int32_t blockx, int32_t blocky, int32_t blockz)
{
    if (!IsValid())
        return NULL;
    if ((blockx < 0) || (blocky < 0) || (blockz < 0))
        return NULL;
    if ((blockx >= world->map.x_count_block) || (blocky >= world->map.y_count_block) || (blockz >= world->map.z_count_block))
        return NULL;
    return world->map.block_index[blockx][blocky][blockz];
}

bool Maps::isValidTilePos(int32_t x, int32_t y, int32_t z)
{
    if (!IsValid())
        return false;
    if ((x < 0) || (y < 0) || (z < 0))
        return false;
    if ((x >= world->map.x_count) || (y >= world->map.y_count) || (z >= world->map.z_count))
        return false;
    return true;
}

df::map_block *Maps::getTileBlock (int32_t x, int32_t y, int32_t z)
{
    if (!isValidTilePos(x,y,z))
        return NULL;
    return world->map.block_index[x >> 4][y >> 4][z];
}

df::map_block *Maps::ensureTileBlock (int32_t x, int32_t y, int32_t z)
{
    if (!isValidTilePos(x,y,z))
        return NULL;

    auto column = world->map.block_index[x >> 4][y >> 4];
    auto &slot = column[z];
    if (slot)
        return slot;

    // Find another block below
    int z2 = z;
    while (z2 >= 0 && !column[z2]) z2--;
    if (z2 < 0)
        return NULL;

    slot = new df::map_block();
    slot->map_pos = column[z2]->map_pos;
    slot->map_pos.z = z;

    // Assume sky
    df::tile_designation dsgn(0);
    dsgn.bits.light = true;
    dsgn.bits.outside = true;

    for (int tx = 0; tx < 16; tx++)
        for (int ty = 0; ty < 16; ty++) {
            slot->designation[tx][ty] = dsgn;
            slot->temperature_1[tx][ty] = column[z2]->temperature_1[tx][ty];
            slot->temperature_2[tx][ty] = column[z2]->temperature_2[tx][ty];
        }
    
    df::global::world->map.map_blocks.push_back(slot);
    return slot;
}

df::tile_chr *Maps::getTileChr(int32_t x, int32_t y, int32_t z)
{
    df::map_block *block = getTileBlock(x,y,z);
    return block ? &block->chr[x&15][y&15] : NULL;
}

df::tile_color *Maps::getTileColor(int32_t x, int32_t y, int32_t z)
{
    df::map_block *block = getTileBlock(x,y,z);
    return block ? &block->color[x&15][y&15] : NULL;
}

df::tiletype Maps::getTileType(int32_t x, int32_t y, int32_t z)
{
    df::map_block *block = getTileBlock(x,y,z);
    return block ? convertTile(block->chr[x&15][y&15],block->color[x&15][y&15]) : tiletype::Void;
}

df::tile_designation *Maps::getTileDesignation(int32_t x, int32_t y, int32_t z)
{
    df::map_block *block = getTileBlock(x,y,z);
    return block ? &block->designation[x&15][y&15] : NULL;
}

df::tile_occupancy *Maps::getTileOccupancy(int32_t x, int32_t y, int32_t z)
{
    df::map_block *block = getTileBlock(x,y,z);
    return block ? &block->occupancy[x&15][y&15] : NULL;
}

df::region_map_entry *Maps::getRegionBiome(df::coord2d rgn_pos)
{
    auto data = &world->world_data;
    if (!data)
        return NULL;

    if (rgn_pos.x < 0 || rgn_pos.x >= data->world_width ||
        rgn_pos.y < 0 || rgn_pos.y >= data->world_height)
        return NULL;

    return &data->region_map[rgn_pos.x][rgn_pos.y];
}

void Maps::enableBlockUpdates(df::map_block *blk, bool flow, bool temperature)
{
    if (!blk || !(flow || temperature)) return;

    if (temperature)
        blk->flags.set(block_flags::update_temperature);
}

df::flow_info *Maps::spawnFlow(df::coord pos, df::flow_type type, int mat_type, int mat_subtype, int density)
{
    using df::global::flows;

    auto block = getTileBlock(pos);
    if (!flows || !block)
        return NULL;

    auto flow = new df::flow_info();
    flow->type = type;
    flow->material = (df::material_type)mat_type;
    flow->matgloss = mat_subtype;
    flow->density = std::min(100, density);
    flow->pos = pos;

    block->flows.push_back(flow);
    flows->push_back(flow);
    return flow;
}

bool Maps::canWalkBetween(df::coord pos1, df::coord pos2)
{
    auto block1 = getTileBlock(pos1);
    auto block2 = getTileBlock(pos2);

    if (!block1 || !block2)
        return false;

    auto tile1 = index_tile<uint16_t>(block1->walkable, pos1);
    auto tile2 = index_tile<uint16_t>(block2->walkable, pos2);

    return tile1 && tile1 == tile2;
}

bool Maps::canStepBetween(df::coord pos1, df::coord pos2)
{
    color_ostream& out = Core::getInstance().getConsole();
    int32_t dx = pos2.x-pos1.x;
    int32_t dy = pos2.y-pos1.y;
    int32_t dz = pos2.z-pos1.z;

    if ( dx*dx > 1 || dy*dy > 1 || dz*dz > 1 )
        return false;

    // cannot move diagonally
    if ( dx*dx + dy*dy > 1)
        return false;

    if ( pos2.z < pos1.z ) {
        df::coord temp = pos1;
        pos1 = pos2;
        pos2 = temp;
    }

    df::map_block* block1 = getTileBlock(pos1);
    df::map_block* block2 = getTileBlock(pos2);

    if ( !block1 || !block2 )
        return false;

    if ( !index_tile<uint16_t>(block1->walkable,pos1) || !index_tile<uint16_t>(block2->walkable,pos2) ) {
        return false;
    }
    
    if ( block1->occupancy[pos1.x&0xF][pos1.y&0xF].bits.water ||
         block2->occupancy[pos2.x&0xF][pos2.y&0xF].bits.water )
        return false;

    if ( block1->occupancy[pos1.x&0xF][pos1.y&0xF].bits.lava ||
         block2->occupancy[pos2.x&0xF][pos2.y&0xF].bits.lava )
        return false;

    if ( dz == 0 )
        return true;

    df::tiletype type1 = Maps::getTileType(pos1);
    df::tiletype type2 = Maps::getTileType(pos2);

    df::tiletype_shape shape1 = ENUM_ATTR(tiletype,shape,type1);
    df::tiletype_shape shape2 = ENUM_ATTR(tiletype,shape,type2);

    if ( dx == 0 && dy == 0 ) {
        //check for forbidden hatches and floors and such
        df::enums::tile_building_occ::tile_building_occ upOcc = index_tile<df::tile_occupancy>(block2->occupancy,pos2).bits.building;
        if ( upOcc == df::enums::tile_building_occ::Impassable)
            return false;

        if ( shape1 == tiletype_shape::STAIR_UPDOWN && shape2 == shape1 )
            return true;
        if ( shape1 == tiletype_shape::STAIR_UPDOWN && shape2 == tiletype_shape::STAIR_DOWN )
            return true;
        if ( shape1 == tiletype_shape::STAIR_UP && shape2 == tiletype_shape::STAIR_UPDOWN )
            return true;
        if ( shape1 == tiletype_shape::STAIR_UP && shape2 == tiletype_shape::STAIR_DOWN )
            return true;
        // ramps in 23a are simple - they don't need walls next to them
        if ( shape1 == tiletype_shape::RAMP && shape2 == tiletype_shape::RAMP_TOP )
            return true;
        return false;
    }
    
    //diagonal up: has to be a ramp
    if ( shape1 == tiletype_shape::RAMP /*&& shape2 == tiletype_shape::RAMP*/ ) {
        return true;
    }

    return false;
}
