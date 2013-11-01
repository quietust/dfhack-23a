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
#include "modules/Materials.h"

#include "DataDefs.h"
#include "df/world_data.h"
#include "df/world_data.h"
#include "df/region_map_entry.h"
#include "df/flow_info.h"
#include "df/mineral_cluster.h"
#include "df/plant.h"
#include "df/building_type.h"

using namespace DFHack;
using namespace MapExtras;
using namespace df::enums;
using df::global::world;

#define COPY(a,b) memcpy(&a,&b,sizeof(a))

MapExtras::Block::Block(MapCache *parent, DFCoord _bcoord) : parent(parent)
{
    dirty_designations = false;
    dirty_tiles = false;
    dirty_temperatures = false;
    dirty_occupancies = false;
    valid = false;
    bcoord = _bcoord;
    block = Maps::getBlock(bcoord);
    tags = NULL;

    init();
}

void MapExtras::Block::init()
{
    item_counts = NULL;
    tiles = NULL;
    basemats = NULL;

    if(block)
    {
        COPY(designation, block->designation);
        COPY(occupancy, block->occupancy);

        COPY(temp1, block->temperature_1);
        COPY(temp2, block->temperature_2);

        valid = true;
    }
    else
    {
        memset(designation,0,sizeof(designation));
        memset(occupancy,0,sizeof(occupancy));
        memset(temp1,0,sizeof(temp1));
        memset(temp2,0,sizeof(temp2));
    }
}

bool MapExtras::Block::Allocate()
{
    if (block)
        return true;

    block = Maps::ensureTileBlock(bcoord.x*16, bcoord.y*16, bcoord.z);
    if (!block)
        return false;

    delete[] item_counts;
    delete tiles;
    delete basemats;
    init();

    return true;
}

MapExtras::Block::~Block()
{
    delete[] item_counts;
    delete[] tags;
    delete tiles;
    delete basemats;
}

void MapExtras::Block::init_tags()
{
    if (!tags)
        tags = new T_tags[16];
    memset(tags,0,sizeof(T_tags)*16);
}

void MapExtras::Block::init_tiles(bool basemat)
{
    if (!tiles)
    {
        tiles = new TileInfo();

        if (block)
            ParseTiles(tiles);
    }

    if (basemat && !basemats)
    {
        basemats = new BasematInfo();

        if (block)
            ParseBasemats(tiles, basemats);
    }
}

MapExtras::Block::TileInfo::TileInfo()
{
    frozen.clear();
    dirty_raw.clear();
    memset(raw_tiles,0,sizeof(raw_tiles));
    dirty_base.clear();
    memset(base_tiles,0,sizeof(base_tiles));
}

MapExtras::Block::TileInfo::~TileInfo()
{
}

MapExtras::Block::BasematInfo::BasematInfo()
{
    dirty.clear();
    memset(mat_type,0,sizeof(mat_type));
    memset(mat_subtype,-1,sizeof(mat_subtype));
    memset(layermat,-1,sizeof(layermat));
}

bool MapExtras::Block::setTiletypeAt(df::coord2d pos, df::tiletype tt, bool force)
{
    if (!block)
        return false;

    if (!basemats)
        init_tiles(true);

    pos = pos & 15;

    dirty_tiles = true;
    tiles->raw_tiles[pos.x][pos.y] = tt;
    tiles->dirty_raw.setassignment(pos, true);

    return true;
}

void MapExtras::Block::ParseTiles(TileInfo *tiles)
{
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            tiles->base_tiles[x][y] = tiles->raw_tiles[x][y] = convertTile(block->chr[x][y], block->color[x][y]);
        }
    }
}

void MapExtras::Block::ParseBasemats(TileInfo *tiles, BasematInfo *bmats)
{
    BlockInfo info;

    info.prepare(this);

    COPY(bmats->layermat, info.basemats);

    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            using namespace df::enums::tiletype_material;

            auto tt = tiles->base_tiles[x][y];
            auto mat = info.getBaseMaterial(tt, df::coord2d(x,y));
            
            bmats->mat_type[x][y] = mat.mat_type;
            bmats->mat_subtype[x][y] = mat.mat_subtype;
        }
    }
}

bool MapExtras::Block::Write ()
{
    if(!valid) return false;

    if(dirty_designations)
    {
        COPY(block->designation, designation);
        block->flags.set(block_flags::designated);
        dirty_designations = false;
    }
    if(dirty_tiles && tiles)
    {
        dirty_tiles = false;

        for (int x = 0; x < 16; x++)
        {
            for (int y = 0; y < 16; y++)
            {
                if (tiles->dirty_raw.getassignment(x,y))
                    convertTile(tiles->raw_tiles[x][y], block->chr[x][y], block->color[x][y]);
            }
        }

        delete tiles; tiles = NULL;
        delete basemats; basemats = NULL;
    }
    if(dirty_temperatures)
    {
        COPY(block->temperature_1, temp1);
        COPY(block->temperature_2, temp2);
        dirty_temperatures = false;
    }
    if(dirty_occupancies)
    {
        COPY(block->occupancy, occupancy);
        dirty_occupancies = false;
    }
    return true;
}

void MapExtras::BlockInfo::prepare(Block *mblock)
{
    this->mblock = mblock;

    block = mblock->getRaw();
    parent = mblock->getParent();

    SquashVeins(block, material, matgloss);
    SquashRocks(block, basemats);

    for (size_t i = 0; i < world->plants.all.size(); i++)
    {
        auto pp = world->plants.all[i];
        if ((pp->pos.x & 0xFFF0) == block->map_pos.x && (pp->pos.y & 0xFFF0) == block->map_pos.y && pp->pos.z == block->map_pos.z)
            plants[pp->pos] = pp;
    }
}

t_matpair MapExtras::BlockInfo::getBaseMaterial(df::tiletype tt, df::coord2d pos)
{
    using namespace df::enums::tiletype_material;

    t_matpair rv(material_type::STONE_GRAY,-1);
    int x = pos.x, y = pos.y;

    switch (tileMaterial(tt)) {
    case NONE:
    case AIR:
        rv.mat_type = -1;
        break;

    case DIRT:
    case MUD:
        rv.mat_type = material_type::MUD;
        break;

    case SAND:
        rv.mat_type = material_type::SAND;
        break;

    case STONE:
    case WOOD:  // fake material for Human buildings in advmode
        rv.mat_subtype = basemats[x][y];
        break;

    case STONE_LIGHT:
    case STONE_DARK:
    case ORE:
    case GEM:
        rv.mat_type = material[x][y];
        rv.mat_subtype = matgloss[x][y];
        break;

    case PLANT:
        if (auto plant = plants[block->map_pos + df::coord(x,y,0)])
        {
            if (plant->flags >= plant_flags::shrub_forest)
            {
                rv.mat_type = material_type::PLANT;
                rv.mat_subtype = plant->plant_id;
            }
            else
            {
                rv.mat_type = material_type::WOOD;
                rv.mat_subtype = plant->wood_id;
            }
        }
        break;

    case GRASS_LIGHT:
    case GRASS_DARK:
        rv.mat_type = -1;
        break;

    case MAGMA:
    case HFS:
        // use generic 'rock'
        break;

    case POOL:
    case RIVER:
        rv.mat_type = material_type::WATER;
        break;

    case ASHES:
    case FIRE:
    case CAMPFIRE:
        rv.mat_type = material_type::ASH;
        break;
    }

    return rv;
}

void MapExtras::BlockInfo::SquashVeins(df::map_block *mb,
    t_blockmaterials & material, t_blockmaterials & matgloss)
{
    memset(material,-1,sizeof(material));
    memset(matgloss,-1,sizeof(matgloss));
    for (uint32_t x = 0;x<16;x++) for (uint32_t y = 0; y< 16;y++)
    {
        auto tt = convertTile(mb->chr[x][y],mb->color[x][y]);
        auto mat = tileMaterial(tt);
        switch (mat)
        {
        case tiletype_material::ORE:
            switch (tileSpecial(tt))
            {
            case tiletype_special::ZINC:
                material[x][y] = material_type::ZINC;
                break;
            case tiletype_special::IRON:
                material[x][y] = material_type::IRON;
                break;
            case tiletype_special::TIN:
                material[x][y] = material_type::TIN;
                break;
            case tiletype_special::SILVER:
                material[x][y] = material_type::SILVER;
                break;
            case tiletype_special::ADAMANTINE:
                material[x][y] = material_type::ADAMANTINE;
                break;
            case tiletype_special::GOLD:
                material[x][y] = material_type::GOLD;
                break;
            case tiletype_special::PLATINUM:
                material[x][y] = material_type::PLATINUM;
                break;
            case tiletype_special::COAL:
                material[x][y] = material_type::COAL;
                break;
            }
            break;
        case tiletype_material::GEM:
            for (int i = 0; i < world->gem_clusters.size(); i++)
            {
                auto gem = world->gem_clusters[i];
                int _x = x + mb->map_pos.x;
                int _y = y + mb->map_pos.y;
                int _z = mb->map_pos.z;
                if (gem->x1 >= _x && gem->x2 <= _x && gem->y1 >= _y && gem->y2 <= _y && gem->z == _z && gem->color == (mb->color[x][y].bits.color & 0x7))
                {
                    material[x][y] = gem->material;
                    matgloss[x][y] = gem->matgloss;
                }
            }
            break;
        case tiletype_material::STONE_DARK:
        case tiletype_material::STONE_LIGHT:
            if (mat == tiletype_material::STONE_DARK)
            {
                material[x][y] = material_type::STONE_DARK;
                matgloss[x][y] = world->lava_stone;
            }
            else
            {
                material[x][y] = material_type::STONE_LIGHT;
                matgloss[x][y] = world->river_stone;
            }
            for (int i = 0; i < world->stone_clusters.size(); i++)
            {
                auto stone = world->stone_clusters[i];
                int _x = x + mb->map_pos.x;
                int _y = y + mb->map_pos.y;
                int _z = mb->map_pos.z;
                if (stone->x1 >= _x && stone->x2 <= _x && stone->y1 >= _y && stone->y2 <= _y && stone->z == _z && stone->color == (mb->color[x][y].bits.color & 0x7))
                {
                    material[x][y] = stone->material;
                    matgloss[x][y] = stone->matgloss;
                }
            }
            break;
        }
    }
}

void MapExtras::BlockInfo::SquashRocks(df::map_block *mb,
    t_blockmaterials & materials)
{
    memset(materials,-1,sizeof(materials));
    for (uint32_t x = 0;x<16;x++) for (uint32_t y = 0; y< 16;y++)
    {
        materials[x][y] = world->map.stone_types[mb->designation[x][y].bits.geolayer_index];
    }
}

void MapExtras::Block::init_item_counts()
{
    if (item_counts) return;

    item_counts = new T_item_counts[16];
    memset(item_counts, 0, sizeof(T_item_counts)*16);

    if (!block) return;

    for (size_t i = 0; i < block->items.size(); i++)
    {
        auto it = df::item::find(block->items[i]);
        if (!it || !it->flags.bits.on_ground)
            continue;

        df::coord tidx = it->pos - block->map_pos;
        if (!is_valid_tile_coord(tidx) || tidx.z != 0)
            continue;

        item_counts[tidx.x][tidx.y]++;
    }
}

bool MapExtras::Block::addItemOnGround(df::item *item)
{
    if (!block)
        return false;

    init_item_counts();

    bool inserted;
    insert_into_vector(block->items, item->id, &inserted);

    if (inserted)
    {
        df::coord pos(item->pos.x, item->pos.y, item->pos.z);
        int &count = index_tile<int&>(item_counts,pos);

        if (count++ == 0)
        {
            index_tile<df::tile_occupancy&>(occupancy,pos).bits.item = true;
            index_tile<df::tile_occupancy&>(block->occupancy,pos).bits.item = true;
        }
    }

    return inserted;
}

bool MapExtras::Block::removeItemOnGround(df::item *item)
{
    if (!block)
        return false;

    init_item_counts();

    int idx = /*binsearch*/linear_index(block->items, item->id);
    if (idx < 0)
        return false;

    vector_erase_at(block->items, idx);

    df::coord pos(item->pos.x, item->pos.y, item->pos.z);
    int &count = index_tile<int&>(item_counts,pos);

    if (--count == 0)
    {
        index_tile<df::tile_occupancy&>(occupancy,pos).bits.item = false;

        auto &occ = index_tile<df::tile_occupancy&>(block->occupancy,pos);

        occ.bits.item = false;

        // Clear the 'site blocked' flag in the building, if any.
        // Otherwise the job would be re-suspended without actually checking items.
        if (occ.bits.building == tile_building_occ::Planned)
        {
            if (auto bld = Buildings::findAtTile(pos))
            {
                // TODO: maybe recheck other tiles like the game does.
                bld->flags.bits.site_blocked = false;
            }
        }
    }

    return true;
}

MapExtras::MapCache::MapCache()
{
    Maps::getSize(x_bmax, y_bmax, z_max);
    x_tmax = x_bmax*16; y_tmax = y_bmax*16;
    valid = true;
}

MapExtras::Block *MapExtras::MapCache::BlockAt(DFCoord blockcoord)
{
    if(!valid)
        return 0;
    std::map <DFCoord, Block*>::iterator iter = blocks.find(blockcoord);
    if(iter != blocks.end())
    {
        return (*iter).second;
    }
    else
    {
        if(unsigned(blockcoord.x) < x_bmax &&
           unsigned(blockcoord.y) < y_bmax &&
           unsigned(blockcoord.z) < z_max)
        {
            Block * nblo = new Block(this, blockcoord);
            blocks[blockcoord] = nblo;
            return nblo;
        }
        return 0;
    }
}

void MapExtras::MapCache::resetTags()
{
    for (auto it = blocks.begin(); it != blocks.end(); ++it)
    {
        delete[] it->second->tags;
        it->second->tags = NULL;
    }
}
