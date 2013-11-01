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
#include "df/world_geo_biome.h"
#include "df/world_geo_layer.h"
#include "df/feature_init.h"
#include "df/world_data.h"
#include "df/world_region_details.h"
#include "df/region_map_entry.h"
#include "df/flow_info.h"
#include "df/plant.h"
#include "df/building_type.h"

using namespace DFHack;
using namespace MapExtras;
using namespace df::enums;
using df::global::world;

extern bool GetFeature(t_feature &feature, df::coord2d rgn_pos, int32_t index);

const BiomeInfo MapCache::biome_stub = {
    df::coord2d(),
    -1, -1, -1, -1,
    NULL, NULL, NULL,
    { -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1 }
};

#define COPY(a,b) memcpy(&a,&b,sizeof(a))

MapExtras::Block::Block(MapCache *parent, DFCoord _bcoord) : parent(parent)
{
    dirty_designations = false;
    dirty_tiles = false;
    dirty_veins = false;
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

        dirty_tiles = false;

        if (block)
            ParseTiles(tiles);
    }

    if (basemat && !basemats)
    {
        basemats = new BasematInfo();

        dirty_veins = false;

        if (block)
            ParseBasemats(tiles, basemats);
    }
}

MapExtras::Block::TileInfo::TileInfo()
{
    dirty_raw.clear();
    memset(raw_tiles,0,sizeof(raw_tiles));
    ice_info = NULL;
    con_info = NULL;
    memset(base_tiles,0,sizeof(base_tiles));
}

MapExtras::Block::TileInfo::~TileInfo()
{
    delete ice_info;
    delete con_info;
}

void MapExtras::Block::TileInfo::init_iceinfo()
{
    if (ice_info)
        return;

    ice_info = new IceInfo();
}

void MapExtras::Block::TileInfo::init_coninfo()
{
    if (con_info)
        return;

    con_info = new ConInfo();
    con_info->constructed.clear();
    COPY(con_info->tiles, base_tiles);
    memset(con_info->mat_type, -1, sizeof(con_info->mat_type));
    memset(con_info->mat_subtype, -1, sizeof(con_info->mat_subtype));
}

MapExtras::Block::BasematInfo::BasematInfo()
{
    vein_dirty.clear();
    memset(mat_type,0,sizeof(mat_type));
    memset(mat_subtype,-1,sizeof(mat_subtype));
    memset(veinmat,-1,sizeof(veinmat));
}

bool MapExtras::Block::setFlagAt(df::coord2d p, df::tile_designation::Mask mask, bool set)
{
    if(!valid) return false;
    auto &val = index_tile<df::tile_designation&>(designation,p);
    bool cur = (val.whole & mask) != 0;
    if (cur != set)
    {
        dirty_designations = true;
        val.whole = (set ? val.whole | mask : val.whole & ~mask);
    }
    return true;
}

bool MapExtras::Block::setFlagAt(df::coord2d p, df::tile_occupancy::Mask mask, bool set)
{
    if(!valid) return false;
    auto &val = index_tile<df::tile_occupancy&>(occupancy,p);
    bool cur = (val.whole & mask) != 0;
    if (cur != set)
    {
        dirty_occupancies = true;
        val.whole = (set ? val.whole | mask : val.whole & ~mask);
    }
    return true;
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

bool MapExtras::Block::setVeinMaterialAt(df::coord2d pos, int16_t mat)
{
    using namespace df::enums::tiletype_material;

    if (!block)
        return false;

    if (!basemats)
        init_tiles(true);

    pos = pos & 15;
    auto &cur_mat = basemats->veinmat[pos.x][pos.y];

    if (cur_mat == mat && (mat < 0))
        return true;

    if (mat >= 0)
    {
        // Cannot allocate veins?
        if (!df::block_square_event_mineralst::_identity.can_instantiate())
            return false;

        // Bad material?
        if (!isStoneInorganic(mat))
            return false;
    }

    dirty_veins = true;
    cur_mat = mat;
    basemats->vein_dirty.setassignment(pos, true);

    if (tileMaterial(tiles->base_tiles[pos.x][pos.y]) == MINERAL)
        basemats->set_base_mat(tiles, pos, 0, mat);

    return true;
}

bool MapExtras::Block::setStoneAt(df::coord2d pos, df::tiletype tile, int16_t mat, bool force_vein, bool kill_veins)
{
    using namespace df::enums::tiletype_material;

    if (!block)
        return false;

    if (!isStoneInorganic(mat) || !isCoreMaterial(tile))
        return false;

    if (!basemats)
        init_tiles(true);

    // Check if anything needs to be done
    pos = pos & 15;
    auto &cur_tile = tiles->base_tiles[pos.x][pos.y];
    auto &cur_mattype = basemats->mat_type[pos.x][pos.y];
    auto &cur_matsubtype = basemats->mat_subtype[pos.x][pos.y];

    if (!force_vein && cur_tile == tile && cur_mattype == 0 && cur_matsubtype == mat)
        return true;

    bool vein = false;

    if (force_vein)
        vein = true;
    else if (mat == lavaStoneAt(pos))
        tile = matchTileMaterial(tile, LAVA_STONE);
    else if (mat == layerMaterialAt(pos))
        tile = matchTileMaterial(tile, STONE);
    else
        vein = true;

    if (vein)
        tile = matchTileMaterial(tile, MINERAL);

    if (tile == tiletype::Void)
        return false;
    if ((vein || kill_veins) && !setVeinMaterialAt(pos, vein ? mat : -1))
        return false;

    if (cur_tile != tile)
    {
        dirty_tiles = true;
        tiles->set_base_tile(pos, tile);
    }

    basemats->set_base_mat(tiles, pos, 0, mat);

    return true;
}

bool MapExtras::Block::setSoilAt(df::coord2d pos, df::tiletype tile, bool kill_veins)
{
    using namespace df::enums::tiletype_material;

    if (!block)
        return false;

    if (!isCoreMaterial(tile))
        return false;

    if (!basemats)
        init_tiles(true);

    pos = pos & 15;
    auto &cur_tile = tiles->base_tiles[pos.x][pos.y];

    tile = matchTileMaterial(tile, SOIL);
    if (tile == tiletype::Void)
        return false;

    if (kill_veins && !setVeinMaterialAt(pos, -1))
        return false;

    if (cur_tile != tile)
    {
        dirty_tiles = true;
        tiles->set_base_tile(pos, tile);
    }

    int mat = layerMaterialAt(pos);
    if (BlockInfo::getGroundType(mat) == BlockInfo::G_STONE)
        mat = biomeInfoAt(pos).default_soil;

    basemats->set_base_mat(tiles, pos, 0, mat);

    return true;
}

void MapExtras::Block::ParseTiles(TileInfo *tiles)
{
    tiletypes40d icetiles;
    BlockInfo::SquashFrozenLiquids(block, icetiles);

    COPY(tiles->raw_tiles, block->tiletype);

    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            using namespace df::enums::tiletype_material;

            df::tiletype tt = tiles->raw_tiles[x][y];
            df::coord coord = block->map_pos + df::coord(x,y,0);
            bool had_ice = false;

            // Frozen liquid comes topmost
            if (tileMaterial(tt) == FROZEN_LIQUID)
            {
                had_ice = true;
                tiles->init_iceinfo();

                tiles->ice_info->frozen.setassignment(x,y,true);
                if (icetiles[x][y] != tiletype::Void)
                {
                    tt = icetiles[x][y];
                }
            }

            // The next layer may be construction
            bool is_con = false;

            if (tileMaterial(tt) == CONSTRUCTION)
            {
                df::construction *con = df::construction::find(coord);
                if (con)
                {
                    if (!tiles->con_info)
                        tiles->init_coninfo();

                    is_con = true;
                    tiles->con_info->constructed.setassignment(x,y,true);
                    tiles->con_info->tiles[x][y] = tt;
                    tiles->con_info->mat_type[x][y] = con->material;
                    tiles->con_info->mat_subtype[x][y] = con->matgloss;

                    tt = con->original_tile;

                    // Ice under construction is buggy:
                    // http://www.bay12games.com/dwarves/mantisbt/view.php?id=6330
                    // Therefore we just pretend it wasn't there (if it isn't too late),
                    // and overwrite it if/when we write the base layer.
                    if (!had_ice && tileMaterial(tt) == FROZEN_LIQUID)
                    {
                        if (icetiles[x][y] != tiletype::Void)
                            tt = icetiles[x][y];
                    }
                }
            }

            // Finally, base material
            tiles->base_tiles[x][y] = tt;

            // Copy base info back to construction layer
            if (tiles->con_info && !is_con)
                tiles->con_info->tiles[x][y] = tt;
        }
    }
}

void MapExtras::Block::TileInfo::set_base_tile(df::coord2d pos, df::tiletype tile)
{
    base_tiles[pos.x][pos.y] = tile;

    if (con_info)
    {
        if (con_info->constructed.getassignment(pos))
        {
            con_info->dirty.setassignment(pos, true);
            return;
        }

        con_info->tiles[pos.x][pos.y] = tile;
    }

    if (ice_info && ice_info->frozen.getassignment(pos))
    {
        ice_info->dirty.setassignment(pos, true);
        return;
    }

    dirty_raw.setassignment(pos, true);
    raw_tiles[pos.x][pos.y] = tile;
}

void MapExtras::Block::WriteTiles(TileInfo *tiles)
{
    if (tiles->con_info)
    {
        for (int y = 0; y < 16; y++)
        {
            if (!tiles->con_info->dirty[y])
                continue;

            for (int x = 0; x < 16; x++)
            {
                if (!tiles->con_info->dirty.getassignment(x,y))
                    continue;

                df::coord coord = block->map_pos + df::coord(x,y,0);
                df::construction *con = df::construction::find(coord);
                if (con)
                    con->original_tile = tiles->base_tiles[x][y];
            }
        }

        tiles->con_info->dirty.clear();
    }

    if (tiles->ice_info && tiles->ice_info->dirty.has_assignments())
    {
        df::tiletype (*newtiles)[16] = (tiles->con_info ? tiles->con_info->tiles : tiles->base_tiles);

        for (int i = block->block_events.size()-1; i >= 0; i--)
        {
            auto event = block->block_events[i];
            auto ice = strict_virtual_cast<df::block_square_event_frozen_liquidst>(event);
            if (!ice)
                continue;

            for (int y = 0; y < 16; y++)
            {
                if (!tiles->ice_info->dirty[y])
                    continue;

                for (int x = 0; x < 16; x++)
                {
                    if (!tiles->ice_info->dirty.getassignment(x,y))
                        continue;
                    if (ice->tiles[x][y] == tiletype::Void)
                        continue;

                    ice->tiles[x][y] = newtiles[x][y];
                }
            }
        }

        tiles->ice_info->dirty.clear();
    }

    for (int y = 0; y < 16; y++)
    {
        if (!tiles->dirty_raw[y])
            continue;

        for (int x = 0; x < 16; x++)
        {
            if (tiles->dirty_raw.getassignment(x,y))
                block->tiletype[x][y] = tiles->raw_tiles[x][y];
        }
    }
}

void MapExtras::Block::ParseBasemats(TileInfo *tiles, BasematInfo *bmats)
{
    BlockInfo info;

    info.prepare(this);

    COPY(bmats->veinmat, info.veinmats);

    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            using namespace df::enums::tiletype_material;

            auto tt = tiles->base_tiles[x][y];
            auto mat = info.getBaseMaterial(tt, df::coord2d(x,y));

            bmats->set_base_mat(tiles, df::coord2d(x,y), mat.mat_type, mat.mat_subtype);
        }
    }
}

void MapExtras::Block::BasematInfo::set_base_mat(TileInfo *tiles, df::coord2d pos, int16_t type, int16_t subtype)
{
    mat_type[pos.x][pos.y] = type;
    mat_subtype[pos.x][pos.y] = subtype;

    // Copy base info back to construction layer
    if (tiles->con_info && !tiles->con_info->constructed.getassignment(pos))
    {
        tiles->con_info->mat_type[pos.x][pos.y] = type;
        tiles->con_info->mat_subtype[pos.x][pos.y] = subtype;
    }
}

void MapExtras::Block::WriteVeins(TileInfo *tiles, BasematInfo *bmats)
{
    // Classify modified tiles into distinct buckets
    typedef int t_vein_key;
    std::map<t_vein_key, df::tile_bitmask> added;
    std::set<t_vein_key> discovered;

    for (int y = 0; y < 16; y++)
    {
        if (!bmats->vein_dirty[y])
            continue;

        for (int x = 0; x < 16; x++)
        {
            using namespace df::enums::tiletype_material;

            if (!bmats->vein_dirty.getassignment(x,y))
                continue;

            int matgloss = bmats->veinmat[x][y];
            if (matgloss >= 0)
            {
                t_vein_key key(matgloss);

                added[key].setassignment(x,y,true);
                if (!designation[x][y].bits.hidden)
                    discovered.insert(key);
            }
        }
    }

    // Adjust existing veins
    for (int i = block->block_events.size()-1; i >= 0; i--)
    {
        auto event = block->block_events[i];
        auto vein = strict_virtual_cast<df::block_square_event_mineralst>(event);
        if (!vein)
            continue;

        // First clear all dirty tiles
        vein->tile_bitmask -= bmats->vein_dirty;

        // Then add new if there are any matching ones
        t_vein_key key(vein->stone);

        if (added.count(key))
        {
            vein->tile_bitmask |= added[key];
            if (discovered.count(key))
                vein->flags.bits.discovered = true;

            added.erase(key);
            discovered.erase(key);
        }

        // Delete if became empty
        if (!vein->tile_bitmask.has_assignments())
        {
            vector_erase_at(block->block_events, i);
            delete vein;
        }
    }

    // Finally add new vein objects if there are new unmatched
    for (auto it = added.begin(); it != added.end(); ++it)
    {
        auto vein = df::allocate<df::block_square_event_mineralst>();
        if (!vein)
            break;

        block->block_events.push_back(vein);

        vein->stone = it->first;
        vein->tile_bitmask = it->second;
        vein->flags.bits.discovered = discovered.count(it->first)>0;
    }

    bmats->vein_dirty.clear();
}

bool MapExtras::Block::isDirty()
{
    return valid && (
        dirty_designations ||
        dirty_tiles ||
        dirty_veins ||
        dirty_temperatures ||
        dirty_occupancies
    );
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
    if(dirty_tiles || dirty_veins)
    {
        if (tiles && dirty_tiles)
            WriteTiles(tiles);
        if (basemats && dirty_veins)
            WriteVeins(tiles, basemats);

        dirty_tiles = dirty_veins = false;

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

    SquashVeins(block,veinmats);

    for (size_t i = 0; i < block->plants.size(); i++)
    {
        auto pp = block->plants[i];
        plants[pp->pos] = pp;
    }

    feature = Maps::getInitFeature(block->region_pos, block->feature);
}

BlockInfo::GroundType MapExtras::BlockInfo::getGroundType(int material)
{
    auto raw = df::matgloss_stone::find(material);
    if (!raw)
        return G_UNKNOWN;

    if (raw->flags.is_set(matgloss_stone_flags::SOIL_ANY))
        return G_SOIL;

    return G_STONE;
}

t_matpair MapExtras::BlockInfo::getBaseMaterial(df::tiletype tt, df::coord2d pos)
{
    using namespace df::enums::tiletype_material;

    t_matpair rv(0,-1);
    int x = pos.x, y = pos.y;

    switch (tileMaterial(tt)) {
    case NONE:
    case AIR:
        rv.mat_type = -1;
        break;

    case DRIFTWOOD:
    case SOIL:
    {
        auto &biome = mblock->biomeInfoAt(pos);
        rv.mat_subtype = biome.layer_stone[mblock->layerIndexAt(pos)];

        if (getGroundType(rv.mat_subtype) == G_STONE)
        {
            int subtype = biome.default_soil;
            if (subtype >= 0)
                rv.mat_subtype = subtype;
        }

        break;
    }

    case STONE:
    {
        auto &biome = mblock->biomeInfoAt(pos);
        rv.mat_subtype = biome.layer_stone[mblock->layerIndexAt(pos)];

        if (getGroundType(rv.mat_subtype) == G_SOIL)
        {
            int subtype = biome.default_stone;
            if (subtype >= 0)
                rv.mat_subtype = subtype;
        }

        break;
    }

    case MINERAL:
        rv.mat_subtype = veinmats[x][y];
        break;

    case LAVA_STONE:
        rv.mat_subtype = mblock->biomeInfoAt(pos).lava_stone;
        break;

    case PLANT:
        if (auto plant = plants[block->map_pos + df::coord(x,y,0)])
        {
            if (plant->flags.bits.is_shrub)
                rv.mat_type = material_type::PLANT;
            else
                rv.mat_type = material_type::WOOD;
            rv.mat_subtype = plant->plant_id;
        }
        break;

    case GRASS_LIGHT:
    case GRASS_DARK:
    case GRASS_DRY:
    case GRASS_DEAD:
        rv.mat_type = -1;
        break;

    case FEATURE:
        rv.mat_type = -1;
        break;

    case CONSTRUCTION: // just a fallback
    case MAGMA:
    case HFS:
        // use generic 'rock'
        break;

    case FROZEN_LIQUID:
        rv.mat_type = material_type::WATER;
        break;

    case POOL:
    case BROOK:
    case RIVER:
        rv.mat_subtype = mblock->layerMaterialAt(pos);
        break;

    case ASHES:
    case FIRE:
    case CAMPFIRE:
        rv.mat_type = material_type::ASH;
        break;
    }

    return rv;
}

void MapExtras::BlockInfo::SquashVeins(df::map_block *mb, t_blockmaterials & materials)
{
    std::vector <df::block_square_event_mineralst *> veins;
    Maps::SortBlockEvents(mb,&veins);
    memset(materials,-1,sizeof(materials));

    for (uint32_t x = 0;x<16;x++) for (uint32_t y = 0; y< 16;y++)
    {
        for (size_t i = 0; i < veins.size(); i++)
        {
            if (veins[i]->getassignment(x,y))
            {
                materials[x][y] = veins[i]->stone;
            }
        }
    }
}

void MapExtras::BlockInfo::SquashFrozenLiquids(df::map_block *mb, tiletypes40d & frozen)
{
    std::vector <df::block_square_event_frozen_liquidst *> ices;
    Maps::SortBlockEvents(mb,NULL,&ices);
    memset(frozen,0,sizeof(frozen));
    for (uint32_t x = 0; x < 16; x++) for (uint32_t y = 0; y < 16; y++)
    {
        for (size_t i = 0; i < ices.size(); i++)
        {
            df::tiletype tt2 = ices[i]->tiles[x][y];
            if (tt2 != tiletype::Void)
            {
                frozen[x][y] = tt2;
                break;
            }
        }
    }
}

void MapExtras::BlockInfo::SquashRocks (df::map_block *mb, t_blockmaterials & materials,
                                   std::vector< std::vector <int16_t> > * layerassign)
{
    // get the layer materials
    for (uint32_t x = 0; x < 16; x++) for (uint32_t y = 0; y < 16; y++)
    {
        materials[x][y] = -1;
        uint8_t test = mb->designation[x][y].bits.biome;
        if (test >= 9)
            continue;
        uint8_t idx = mb->region_offset[test];
        if (idx < layerassign->size())
            materials[x][y] = layerassign->at(idx)[mb->designation[x][y].bits.geolayer_index];
    }
}

int MapExtras::Block::biomeIndexAt(df::coord2d p)
{
    if (!block)
        return -1;

    auto des = index_tile<df::tile_designation>(designation,p);
    uint8_t idx = des.bits.biome;
    if (idx >= 9)
        return -1;
    idx = block->region_offset[idx];
    if (idx >= parent->biomes.size())
        return -1;
    return idx;
}

const BiomeInfo &Block::biomeInfoAt(df::coord2d p)
{
    return parent->getBiomeByIndex(biomeIndexAt(p));
}

df::coord2d MapExtras::Block::biomeRegionAt(df::coord2d p)
{
    if (!block)
        return df::coord2d(-30000,-30000);

    int idx = biomeIndexAt(p);
    if (idx < 0)
        return block->region_pos;

    return parent->biomes[idx].pos;
}

bool MapExtras::Block::GetFeature(t_feature *out)
{
    out->type = (df::feature_type)-1;
    if (!valid || block->feature < 0)
        return false;
    return ::GetFeature(*out, block->region_pos, block->feature);
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
        int &count = index_tile<int&>(item_counts,item->pos);

        if (count++ == 0)
        {
            index_tile<df::tile_occupancy&>(occupancy,item->pos).bits.item = true;
            index_tile<df::tile_occupancy&>(block->occupancy,item->pos).bits.item = true;
        }
    }

    return inserted;
}

bool MapExtras::Block::removeItemOnGround(df::item *item)
{
    if (!block)
        return false;

    init_item_counts();

    int idx = binsearch_index(block->items, item->id);
    if (idx < 0)
        return false;

    vector_erase_at(block->items, idx);

    int &count = index_tile<int&>(item_counts,item->pos);

    if (--count == 0)
    {
        index_tile<df::tile_occupancy&>(occupancy,item->pos).bits.item = false;

        auto &occ = index_tile<df::tile_occupancy&>(block->occupancy,item->pos);

        occ.bits.item = false;

        // Clear the 'site blocked' flag in the building, if any.
        // Otherwise the job would be re-suspended without actually checking items.
        if (occ.bits.building == tile_building_occ::Planned)
        {
            if (auto bld = Buildings::findAtTile(item->pos))
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
    valid = 0;
    Maps::getSize(x_bmax, y_bmax, z_max);
    x_tmax = x_bmax*16; y_tmax = y_bmax*16;
    std::vector<df::coord2d> geoidx;
    std::vector<std::vector<int16_t> > layer_mats;
    validgeo = Maps::ReadGeology(&layer_mats, &geoidx);
    valid = true;

    if (auto data = &df::global::world->world_data)
    {
        for (size_t i = 0; i < data->region_details.size(); i++)
        {
            auto info = data->region_details[i];
            region_details[info->pos] = info;
        }
    }

    biomes.resize(layer_mats.size());

    for (size_t i = 0; i < layer_mats.size(); i++)
    {
        biomes[i].pos = geoidx[i];
        biomes[i].biome = Maps::getRegionBiome(geoidx[i]);
        biomes[i].details = region_details[geoidx[i]];

        biomes[i].geo_index = biomes[i].biome ? biomes[i].biome->geo_index : -1;
        biomes[i].geobiome = df::world_geo_biome::find(biomes[i].geo_index);

        biomes[i].lava_stone = -1;
        biomes[i].default_soil = -1;
        biomes[i].default_stone = -1;

        if (biomes[i].details)
            biomes[i].lava_stone = biomes[i].details->lava_stone;

        memset(biomes[i].layer_stone, -1, sizeof(biomes[i].layer_stone));

        for (size_t j = 0; j < std::min(BiomeInfo::MAX_LAYERS,layer_mats[i].size()); j++)
        {
            biomes[i].layer_stone[j] = layer_mats[i][j];

            auto raw = df::matgloss_stone::find(layer_mats[i][j]);
            if (!raw)
                continue;

            bool is_soil = raw->flags.is_set(matgloss_stone_flags::SOIL_ANY);
            if (is_soil)
                biomes[i].default_soil = layer_mats[i][j];
            else if (biomes[i].default_stone == -1)
                biomes[i].default_stone = layer_mats[i][j];
        }

        while (layer_mats[i].size() < 16)
            layer_mats[i].push_back(-1);
    }
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

void MapExtras::MapCache::discardBlock(Block *block)
{
    blocks.erase(block->bcoord);
    delete block;
}

void MapExtras::MapCache::resetTags()
{
    for (auto it = blocks.begin(); it != blocks.end(); ++it)
    {
        delete[] it->second->tags;
        it->second->tags = NULL;
    }
}
