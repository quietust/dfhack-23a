#include <iostream>
#include <iomanip>
#include <map>
#include <algorithm>
#include <vector>
#include <string>

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "modules/Maps.h"
#include "modules/Gui.h"
#include "TileTypes.h"
#include "modules/MapCache.h"
#include "df/plant.h"
#include "df/matgloss_plant.h"
#include "df/matgloss_wood.h"

using std::vector;
using std::string;
using namespace DFHack;
using df::global::world;

const uint32_t sapling_to_tree_threshold = 120 * 28 * 12 * 3; // 3 years

DFHACK_PLUGIN("plants");

enum do_what
{
    do_immolate,
    do_extirpate
};

static bool getoptions( vector <string> & parameters, bool & shrubs, bool & trees, bool & help)
{
    for(size_t i = 0;i < parameters.size();i++)
    {
        if(parameters[i] == "shrubs")
        {
            shrubs = true;
        }
        else if(parameters[i] == "trees")
        {
            trees = true;
        }
        else if(parameters[i] == "all")
        {
            trees = true;
            shrubs = true;
        }
        else if(parameters[i] == "help" || parameters[i] == "?")
        {
            help = true;
        }
        else
        {
            return false;
        }
    }
    return true;
}

/**
 * Book of Immolations, chapter 1, verse 35:
 * Armok emerged from the hellish depths and beheld the sunny realms for the first time.
 * And he cursed the plants and trees for their bloodless wood, turning them into ash and smoldering ruin.
 * Armok was pleased and great temples were built by the dwarves, for they shared his hatred for trees and plants.
 */
static command_result immolations (color_ostream &out, do_what what, bool shrubs, bool trees, bool help)
{
    if(help)
        return CR_WRONG_USAGE;
    CoreSuspender suspend;
    if (!Maps::IsValid())
    {
        out.printerr("Map is not available!\n");
        return CR_FAILURE;
    }
    uint32_t x_max, y_max, z_max;
    Maps::getSize(x_max, y_max, z_max);
    MapExtras::MapCache map;
    if(shrubs || trees)
    {
        int destroyed = 0;
        for(size_t i = 0 ; i < world->plants.all.size(); i++)
        {
            df::plant *p = world->plants.all[i];
            bool is_shrub = p->flags >= plant_flags::shrub_forest;
            if(shrubs && is_shrub || trees && !is_shrub)
            {
                if (what == do_immolate)
                    p->damage_flags.bits.is_burning = true;
                p->hitpoints = 0;
                destroyed ++;
            }
        }
        out.print("Praise Armok!\n");
    }
    else
    {
        int32_t x,y,z;
        if(Gui::getCursorCoords(x,y,z))
        {
            auto block = Maps::getTileBlock(x,y,z);
            stl::vector<df::plant *> *alltrees = &world->plants.all;
            if(alltrees)
            {
                bool didit = false;
                for(size_t i = 0 ; i < alltrees->size(); i++)
                {
                    df::plant * tree = alltrees->at(i);
                    if(tree->pos.x == x && tree->pos.y == y && tree->pos.z == z)
                    {
                        if(what == do_immolate)
                            tree->damage_flags.bits.is_burning = true;
                        tree->hitpoints = 0;
                        didit = true;
                        break;
                    }
                }
                /*
                if(!didit)
                {
                    cout << "----==== There's NOTHING there! ====----" << endl;
                }
                */
            }
        }
        else
        {
            out.printerr("No mass destruction and no cursor...\n" );
        }
    }
    return CR_OK;
}

command_result df_immolate (color_ostream &out, vector <string> & parameters)
{
    bool shrubs = false, trees = false, help = false;
    if(getoptions(parameters,shrubs,trees,help))
    {
        return immolations(out,do_immolate,shrubs,trees,help);
    }
    else
    {
        out.printerr("Invalid parameter!\n");
        return CR_WRONG_USAGE;
    }
}

command_result df_extirpate (color_ostream &out, vector <string> & parameters)
{
    bool shrubs = false, trees = false, help = false;
    if(getoptions(parameters,shrubs,trees,help))
    {
        return immolations(out,do_extirpate,shrubs,trees,help);
    }
    else
    {
        out.printerr("Invalid parameter!\n");
        return CR_WRONG_USAGE;
    }
}

command_result df_grow (color_ostream &out, vector <string> & parameters)
{
    for(size_t i = 0; i < parameters.size();i++)
    {
        if(parameters[i] == "help" || parameters[i] == "?")
            return CR_WRONG_USAGE;
    }
    CoreSuspender suspend;

    if (!Maps::IsValid())
    {
        out.printerr("Map is not available!\n");
        return CR_FAILURE;
    }
    MapExtras::MapCache map;
    int32_t x,y,z;
    if(Gui::getCursorCoords(x,y,z))
    {
        auto block = Maps::getTileBlock(x,y,z);
        stl::vector<df::plant *> *alltrees = &world->plants.all;
        if(alltrees)
        {
            for(size_t i = 0 ; i < alltrees->size(); i++)
            {
                df::plant * tree = alltrees->at(i);
                if(tree->pos.x == x && tree->pos.y == y && tree->pos.z == z)
                {
                    if(tileShape(map.tiletypeAt(DFCoord(x,y,z))) == tiletype_shape::SAPLING &&
                        tileSpecial(map.tiletypeAt(DFCoord(x,y,z))) != tiletype_special::DEAD)
                    {
                        tree->grow_counter = sapling_to_tree_threshold;
                    }
                    break;
                }
            }
        }
    }
    else
    {
        int grown = 0;
        for(size_t i = 0 ; i < world->plants.all.size(); i++)
        {
            df::plant *p = world->plants.all[i];
            df::tiletype ttype = map.tiletypeAt(df::coord(p->pos.x,p->pos.y,p->pos.z));
            bool is_shrub = p->flags >= plant_flags::shrub_forest;
            if(!is_shrub && tileShape(ttype) == tiletype_shape::SAPLING && tileSpecial(ttype) != tiletype_special::DEAD)
            {
                p->grow_counter = sapling_to_tree_threshold;
            }
        }
    }

    return CR_OK;
}

command_result df_createplant (color_ostream &out, vector <string> & parameters)
{
    if ((parameters.size() != 1) || (parameters[0] == "help" || parameters[0] == "?"))
        return CR_WRONG_USAGE;

    CoreSuspender suspend;

    if (!Maps::IsValid())
    {
        out.printerr("Map is not available!\n");
        return CR_FAILURE;
    }

    int32_t x,y,z;
    if(!Gui::getCursorCoords(x,y,z))
    {
        out.printerr("No cursor detected - please place the cursor over the location in which you wish to create a new plant.\n");
        return CR_FAILURE;
    }
    df::map_block *map = Maps::getTileBlock(x, y, z);
    if (!map)
    {
        out.printerr("Invalid location selected!\n");
        return CR_FAILURE;
    }
    int tx = x & 15, ty = y & 15;
    int mat = tileMaterial(Maps::getTileType(x, y, z));
    if ((tileShape(Maps::getTileType(x, y, z)) != tiletype_shape::FLOOR) || ((mat != tiletype_material::DIRT) && (mat != tiletype_material::GRASS_DARK) && (mat != tiletype_material::GRASS_LIGHT)))
    {
        out.printerr("Plants can only be placed on dirt or grass floors!\n");
        return CR_FAILURE;
    }

    int plant_id = -1;
    int wood_id = -1;
    df::matgloss_plant *plant_raw = NULL;
    df::matgloss_wood *wood_raw = NULL;
    for (size_t i = 0; i < world->raws.matgloss.plant.size(); i++)
    {
        plant_raw = world->raws.matgloss.plant[i];
        if (plant_raw->id == parameters[0])
        {
            plant_id = i;
            break;
        }
    }
    for (size_t i = 0; i < world->raws.matgloss.wood.size(); i++)
    {
        wood_raw = world->raws.matgloss.wood[i];
        if (wood_raw->id == parameters[0])
        {
            wood_id = i;
            break;
        }
    }
    if (plant_id != -1 && wood_id != -1)
    {
        out.printerr("Specified name matches both a tree and a shrub! Defaulting to tree...\n");
        plant_id = -1;
        plant_raw = NULL;
    }

    if (plant_id == -1 && wood_id == -1)
    {
        out.printerr("Invalid plant ID specified!\n");
        return CR_FAILURE;
    }

    df::plant *plant = new df::plant;
    if (wood_raw)
    {
        plant->hitpoints = 400000;
        // for now, always set "watery" for WET-permitted plants, even if they're spawned away from water
        // the proper method would be to actually look for nearby water features, but it's not clear exactly how that works
        bool is_wet = wood_raw->flags.is_set(matgloss_wood_flags::WET);
        bool is_indoor = wood_raw->flags.is_set(matgloss_wood_flags::BIOME_SUBTERRANEAN_RIVER) | wood_raw->flags.is_set(matgloss_wood_flags::BIOME_SUBTERRANEAN_CHASM) | wood_raw->flags.is_set(matgloss_wood_flags::BIOME_SUBTERRANEAN_LAVA);
        if (is_wet && is_indoor)
            plant->flags = plant_flags::tree_indoor_wet;
        else if (is_indoor)
            plant->flags = plant_flags::tree_indoor_dry;
        else if (is_wet)
            plant->flags = plant_flags::tree_outdoor_wet;
        else
            plant->flags = plant_flags::tree_outdoor_dry;
        plant->wood_id = wood_id;
    }
    else
    {
        plant->hitpoints = 100000;

        if (plant_raw->flags.is_set(matgloss_plant_flags::CAVE))
            plant->flags = plant_flags::shrub_cave;
        else if (plant_raw->flags.is_set(matgloss_plant_flags::RIVER))
            plant->flags = plant_flags::shrub_river;
        else if (plant_raw->flags.is_set(matgloss_plant_flags::SWAMP))
            plant->flags = plant_flags::shrub_swamp;
        else if (plant_raw->flags.is_set(matgloss_plant_flags::FOREST))
            plant->flags = plant_flags::shrub_forest;
        else // this shouldn't happen
            plant->flags = plant_flags::shrub_forest;

        plant->plant_id = plant_id;
    }
    plant->pos.x = x;
    plant->pos.y = y;
    plant->pos.z = z;
//  plant->temperature_tile_tick = -1;

    world->plants.all.push_back(plant);
    switch (plant->flags)
    {
    case plant_flags::tree_outdoor_wet: world->plants.tree_outside_dry.push_back(plant); break;
    case plant_flags::tree_outdoor_dry: world->plants.tree_outside_wet.push_back(plant); break;
    case plant_flags::tree_indoor_dry: world->plants.tree_inside_dry.push_back(plant); break;
    case plant_flags::tree_indoor_wet: world->plants.tree_inside_wet.push_back(plant); break;
    case plant_flags::shrub_forest: world->plants.shrub_forest.push_back(plant); break;
    case plant_flags::shrub_swamp: world->plants.shrub_swamp.push_back(plant); break;
    case plant_flags::shrub_river: world->plants.shrub_river.push_back(plant); break;
    case plant_flags::shrub_cave: world->plants.shrub_cave.push_back(plant); break;
    }

    if (plant_id)
        map->chr[tx][ty] = tile_chr::Shrub;
    else
        map->chr[tx][ty] = tile_chr::Sapling;
    map->color[tx][ty].bits.color = 2;

    return CR_OK;
}

DFhackCExport command_result plugin_init ( color_ostream &out, std::vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand("grow", "Grows saplings into trees (with active cursor, only the targetted one).", df_grow, false,
        "This command turns all living saplings on the map into full-grown trees.\n"));
    commands.push_back(PluginCommand("immolate", "Set plants on fire (under cursor, 'shrubs', 'trees' or 'all').", df_immolate, false,
        "Without any options, this command burns a plant under the cursor.\n"
        "Options:\n"
        "shrubs   - affect all shrubs\n"
        "trees    - affect all trees\n"
        "all      - affect all plants\n"));
    commands.push_back(PluginCommand("extirpate", "Kill plants (same mechanics as immolate).", df_extirpate, false,
        "Without any options, this command destroys a plant under the cursor.\n"
        "Options:\n"
        "shrubs   - affect all shrubs\n"
        "trees    - affect all trees\n"
        "all      - affect all plants\n"));
    commands.push_back(PluginCommand("createplant", "Create a new plant at the cursor.", df_createplant, false,
        "Specify the type of plant to create by its raw ID (e.g. TOWER_CAP or MUSHROOM_HELMET_PLUMP).\n"
        "Only shrubs and saplings can be placed, and they must be located on a dirt or grass floor.\n"));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}
