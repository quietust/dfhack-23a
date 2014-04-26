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
#include "df/mineral_cluster.h"
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

int random (int max)
{
    return rand() % max;
}
df::tile_chr getTileChr (int x, int y, int z)
{
    df::tile_chr *tmp = Maps::getTileChr(x, y, z);
    if (tmp)
        return *tmp;
    else
        return tile_chr::Void;
}
void setTileChr (int x, int y, int z, df::tile_chr chr)
{
    df::tile_chr *tmp = Maps::getTileChr(x, y, z);
    if (tmp)
        *tmp = chr;
}
int getTileColor (int x, int y, int z)
{
    df::tile_color *tmp = Maps::getTileColor(x, y, z);
    if (tmp)
        return tmp->bits.color;
    else
        return 0;
}
void setTileColor (int x, int y, int z, int color)
{
    df::tile_color *tmp = Maps::getTileColor(x, y, z);
    if (tmp)
        tmp->bits.color = color;
}
vector<df::coord2d> generateLine (int x0, int y0, int x1, int y1)
{
    vector<df::coord2d> result;

    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1)
    {
        df::coord2d point(x0, y0);
        result.push_back(point);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = err * 2;
        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
    return result;
}



command_result addzlevel (color_ostream &out, vector <string> & parameters)
{
    size_t z = world->map.z_count;

    // create new map blocks
    for (size_t x = 0; x < world->map.x_count; x += 16)
    {
        for (size_t y = 0; y < world->map.y_count; y += 16)
        {
            df::map_block *blk = new df::map_block;
            blk->flags.resize(1);
            blk->map_pos.x = x;
            blk->map_pos.y = y;
            blk->map_pos.z = z;
            world->map.map_blocks.push_back(blk);
        }
    }

    // increment Z-level counts
    world->map.z_count_block++;
    world->map.z_count++;

    // rebuild and repopulate block lookup
    // DFHack Maps module relies on this, and we're going to use it below
    for (size_t x = 0; x < world->map.x_count_block; x++)
    {
        for (size_t y = 0; y < world->map.y_count_block; y++)
        {
            delete[] world->map.block_index[x][y];
            world->map.block_index[x][y] = new df::map_block *[world->map.z_count_block];
        }
    }
    for (size_t i = 0; i < world->map.map_blocks.size(); i++)
    {
        df::map_block *blk = world->map.map_blocks[i];
        world->map.block_index[blk->map_pos.x >> 4][blk->map_pos.y >> 4][blk->map_pos.z] = blk;
    }

    // initialize map data - open space outside, unmined stone inside
    for (size_t x = 0; x < world->map.x_count_block; x++)
    {
        for (size_t y = 0; y < world->map.y_count_block; y++)
        {
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
                        cur->designation[tx][ty].bits.geolayer_index = base->designation[tx][ty].bits.geolayer_index;
                    }
                    else
                    {
                        cur->chr[tx][ty] = tile_chr::Wall;
                        cur->color[tx][ty].bits.color = 7;
                        cur->designation[tx][ty].bits.subterranean = 1;
                        if (_x > ui->min_dig_depth)
                            cur->designation[tx][ty].bits.hidden = 1;
                    }
                    cur->temperature_1[tx][ty] = base->temperature_1[tx][ty];
                    cur->temperature_2[tx][ty] = base->temperature_2[tx][ty];
                }
            }
        }
    }

    // initialize stone layers
    int slope = random(41) - 20;
    {
        int layer_bounds[480];
        bool skip_adjust = true;
        for (int x = -197; x + 2 < world->map.x_count + 300; x += 3)
        {
            int x_base = x - 1;
            int x_base_min = x - 3;
            int x_base_max = x;
            int x_end = x + 2;
            int geo_index = random(16);
            for (int y = 0; y < world->map.y_count; y++)
            {
                if (!skip_adjust)
                {
                    if (x_base < layer_bounds[y] - 1)
                        x_base = layer_bounds[y] - 1;
                    if (x_base_min < layer_bounds[y] - 1)
                        x_base_min = layer_bounds[y] - 1;
                    if (x_base_max < layer_bounds[y] + 2)
                        x_base_max = layer_bounds[y] + 2;
                    if (x_end < layer_bounds[y] + 7)
                        x_end = layer_bounds[y] + 7;
                }
                for (int _x = x_base; _x <= x_end; _x++)
                {
                    if (_x < ui->min_dig_depth || _x >= world->map.x_count)
                        continue;
                    Maps::getTileDesignation(_x, y, z)->bits.geolayer_index = geo_index;
                }
                layer_bounds[y] = x_base;

                if ((slope > 0) && (random(30) < slope))
                {
                    x_base_min++;
                    x_base_max++;
                    x_base++;
                    x_end++;
                }
                else if ((slope < 0) && (-random(30) > slope))
                {
                    x_base_min--;
                    x_base_max--;
                    x_base--;
                    x_end--;
                }
                else
                    x_base += random(3) - 1;

                if (x_base < x_base_min)
                    x_base = x_base_min;
                if (x_base > x_base_max)
                    x_base = x_base_max;
            }
            skip_adjust = false;
        }
    }

    // insert light/dark stone clusters
    {
        vector<int16_t> stone_dark;
        vector<int16_t> stone_light;
        for (int i = 0; i < world->raws.matgloss.stone.size(); i++)
        {
            auto stone = world->raws.matgloss.stone[i];
            if (stone->flags.bits.LIGHT)
                stone_light.push_back(i);
            if (stone->flags.bits.DARK)
                stone_dark.push_back(i);
        }

        int num_clusters = ((world->map.x_count - ui->min_dig_depth) * world->map.y_count) * 1000 / 182400;
        for (int i = 0; i < num_clusters; i++)
        {
            int x = random(world->map.x_count - 108) + ui->min_dig_depth + 2;
            int y = random(world->map.y_count - 4) + 2;
            if (x >= world->map.x_count)
                continue;
            bool is_dark = (i % 2);
            char color = is_dark ? 8 : 15;

            setTileChr(x, y, z, tile_chr::Wall);
            setTileColor(x, y, z, color);

            setTileChr(x - 1, y, z, tile_chr::Wall);
            setTileColor(x - 1, y, z, color);

            setTileChr(x + 1, y, z, tile_chr::Wall);
            setTileColor(x + 1, y, z, color);

            setTileChr(x, y - 1, z, tile_chr::Wall);
            setTileColor(x, y - 1, z, color);

            setTileChr(x, y + 1, z, tile_chr::Wall);
            setTileColor(x, y + 1, z, color);

            for (int _y = -4; _y <= 4; _y++)
            {
                int _x = 2;
                if (slope >= 10)
                {
                    if (_y >= 4)
                        _x = 4;
                    else if (_y >= 2)
                        _x = 3;
                    else if (_y >= -1)
                        _x = 2;
                    else if (_y >= -3)
                        _x = 1;
                    else
                        _x = 0;
                }
                if (slope <= -10)
                {
                    if (_y >= 4)
                        _x = 0;
                    else if (_y >= 2)
                        _x = 1;
                    else if (_y >= -1)
                        _x = 2;
                    else if (_y >= -3)
                        _x = 3;
                    else
                        _x = 4;
                }

                _x -= 3;
                for (int j = 0; j < 3; j++)
                {
                    // prevent slanted light/dark stone clusters from sticking out of the cliff face
                    if (_x + x < ui->min_dig_depth + 1)
                        continue;
                    if (!random(2))
                    {
                        setTileChr(_x + x, _y + y, z, tile_chr::Wall);
                        setTileColor(_x + x, _y + y, z, color);
                    }
                    _x++;
                }
            }
            auto cluster = new df::mineral_cluster();
            world->stone_clusters.push_back(cluster);
            cluster->z = z;
            cluster->x1 = x - 3;
            cluster->x2 = x + 3;
            cluster->y1 = y - 4;
            cluster->y2 = y + 4;
            cluster->color = getTileColor(x, y, z) & 7;

            if (is_dark)
            {
                cluster->material = material_type::STONE_DARK;
                cluster->matgloss = stone_dark[random(stone_dark.size())];
            }
            else
            {
                cluster->material = material_type::STONE_LIGHT;
                cluster->matgloss = stone_light[random(stone_light.size())];
            }
        }
    }

    // insert ore/coal veins
    {
        for (int i = 0; i < 100; i++)
        {
            df::tile_chr tile = tile_chr::Ore;
            int x1 = ui->min_dig_depth + i * 3 + 1;
            int y1 = random(world->map.y_count - 2) + 1;

            int x2 = random(10) + x1 + 1;
            if (x2 > world->map.x_count - 3)
                x2 = world->map.x_count - 3;

            int y2 = y1 + random(21) - 10;
            if (y2 < 1)
                y2 = random(10) + 1;
            if (y2 > world->map.y_count - 2)
                y2 = world->map.y_count - 2 - random(10);

            char color;
            switch (i % 8)
            {
            case 0:
                color = 8; // silver
                break;
            case 1:
                if (x1 >= 270)
                    color = 14; // gold
                else
                    color = 8; // silver
                break;
            case 2:
                color = 2; // copper
                break;
            case 3:
                if (x1 >= 270)
                    color = 4; // iron
                else
                    color = 2; // copper
                break;
            case 4:
                color = 0; // zinc
                break;
            case 5:
                color = 6; // tin
                break;
            case 6:
                if (x1 >= 270)
                {
                    color = 8; // coal
                    tile = tile_chr::Gem;
                }
                else
                    color = 2; // copper
                break;
            case 7:
                if (x1 >= 270)
                    color = 15; // platinum
                else
                    color = 6; // tin
                break;
            }
            for (int j = 0; j < 10; j++)
            {
                vector<df::coord2d> points = generateLine(x1, y1, x2, y2);
                for (int k = 0; k < points.size(); k++)
                {
                    int _x = points[k].x;
                    int _y = points[k].y;
                    setTileChr(_x, _y, z, tile);
                    setTileColor(_x, _y, z, color);
                    if (!random(2))
                    {
                        setTileChr(_x - 1, _y, z, tile);
                        setTileColor(_x - 1, _y, z, color);
                    }
                    if (!random(2))
                    {
                        setTileChr(_x + 1, _y, z, tile);
                        setTileColor(_x + 1, _y, z, color);
                    }
                    if (!random(2))
                    {
                        setTileChr(_x, _y - 1, z, tile);
                        setTileColor(_x, _y - 1, z, color);
                    }
                    if (!random(2))
                    {
                        setTileChr(_x, _y + 1, z, tile);
                        setTileColor(_x, _y + 1, z, color);
                    }
                }
                x1 = x2;
                y1 = y2;

                x2 += random(10) + 1;
                if (x2 > world->map.x_count - 3)
                    x2 = world->map.x_count - 3;

                y2 += random(21) - 10;
                if (y2 < 1)
                    y2 = random(10) + 1;

                if (y2 > world->map.y_count - 2)
                    y2 = world->map.y_count - 2 - random(10);
            }
        }
    }

    // insert gem clusters
    {
        vector<int16_t> gem_ornamental[4];
        vector<int16_t> gem_semi[4];
        vector<int16_t> gem_precious[4];
        for (int i = 0; i < world->raws.matgloss.gem.size(); i++)
        {
            auto gem = world->raws.matgloss.gem[i];
            int color = 0;
            switch (gem->color)
            {
            case 1: // blue = 3
                color++;
            case 7: // white = 2
                color++;
            case 4: // red = 1
                color++;
            case 2: // green = 0
                if (gem->flags.bits.ORNAMENTAL)
                    gem_ornamental[color].push_back(i);
                if (gem->flags.bits.SEMI)
                    gem_semi[color].push_back(i);
                if (gem->flags.bits.PRECIOUS)
                    gem_precious[color].push_back(i);
            }
        }
        for (int i = 0; i < 680; i++)
        {
            int x = random(10) + (i / 2) + ui->min_dig_depth + 1;
            int y = random(world->map.y_count - 2) + 1;
            if (x >= world->map.x_count - 1)
                break;
            char color;
            switch (i % 4)
            {
            case 0:
                color = 10; // green
                break;
            case 1:
                color = 12; // red
                break;
            case 2:
                color = 15; // white
                break;
            case 3:
                color = 9; // blue
                break;
            }
            setTileChr(x, y, z, tile_chr::Gem);
            setTileColor(x, y, z, color);
            if (!random(2))
            {
                setTileChr(x - 1, y, z, tile_chr::Gem);
                setTileColor(x - 1, y, z, color);
            }
            if (!random(2))
            {
                setTileChr(x + 1, y, z, tile_chr::Gem);
                setTileColor(x + 1, y, z, color);
            }
            if (!random(2))
            {
                setTileChr(x, y - 1, z, tile_chr::Gem);
                setTileColor(x, y - 1, z, color);
            }
            if (!random(2))
            {
                setTileChr(x, y + 1, z, tile_chr::Gem);
                setTileColor(x, y + 1, z, color);
            }

            auto cluster = new df::mineral_cluster();
            world->gem_clusters.push_back(cluster);
            cluster->z = z;
            cluster->x1 = x - 1;
            cluster->x2 = x + 1;
            cluster->y1 = y - 1;
            cluster->y2 = y + 1;
            cluster->color = getTileColor(x, y, z) & 7;

            if (x < 180)
            {
                if (!random(10))
                    cluster->material = material_type::GEM_SEMI;
                else
                    cluster->material = material_type::GEM_ORNAMENTAL;
            }
            else if (x < 280)
            {
                if (!random(10))
                    cluster->material = material_type::GEM_PRECIOUS;
                else if (!random(2))
                    cluster->material = material_type::GEM_SEMI;
                else
                    cluster->material = material_type::GEM_ORNAMENTAL;
            }
            else if (x < 380)
            {
                if (!random(5))
                    cluster->material = material_type::GEM_PRECIOUS;
                else if (random(3))
                    cluster->material = material_type::GEM_SEMI;
                else
                    cluster->material = material_type::GEM_ORNAMENTAL;
            }
            else
            {
                if (random(3))
                    cluster->material = material_type::GEM_PRECIOUS;
                else if (random(5))
                    cluster->material = material_type::GEM_SEMI;
                else
                    cluster->material = material_type::GEM_ORNAMENTAL;
            }
            switch (cluster->material)
            {
            case material_type::GEM_ORNAMENTAL:
                cluster->matgloss = gem_ornamental[i % 4][random(gem_ornamental[i % 4].size())];
                break;
            case material_type::GEM_SEMI:
                cluster->matgloss = gem_semi[i % 4][random(gem_semi[i % 4].size())];
                break;
            case material_type::GEM_PRECIOUS:
                cluster->matgloss = gem_precious[i % 4][random(gem_precious[i % 4].size())];
                break;
            }
        }
    }

    // adjust finished map to accomodate features
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
                        continue;
                    // open space above cave river, chasm, magma flow
                    if ((base->chr[tx][ty] == tile_chr::River) || (base->chr[tx][ty] == tile_chr::Waterfall) || ((base->chr[tx][ty] == tile_chr::Chasm) && ((base->color[tx][ty].bits.color == 4) || (base->color[tx][ty].bits.color == 8))))
                    {
                        cur->chr[tx][ty] = tile_chr::OpenSpace;
                        cur->color[tx][ty].bits.color = 0;
                    }
                    // clone eerie glowing pits upward (so they hopefully still trigger HFS upon discovery) along with raw adamantine
                    if (((base->chr[tx][ty] == tile_chr::Chasm) && (base->color[tx][ty].bits.color == 12)) || ((base->chr[tx][ty] == tile_chr::Ore) && (base->color[tx][ty].bits.color == 11)))
                    {
                        cur->chr[tx][ty] = base->chr[tx][ty];
                        cur->color[tx][ty].bits.color = base->color[tx][ty].bits.color;
                    }
                }
            }
        }
    }

    world->reindex_pathfinding = true;
    out.print("Successfully added new Z-level.\n");
    return CR_OK;
}

command_result addstair (color_ostream &out, vector <string> & parameters)
{
    uint32_t x_max,y_max,z_max;
    Maps::getSize(x_max, y_max, z_max);

    int32_t cx, cy, cz;
    Gui::getCursorCoords(cx, cy, cz);

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
    if (Maps::getTileOccupancy(cx, cy, cz)->bits.building)
    {
        out.printerr("There is a building in the way!\n");
        return CR_FAILURE;
    }

    df::tiletype_shape shape1 = tileShape(Maps::getTileType(cx, cy, cz));
    df::tiletype_material mat1 = tileMaterial(Maps::getTileType(cx, cy, cz));
    df::tiletype_material mat2 = tileMaterial(Maps::getTileType(cx, cy, cz + 1));

    if (shape1 != tiletype_shape::FLOOR && shape1 != tiletype_shape::STAIR_DOWN)
    {
        out.printerr("Stairs can only be placed on floors or downward stairs!\n");
        return CR_FAILURE;
    }

    // add upward stair on lower Z-level
    if (getTileChr(cx, cy, cz) == tile_chr::StairD)
        setTileChr(cx, cy, cz, tile_chr::StairUD);
    else
        setTileChr(cx, cy, cz, tile_chr::StairU);

    // ensure that the up-stair is made of a valid material (it may have been mud/dirt)
    if (mat1 != tiletype_material::STONE && mat1 != tiletype_material::STONE_LIGHT && mat1 != tiletype_material::STONE_DARK)
        setTileColor(cx, cy, cz, 7);

    // add downward stair on upper Z-level
    if (getTileChr(cx, cy, cz + 1) == tile_chr::StairU)
        setTileChr(cx, cy, cz + 1, tile_chr::StairUD);
    else
        setTileChr(cx, cy, cz + 1, tile_chr::StairD);

    // ensure that the down-stair is made of a valid material (it may have been ore/gem)
    if (mat2 != tiletype_material::STONE && mat2 != tiletype_material::STONE_LIGHT && mat2 != tiletype_material::STONE_DARK)
        setTileColor(cx, cy, cz + 1, 7);

    // reveal it
    Maps::getTileDesignation(cx, cy, cz + 1)->bits.hidden = 0;

    // and reveal the surrounding tiles
    Maps::getTileDesignation(cx - 1, cy, cz + 1)->bits.hidden = 0;
    Maps::getTileDesignation(cx + 1, cy, cz + 1)->bits.hidden = 0;
    Maps::getTileDesignation(cx, cy - 1, cz + 1)->bits.hidden = 0;
    Maps::getTileDesignation(cx, cy + 1, cz + 1)->bits.hidden = 0;

    world->reindex_pathfinding = true;
    out.print("Successfully added upward stairs.\n");
    return CR_OK;
}

command_result addramp (color_ostream &out, vector <string> & parameters)
{
    uint32_t x_max,y_max,z_max;
    Maps::getSize(x_max, y_max, z_max);

    int32_t cx, cy, cz;
    Gui::getCursorCoords(cx, cy, cz);

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
    if (Maps::getTileOccupancy(cx, cy, cz)->bits.building)
    {
        out.printerr("There is a building in the way!\n");
        return CR_FAILURE;
    }

    df::tiletype_shape shape1 = tileShape(Maps::getTileType(cx, cy, cz));
    df::tiletype_material mat1 = tileMaterial(Maps::getTileType(cx, cy, cz));

    if (shape1 != tiletype_shape::FLOOR)
    {
        out.printerr("Ramps can only be placed on floors!\n");
        return CR_FAILURE;
    }

    // add upward ramp on current Z-level
    setTileChr(cx, cy, cz, tile_chr::Ramp);

    // ensure that the up-ramp is made of a valid material (it may have been mud/dirt)
    if (mat1 != tiletype_material::STONE && mat1 != tiletype_material::STONE_LIGHT && mat1 != tiletype_material::STONE_DARK)
        setTileColor(cx, cy, cz, 7);

    // add downward ramp on Z-level above, using same color
    setTileChr(cx, cy, cz + 1, tile_chr::RampTop);
    setTileColor(cx, cy, cz + 1, getTileColor(cx, cy, cz));

    // reveal it
    Maps::getTileDesignation(cx, cy, cz + 1)->bits.hidden = 0;

    // and reveal the surrounding tiles
    Maps::getTileDesignation(cx - 1, cy, cz + 1)->bits.hidden = 0;
    Maps::getTileDesignation(cx + 1, cy, cz + 1)->bits.hidden = 0;
    Maps::getTileDesignation(cx, cy - 1, cz + 1)->bits.hidden = 0;
    Maps::getTileDesignation(cx, cy + 1, cz + 1)->bits.hidden = 0;

    world->reindex_pathfinding = true;
    out.print("Successfully added ramp.\n");
    return CR_OK;
}

command_result remstair (color_ostream &out, vector <string> & parameters)
{
    uint32_t x_max,y_max,z_max;
    Maps::getSize(x_max, y_max, z_max);

    int32_t cx, cy, cz;
    Gui::getCursorCoords(cx, cy, cz);

    if (cx < 0)
    {
        out.printerr("Cursor is not active. Point the cursor where you want to remove stairs/ramps.\n");
        return CR_FAILURE;
    }

    const df::tile_chr floors[4] = { tile_chr::Floor1, tile_chr::Floor2, tile_chr::Floor3, tile_chr::Floor4 };
    if (getTileChr(cx, cy, cz) == tile_chr::Ramp)
    {
        // change upward ramp to random rough floor
        setTileChr(cx, cy, cz, floors[random(4)]);

        // and change downward ramp to open space
        if (getTileChr(cx, cy, cz + 1) == tile_chr::RampTop)
        {
            setTileChr(cx, cy, cz + 1, tile_chr::OpenSpace);
            setTileColor(cx, cy, cz + 1, 0);
        }

        world->reindex_pathfinding = true;
        out.print("Successfully removed ramp.\n");
        return CR_OK;
    }

    if (getTileChr(cx, cy, cz) == tile_chr::StairU || getTileChr(cx, cy, cz) == tile_chr::StairUD)
    {
        df::tile_chr chr = getTileChr(cx, cy, cz);
        if (chr == tile_chr::StairUD)
            setTileChr(cx, cy, cz, tile_chr::StairD);
        else if (chr == tile_chr::StairU)
            setTileChr(cx, cy, cz, floors[random(4)]);

        chr = getTileChr(cx, cy, cz + 1);
        if (chr == tile_chr::StairUD)
            setTileChr(cx, cy, cz, tile_chr::StairU);
        else if (chr == tile_chr::StairD)
            setTileChr(cx, cy, cz, floors[random(4)]);


        world->reindex_pathfinding = true;
        out.print("Successfully removed upward stairs.\n");
        return CR_OK;
    }

    out.printerr("Invalid tile selected!\n");
    return CR_FAILURE;
}

command_result df_3d (color_ostream &out, vector <string> & parameters)
{
    CoreSuspender suspend;

    if (parameters.empty())
        return CR_WRONG_USAGE;

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

    string cmd = parameters[0];

    if (cmd == "addz")
        return addzlevel(out, parameters);
    if (cmd == "addstair")
        return addstair(out, parameters);
    if (cmd == "addramp")
        return addramp(out, parameters);
    if (cmd == "remstair")
        return remstair(out, parameters);

    return CR_WRONG_USAGE;
}

DFHACK_PLUGIN("df3d");

DFhackCExport command_result plugin_init ( color_ostream &out, vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "df3d", "Make the 2D version of Dwarf Fortress more 3-dimensional.", df_3d, false,
        "  df3d addz\n"
        "    Adds a new Z-level to your fortress map.\n"
        "  df3d addstair\n"
        "    Inserts an upward staircase at the cursor and places a downward staircase\n"
        "    in the tile immediately above it, revealing any tiles around it.\n"
        "    Can only be used on floor tiles made of stone (gray/light/dark).\n"
        "  df3d addramp\n"
        "    Inserts an upward ramp at the cursor and places a downward ramp in the\n"
        "    tile immediately above it, revealing any tiles around it.\n"
        "    Can only be used on floor tiles made of stone (gray/light/dark).\n"
        "    Note that dwarves can NOT dig out tiles immediately adjacent to ramps, so\n"
        "    you will need to dig stairs to the area first.\n"
        "    Also, adjacent walls are NOT needed to traverse ramps in either direction.\n"
        "  df3d remstair\n"
        "    Deletes upward stairs/ramps from the tile under the cursor.\n"
    ));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}
