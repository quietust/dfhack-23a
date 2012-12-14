// (un)designate matching plants for gathering/cutting

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"

#include "DataDefs.h"
#include "TileTypes.h"
#include "modules/Maps.h"
#include "df/world.h"
#include "df/map_block.h"
#include "df/matgloss_plant.h"
#include "df/matgloss_wood.h"
#include "df/plant.h"

#include <set>

using std::string;
using std::vector;
using std::set;
using namespace DFHack;
using namespace df::enums;

using df::global::world;

command_result df_getplants (color_ostream &out, vector <string> & parameters)
{
    string plantMatStr = "";
    set<int> shrubTypes, woodTypes;
    set<string> plantNames;
    bool deselect = false, exclude = false, treesonly = false, shrubsonly = false, all = false;

    int count = 0;
    for (size_t i = 0; i < parameters.size(); i++)
    {
        if(parameters[i] == "help" || parameters[i] == "?")
            return CR_WRONG_USAGE;
        else if(parameters[i] == "-t")
            treesonly = true;
        else if(parameters[i] == "-s")
            shrubsonly = true;
        else if(parameters[i] == "-c")
            deselect = true;
        else if(parameters[i] == "-x")
            exclude = true;
        else if(parameters[i] == "-a")
            all = true;
        else
            plantNames.insert(parameters[i]);
    }
    if (treesonly && shrubsonly)
    {
        out.printerr("Cannot specify both -t and -s at the same time!\n");
        return CR_WRONG_USAGE;
    }
    if (all && exclude)
    {
        out.printerr("Cannot specify both -a and -x at the same time!\n");
        return CR_WRONG_USAGE;
    }
    if (all && plantNames.size())
    {
        out.printerr("Cannot specify -a along with plant IDs!\n");
        return CR_WRONG_USAGE;
    }

    CoreSuspender suspend;

    for (size_t i = 0; i < world->raws.matgloss.plant.size(); i++)
    {
        df::matgloss_plant *plant = world->raws.matgloss.plant[i];
        if (all)
            shrubTypes.insert(i);
        else if (plantNames.find(plant->id) != plantNames.end())
        {
            plantNames.erase(plant->id);
            shrubTypes.insert(i);
        }
    }
    for (size_t i = 0; i < world->raws.matgloss.wood.size(); i++)
    {
        df::matgloss_wood *wood = world->raws.matgloss.wood[i];
        if (all)
            woodTypes.insert(i);
        else if (plantNames.find(wood->id) != plantNames.end())
        {
            plantNames.erase(wood->id);
            woodTypes.insert(i);
        }
    }

    if (plantNames.size() > 0)
    {
        out.printerr("Invalid plant/wood ID(s):");
        for (set<string>::const_iterator it = plantNames.begin(); it != plantNames.end(); it++)
            out.printerr(" %s", it->c_str());
        out.printerr("\n");
        return CR_FAILURE;
    }

    if (shrubTypes.size() == 0 && woodTypes.size() == 0)
    {
        out.print("Valid plant IDs:\n");
        for (size_t i = 0; i < world->raws.matgloss.wood.size(); i++)
        {
            df::matgloss_plant *plant = world->raws.matgloss.plant[i];
            out.print("* (shrub) %s - %s\n", plant->id.c_str(), plant->name.c_str());
        }
        for (size_t i = 0; i < world->raws.matgloss.wood.size(); i++)
        {
            df::matgloss_wood *wood = world->raws.matgloss.wood[i];
            out.print("* (tree) %s - %s\n", wood->id.c_str(), wood->name.c_str());
        }
        return CR_OK;
    }

    count = 0;
    for (size_t i = 0; i < world->plants.all.size(); i++)
    {
        df::plant *plant = world->plants.all[i];
        df::map_block *cur = Maps::getTileBlock(plant->pos);
        bool dirty = false;
        {
            int x = plant->pos.x % 16;
            int y = plant->pos.y % 16;
            df::tiletype tt = Maps::getTileType(plant->pos);
            df::tiletype_shape shape = tileShape(tt);
            df::tiletype_special special = tileSpecial(tt);
            bool is_shrub = plant->flags >= plant_flags::shrub_forest && plant->flags <= plant_flags::shrub_cave;
            bool is_tree = plant->flags >= plant_flags::tree_outdoor_wet && plant->flags <= plant_flags::tree_indoor_wet;
            if ((is_shrub && (shrubTypes.find(plant->plant_id) != shrubTypes.end())) ||
                (is_tree && (woodTypes.find(plant->wood_id) != woodTypes.end())))
            {
                if (exclude)
                    continue;
            }
            else
            {
                if (!exclude)
                    continue;
            }
            if (is_shrub && (treesonly || !(shape == tiletype_shape::SHRUB && special != tiletype_special::DEAD)))
                continue;
            if (is_tree && (shrubsonly || !(shape == tiletype_shape::TREE)))
                continue;
            if (cur->designation[x][y].bits.hidden)
                continue;
            if (deselect && cur->designation[x][y].bits.dig)
            {
                cur->designation[x][y].bits.dig = 0;
                dirty = true;
                ++count;
            }
            if (!deselect && cur->designation[x][y].bits.dig == 0)
            {
                cur->designation[x][y].bits.dig = 1;
                dirty = true;
                ++count;
            }
        }
        if (dirty)
            cur->flags.set(block_flags::designated);
    }
    if (count)
        out.print("Updated %d plant designations.\n", count);
    return CR_OK;
}

DFHACK_PLUGIN("getplants");

DFhackCExport command_result plugin_init ( color_ostream &out, vector <PluginCommand> &commands)
{
    commands.push_back(PluginCommand(
        "getplants", "Cut down all of the specified trees or gather specified shrubs",
        df_getplants, false,
        "  Specify the types of trees to cut down and/or shrubs to gather by their\n"
        "  plant IDs, separated by spaces.\n"
        "Options:\n"
        "  -t - Select trees only (exclude shrubs)\n"
        "  -s - Select shrubs only (exclude trees)\n"
        "  -c - Clear designations instead of setting them\n"
        "  -x - Apply selected action to all plants except those specified\n"
        "  -a - Select every type of plant (obeys -t/-s)\n"
        "Specifying both -t and -s will have no effect.\n"
        "If no plant IDs are specified, all valid plant IDs will be listed.\n"
    ));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}
