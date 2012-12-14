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

/*******************************************************************************
                                    M A P S
                            Read and write DF's map
*******************************************************************************/
#pragma once
#ifndef CL_MOD_MAPS
#define CL_MOD_MAPS

#include "Export.h"
#include "Module.h"
#include <vector>
#include "BitArray.h"
#include "modules/Materials.h"

#include "df/world.h"
#include "df/world_data.h"
#include "df/map_block.h"
#include "df/flow_type.h"
#include "df/tiletype.h"

/**
 * \defgroup grp_maps Maps module and its types
 * @ingroup grp_modules
 */

namespace df
{
    struct world_data;
}

namespace DFHack
{
/***************************************************************************
                                T Y P E S
***************************************************************************/
/**
    * Function for translating feature index to its name
    * \ingroup grp_maps
    */
typedef df::coord DFCoord;
typedef DFCoord planecoord;

/**
 * map block flags wrapper
 * \ingroup grp_maps
 */
typedef df::block_flags t_blockflags;

/**
 * 16x16 array of tile types
 * \ingroup grp_maps
 */
typedef df::tiletype tiletypes40d [16][16];
/**
 * 16x16 array used for squashed block materials
 * \ingroup grp_maps
 */
typedef int16_t t_blockmaterials [16][16];
/**
 * 16x16 array of designation flags
 * \ingroup grp_maps
 */
typedef df::tile_designation t_designation;
typedef t_designation designations40d [16][16];
/**
 * 16x16 array of occupancy flags
 * \ingroup grp_maps
 */
typedef df::tile_occupancy t_occupancy;
typedef t_occupancy occupancies40d [16][16];
/**
 * 16x16 array of temperatures
 * \ingroup grp_maps
 */
typedef uint16_t t_temperatures [16][16];

/**
 * Index a tile array by a 2D coordinate, clipping it to mod 16
 */
template<class R, class T> inline R index_tile(T &v, df::coord2d p) {
    return v[p.x&15][p.y&15];
}

/**
 * Check if a 2D coordinate is in the 0-15 range.
 */
inline bool is_valid_tile_coord(df::coord2d p) {
    return (p.x & ~15) == 0 && (p.y & ~15) == 0;
}


/**
 * The Maps module
 * \ingroup grp_modules
 * \ingroup grp_maps
 */
namespace Maps
{

extern DFHACK_EXPORT bool IsValid();

/*
 * BLOCK DATA
 */

/// get size of the map in tiles
extern DFHACK_EXPORT void getSize(uint32_t& x, uint32_t& y, uint32_t& z);

extern DFHACK_EXPORT bool isValidTilePos(int32_t x, int32_t y, int32_t z);
inline bool isValidTilePos(df::coord pos) { return isValidTilePos(pos.x, pos.y, pos.z); }

/**
 * Get the map block or NULL if block is not valid
 */
extern DFHACK_EXPORT df::map_block * getBlock (int32_t blockx, int32_t blocky, int32_t blockz);
extern DFHACK_EXPORT df::map_block * getTileBlock (int32_t x, int32_t y, int32_t z);
extern DFHACK_EXPORT df::map_block * ensureTileBlock (int32_t x, int32_t y, int32_t z);

inline df::map_block * getBlock (df::coord pos) { return getBlock(pos.x, pos.y, pos.z); }
inline df::map_block * getTileBlock (df::coord pos) { return getTileBlock(pos.x, pos.y, pos.z); }
inline df::map_block * ensureTileBlock (df::coord pos) { return ensureTileBlock(pos.x, pos.y, pos.z); }

extern DFHACK_EXPORT df::tile_chr *getTileChr(int32_t x, int32_t y, int32_t z);
extern DFHACK_EXPORT df::tile_color *getTileColor(int32_t x, int32_t y, int32_t z);
extern DFHACK_EXPORT df::tiletype getTileType(int32_t x, int32_t y, int32_t z);
extern DFHACK_EXPORT df::tile_designation *getTileDesignation(int32_t x, int32_t y, int32_t z);
extern DFHACK_EXPORT df::tile_occupancy *getTileOccupancy(int32_t x, int32_t y, int32_t z);

inline df::tile_chr *getTileChr(df::coord pos) {
    return getTileChr(pos.x, pos.y, pos.z);
}
inline df::tile_color *getTileColor(df::coord pos) {
    return getTileColor(pos.x, pos.y, pos.z);
}
inline df::tiletype getTileType(df::coord pos) {
    return getTileType(pos.x, pos.y, pos.z);
}
inline df::tile_designation *getTileDesignation(df::coord pos) {
    return getTileDesignation(pos.x, pos.y, pos.z);
}
inline df::tile_occupancy *getTileOccupancy(df::coord pos) {
    return getTileOccupancy(pos.x, pos.y, pos.z);
}

/**
 * Returns biome info about the specified world region.
 */
DFHACK_EXPORT df::region_map_entry *getRegionBiome(df::coord2d rgn_pos);

// Enables per-frame updates for liquid flow and/or temperature.
DFHACK_EXPORT void enableBlockUpdates(df::map_block *blk, bool flow = false, bool temperature = false);

DFHACK_EXPORT df::flow_info *spawnFlow(df::coord pos, df::flow_type type, int mat_type = 0, int mat_index = -1, int density = 100);

DFHACK_EXPORT bool canWalkBetween(df::coord pos1, df::coord pos2);
}
}
#endif
