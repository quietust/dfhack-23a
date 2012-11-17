
#include "Internal.h"

#include <string>
#include <sstream>
#include <vector>
#include <cstdio>
#include <map>
#include <set>
using namespace std;

#include "VersionInfo.h"
#include "MemAccess.h"
#include "Types.h"
#include "Error.h"
#include "modules/kitchen.h"
#include "ModuleFactory.h"
#include "Core.h"
using namespace DFHack;

#include "DataDefs.h"
#include "df/world.h"
#include "df/ui.h"
#include "df/item_type.h"
#include "df/matgloss_plant.h"

using namespace df::enums;
using df::global::world;
using df::global::ui;

void Kitchen::debug_print(color_ostream &out)
{
    out.print("Kitchen Exclusions\n");
    for(std::size_t i = 0; i < size(); ++i)
    {
        out.print("%2u: IT:%2i IS:%i MT:%3i MI:%2i ET:%i %s\n",
                       i,
                       ui->kitchen.item_types[i],
                       ui->kitchen.item_subtypes[i],
                       ui->kitchen.materials[i],
                       ui->kitchen.matglosses[i],
                       ui->kitchen.exc_types[i],
                       (ui->kitchen.materials[i] == material_type::PLANT || ui->kitchen.materials[i] == material_type::PLANT_ALCOHOL) ? world->raws.matgloss.plant[ui->kitchen.matglosses[i]]->id.c_str() : "n/a"
        );
    }
    out.print("\n");
}

void Kitchen::allowPlantSeedCookery(t_materialSubtype plant)
{
    bool match = false;
    do
    {
        match = false;
        std::size_t matchIndex = 0;
        for(std::size_t i = 0; i < size(); ++i)
        {
            if(ui->kitchen.matglosses[i] == plant
               && (ui->kitchen.item_types[i] == item_type::SEEDS || ui->kitchen.item_types[i] == item_type::PLANT)
               && ui->kitchen.exc_types[i] == cookingExclusion
            )
            {
                match = true;
                matchIndex = i;
            }
        }
        if(match)
        {
            ui->kitchen.item_types.erase(ui->kitchen.item_types.begin() + matchIndex);
            ui->kitchen.item_subtypes.erase(ui->kitchen.item_subtypes.begin() + matchIndex);
            ui->kitchen.matglosses.erase(ui->kitchen.matglosses.begin() + matchIndex);
            ui->kitchen.materials.erase(ui->kitchen.materials.begin() + matchIndex);
            ui->kitchen.exc_types.erase(ui->kitchen.exc_types.begin() + matchIndex);
        }
    } while(match);
};

void Kitchen::denyPlantSeedCookery(t_materialSubtype plant)
{
    df::matgloss_plant *type = world->raws.matgloss.plant[plant];
    bool SeedAlreadyIn = false;
    bool PlantAlreadyIn = false;
    for(std::size_t i = 0; i < size(); ++i)
    {
        if(ui->kitchen.matglosses[i] == plant
           && ui->kitchen.exc_types[i] == cookingExclusion)
        {
            if(ui->kitchen.item_types[i] == item_type::SEEDS)
                SeedAlreadyIn = true;
            else if (ui->kitchen.item_types[i] == item_type::PLANT)
                PlantAlreadyIn = true;
        }
    }
    if(!SeedAlreadyIn)
    {
        ui->kitchen.item_types.push_back(item_type::SEEDS);
        ui->kitchen.item_subtypes.push_back(organicSubtype);
        ui->kitchen.materials.push_back(material_type::PLANT);
        ui->kitchen.matglosses.push_back(plant);
        ui->kitchen.exc_types.push_back(cookingExclusion);
    }
    if(!PlantAlreadyIn)
    {
        ui->kitchen.item_types.push_back(item_type::PLANT);
        ui->kitchen.item_subtypes.push_back(organicSubtype);
        ui->kitchen.materials.push_back(material_type::PLANT);
        ui->kitchen.matglosses.push_back(plant);
        ui->kitchen.exc_types.push_back(cookingExclusion);
    }
};

void Kitchen::fillWatchMap(std::map<t_materialSubtype, unsigned int>& watchMap)
{
    watchMap.clear();
    for(std::size_t i = 0; i < size(); ++i)
    {
        if(ui->kitchen.item_subtypes[i] == (short)limitType && ui->kitchen.item_subtypes[i] == (short)limitSubtype && ui->kitchen.exc_types[i] == limitExclusion)
        {
            watchMap[ui->kitchen.matglosses[i]] = (unsigned int) ui->kitchen.materials[i];
        }
    }
};

void Kitchen::removeLimit(t_materialSubtype plant)
{
    bool match = false;
    do
    {
        match = false;
        std::size_t matchIndex = 0;
        for(std::size_t i = 0; i < size(); ++i)
        {
            if(ui->kitchen.item_types[i] == limitType
               && ui->kitchen.item_subtypes[i] == limitSubtype
               && ui->kitchen.matglosses[i] == plant
               && ui->kitchen.exc_types[i] == limitExclusion)
            {
                match = true;
                matchIndex = i;
            }
        }
        if(match)
        {
            ui->kitchen.item_types.erase(ui->kitchen.item_types.begin() + matchIndex);
            ui->kitchen.item_subtypes.erase(ui->kitchen.item_subtypes.begin() + matchIndex);
            ui->kitchen.materials.erase(ui->kitchen.materials.begin() + matchIndex);
            ui->kitchen.matglosses.erase(ui->kitchen.matglosses.begin() + matchIndex);
            ui->kitchen.exc_types.erase(ui->kitchen.exc_types.begin() + matchIndex);
        }
    } while(match);
};

void Kitchen::setLimit(t_materialSubtype plant, unsigned int limit)
{
    removeLimit(plant);
    if(limit > seedLimit)
    {
        limit = seedLimit;
    }
    ui->kitchen.item_types.push_back(limitType);
    ui->kitchen.item_subtypes.push_back(limitSubtype);
    ui->kitchen.matglosses.push_back(plant);
    ui->kitchen.materials.push_back(df::enum_field<df::material_type,int32_t>((df::material_type)((limit < seedLimit) ? limit : seedLimit)));
    ui->kitchen.exc_types.push_back(limitExclusion);
};

void Kitchen::clearLimits()
{
    bool match = false;
    do
    {
        match = false;
        std::size_t matchIndex;
        for(std::size_t i = 0; i < size(); ++i)
        {
            if(ui->kitchen.item_types[i] == limitType
               && ui->kitchen.item_subtypes[i] == limitSubtype
               && ui->kitchen.exc_types[i] == limitExclusion)
            {
                match = true;
                matchIndex = i;
            }
        }
        if(match)
        {
            ui->kitchen.item_types.erase(ui->kitchen.item_types.begin() + matchIndex);
            ui->kitchen.item_subtypes.erase(ui->kitchen.item_subtypes.begin() + matchIndex);
            ui->kitchen.matglosses.erase(ui->kitchen.matglosses.begin() + matchIndex);
            ui->kitchen.materials.erase(ui->kitchen.materials.begin() + matchIndex);
            ui->kitchen.exc_types.erase(ui->kitchen.exc_types.begin() + matchIndex);
        }
    } while(match);
};

size_t Kitchen::size()
{
    return ui->kitchen.item_types.size();
};
