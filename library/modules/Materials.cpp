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
#include <cstring>
using namespace std;

#include "Types.h"
#include "modules/Materials.h"
#include "VersionInfo.h"
#include "MemAccess.h"
#include "Error.h"
#include "ModuleFactory.h"
#include "Core.h"

#include "MiscUtils.h"

#include "df/world.h"
#include "df/ui.h"
#include "df/item.h"
#include "df/creature_raw.h"
#include "df/matgloss_wood.h"
#include "df/matgloss_stone.h"
#include "df/matgloss_gem.h"
#include "df/matgloss_plant.h"
#include "df/body_part_raw.h"
#include "df/historical_figure.h"

#include "df/material_vec_ref.h"

#include "df/descriptor_color.h"
#include "df/descriptor_shape.h"

using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;

bool MaterialInfo::decode(df::item *item)
{
    if (!item)
        return decode(-1);
    else
        return decode(item->getActualMaterial(), item->getActualMatgloss());
}

bool MaterialInfo::decode(const df::material_vec_ref &vr, int idx)
{
    if (size_t(idx) >= vr.material.size() || size_t(idx) >= vr.matgloss.size())
        return decode(-1);
    else
        return decode(vr.material[idx], vr.matgloss[idx]);
}

bool MaterialInfo::decode(df::material_type type, int16_t subtype)
{
    this->type = type;
    this->subtype = subtype;

    mode = Invalid;

    wood = NULL;
    stone = NULL;
    gem = NULL;
    plant = NULL;
    creature = NULL;

    df::world_raws &raws = world->raws;

    if (ENUM_ATTR(material_type,is_wood,type))
    {
        if (subtype >= 0 && subtype < raws.matgloss.wood.size())
        {
            mode = Wood;
            wood = raws.matgloss.wood[subtype];
        }
    }

    if (ENUM_ATTR(material_type,is_stone,type))
    {
        if (subtype >= 0 && subtype < raws.matgloss.stone.size())
        {
            mode = Stone;
            stone = raws.matgloss.stone[subtype];
        }
    }

    if (ENUM_ATTR(material_type,is_gem,type))
    {
        if (subtype >= 0 && subtype < raws.matgloss.gem.size())
        {
            mode = Gem;
            gem = raws.matgloss.gem[subtype];
        }
    }

    if (ENUM_ATTR(material_type,is_plant,type))
    {
        if (subtype >= 0 && subtype < raws.matgloss.plant.size())
        {
            mode = Plant;
            plant = raws.matgloss.plant[subtype];
        }
    }

    if (ENUM_ATTR(material_type,is_creature,type))
    {
        if (subtype >= 0 && subtype < raws.creatures.size())
        {
            mode = Creature;
            creature = raws.creatures[subtype];
        }
    }

    if (ENUM_ATTR(material_type,is_builtin,type))
        mode = Builtin;

    if (type == material_type::NONE)
        mode = None;

    return (mode != Invalid) && (mode != None);
}

bool MaterialInfo::find(const std::string &token)
{
    std::vector<std::string> items;
    split_string(&items, token, ":");
    return find(items);
}

bool MaterialInfo::find(const std::vector<std::string> &items)
{
    if (items.size() != 2)
        return false;

    FOR_ENUM_ITEMS(material_type, i)
    {
        if (ENUM_KEY_STR(material_type, i) != items[0])
            continue;
        if (ENUM_ATTR(material_type,is_builtin,i))
            return findBuiltin(i, items[1]);
        if (ENUM_ATTR(material_type,is_wood,i))
            return findWood(i, items[1]);
        if (ENUM_ATTR(material_type,is_stone,i))
            return findStone(i, items[1]);
        if (ENUM_ATTR(material_type,is_gem,i))
            return findGem(i, items[1]);
        if (ENUM_ATTR(material_type,is_plant,i))
            return findPlant(i, items[1]);
        if (ENUM_ATTR(material_type,is_creature,i))
            return findCreature(i, items[1]);
    }
    return decode(-1);
    
    return false;
}

bool MaterialInfo::findBuiltin(df::material_type type, const std::string &subtype)
{
    if (type == material_type::COAL)
    {
        if (subtype == "COKE")
            return decode(type, 0);
        else if (subtype == "CHARCOAL")
            return decode(type, 1);
    }
    if (type == material_type::BLOOD)
    {
        // TODO: parse blood subtypes
    }
    return decode(type, -1);
}

bool MaterialInfo::findWood(df::material_type type, const std::string &subtype)
{
    df::world_raws &raws = world->raws;
    for (size_t i = 0; i < raws.matgloss.wood.size(); i++)
    {
        auto p = raws.matgloss.wood[i];
        if (p->id == subtype)
            return decode(type, i);
    }
    return decode(type, -1);
}

bool MaterialInfo::findStone(df::material_type type, const std::string &subtype)
{
    df::world_raws &raws = world->raws;
    for (size_t i = 0; i < raws.matgloss.stone.size(); i++)
    {
        auto p = raws.matgloss.stone[i];
        if (p->id == subtype)
            return decode(type, i);
    }
    return decode(type, -1);
}

bool MaterialInfo::findGem(df::material_type type, const std::string &subtype)
{
    df::world_raws &raws = world->raws;
    for (size_t i = 0; i < raws.matgloss.gem.size(); i++)
    {
        auto p = raws.matgloss.gem[i];
        if (p->id == subtype)
            return decode(type, i);
    }
    return decode(type, -1);
}

bool MaterialInfo::findPlant(df::material_type type, const std::string &subtype)
{
    df::world_raws &raws = world->raws;
    for (size_t i = 0; i < raws.matgloss.plant.size(); i++)
    {
        auto p = raws.matgloss.plant[i];
        if (p->id == subtype)
            return decode(type, i);
    }
    return decode(type, -1);
}

bool MaterialInfo::findCreature(df::material_type type, const std::string &subtype)
{
    df::world_raws &raws = world->raws;
    for (size_t i = 0; i < raws.creatures.size(); i++)
    {
        auto p = raws.creatures[i];
        if (p->creature_id == subtype)
            return decode(type, i);
    }
    return decode(type, -1);
}

std::string MaterialInfo::getToken()
{
    if (isNone())
        return "NONE";
    if (!isValid())
        return stl_sprintf("INVALID:%d:%d", type, subtype);

    string out = ENUM_KEY_STR(material_type,type) + ":";

    switch (mode) {
    case Builtin:
        if (type == material_type::COAL) {
            if (subtype == 0)
                out += "COKE";
            else if (subtype == 1)
                out += "CHARCOAL";
            else
                out += "NONE";
        }
        else
            out += "NONE";
        break;
    case Wood:
        out += wood->id;
        break;
    case Stone:
        out += stone->id;
        break;
    case Gem:
        out += gem->id;
        break;
    case Plant:
        out += plant->id;
        break;
    case Creature:
        out += creature->creature_id;
        break;
    default:
        out = stl_sprintf("INVALID_MODE:%d:%d", type, subtype);
        break;
    }
    return out;
}

std::string MaterialInfo::toString(uint16_t temp)
{
    // TODO - take temperature into account
    return getMaterialDescription(type, subtype);
}

bool DFHack::isSoilInorganic(int material)
{
    return false;
}

bool DFHack::isStoneInorganic(int material)
{
    return true;
}

Module* DFHack::createMaterials()
{
    return new Materials();
}


Materials::Materials()
{
}

Materials::~Materials()
{
}

bool Materials::Finish()
{
    return true;
}

t_matgloss::t_matgloss()
{
    fore    = 0;
    back    = 0;
    bright  = 0;

    value        = 0;
    wall_tile    = 0;
}

bool Materials::CopyInorganicMaterials (std::vector<t_matgloss> & stone)
{
    size_t size = world->raws.matgloss.stone.size();
    stone.clear();
    stone.reserve (size);
    for (size_t i = 0; i < size;i++)
    {
        df::matgloss_stone *orig = world->raws.matgloss.stone[i];
        t_matgloss mat;
        mat.id = orig->id;
        mat.name = orig->name;

        mat.value = 1;
        if (orig->flags.bits.LIGHT || orig->flags.bits.DARK)
          mat.value = 2;
        if (orig->flags.bits.SHARP)
          mat.value = 3;
        mat.wall_tile = orig->tile;
        mat.fore = orig->color[0];
        mat.back = orig->color[1];
        mat.bright = orig->color[2];
        stone.push_back(mat);
    }
    return true;
}

bool Materials::CopyWoodMaterials (std::vector<t_matgloss> & tree)
{
    size_t size = world->raws.matgloss.wood.size();
    tree.clear();
    tree.reserve (size);
    for (size_t i = 0; i < size;i++)
    {
        t_matgloss mat;
        mat.id = world->raws.matgloss.wood[i]->id;
        tree.push_back(mat);
    }
    return true;
}

bool Materials::CopyPlantMaterials (std::vector<t_matgloss> & plant)
{
    size_t size = world->raws.matgloss.plant.size();
    plant.clear();
    plant.reserve (size);
    for (size_t i = 0; i < size;i++)
    {
        t_matgloss mat;
        mat.id = world->raws.matgloss.plant[i]->id;
        plant.push_back(mat);
    }
    return true;
}

bool Materials::ReadCreatureTypes (void)
{
    size_t size = world->raws.creatures.size();
    race.clear();
    race.reserve (size);
    for (size_t i = 0; i < size;i++)
    {
        t_matgloss mat;
        mat.id = world->raws.creatures[i]->creature_id;
        race.push_back(mat);
    }
    return true;
}

bool Materials::ReadDescriptorColors (void)
{
    size_t size = world->raws.descriptors.colors.size();

    color.clear();
    if(size == 0)
        return false;
    color.reserve(size);
    for (size_t i = 0; i < size;i++)
    {
        df::descriptor_color *c = world->raws.descriptors.colors[i];
        t_descriptor_color col;
        col.id = c->id;
        col.name = c->name;
        col.red = c->red;
        col.green = c->green;
        col.blue = c->blue;
        color.push_back(col);
    }
    return true;
}

bool Materials::ReadCreatureTypesEx (void)
{
    size_t size = world->raws.creatures.size();
    raceEx.clear();
    raceEx.reserve (size);
    for (size_t i = 0; i < size; i++)
    {
        df::creature_raw *cr = world->raws.creatures[i];
        t_creaturetype mat;
        mat.id = cr->creature_id;
        mat.tile_character = cr->tile;
        mat.tilecolor.fore = cr->color[0];
        mat.tilecolor.back = cr->color[1];
        mat.tilecolor.bright = cr->color[2];
    }
    return true;
}

bool Materials::ReadAllMaterials(void)
{
    bool ok = true;
    ok &= this->ReadCreatureTypes();
    ok &= this->ReadCreatureTypesEx();
    ok &= this->ReadDescriptorColors();
    return ok;
}

std::string DFHack::getMaterialDescription(t_materialType material, t_materialSubtype matgloss)
{
    auto raws = df::global::world->raws;
    string out;
    switch (material)
    {
    case material_type::WOOD:
        if (matgloss >= 0 && matgloss < raws.matgloss.wood.size())
            out = raws.matgloss.wood[matgloss]->name;
        else
            out = "wood";
        break;
    case material_type::STONE_GRAY:
        if (matgloss >= 0 && matgloss < raws.matgloss.stone.size())
            out = raws.matgloss.stone[matgloss]->name;
        else
            out = "rock";
        break;
    case material_type::STONE_LIGHT:
        if (matgloss >= 0 && matgloss < raws.matgloss.stone.size())
            out = raws.matgloss.stone[matgloss]->name;
        else
            out = "light stone";
        break;
    case material_type::STONE_DARK:
        if (matgloss >= 0 && matgloss < raws.matgloss.stone.size())
            out = raws.matgloss.stone[matgloss]->name;
        else
            out = "dark stone";
        break;
    case material_type::GOLD:
        out = "gold";
        break;
    case material_type::IRON:
        out = "iron";
        break;
    case material_type::SILVER:
        out = "silver";
        break;
    case material_type::COPPER:
        out = "copper";
        break;
    case material_type::GEM_ORNAMENTAL:
        if (matgloss >= 0 && matgloss < raws.matgloss.gem.size())
            out = raws.matgloss.gem[matgloss]->name[0];
        else
            out = "ornamental gem";
        break;
    case material_type::GEM_SEMI:
        if (matgloss >= 0 && matgloss < raws.matgloss.gem.size())
            out = raws.matgloss.gem[matgloss]->name[0];
        else
            out = "semi-precious gem";
        break;
    case material_type::GEM_PRECIOUS:
        if (matgloss >= 0 && matgloss < raws.matgloss.gem.size())
            out = raws.matgloss.gem[matgloss]->name[0];
        else
            out = "precious gem";
        break;
    case material_type::GEM_RARE:
        if (matgloss >= 0 && matgloss < raws.matgloss.gem.size())
            out = raws.matgloss.gem[matgloss]->name[0];
        else
            out = "rare gem";
        break;
    case material_type::BONE:
        if (matgloss >= 0 && matgloss < raws.creatures.size())
            out = raws.creatures[matgloss]->name[0] + " ";
        out += "bone";
        break;
    case material_type::IVORY:
        if (matgloss >= 0 && matgloss < raws.creatures.size())
            out = raws.creatures[matgloss]->name[0] + " ";
        out += "ivory";
        break;
    case material_type::JADE:
        out = "jade";
        break;
    case material_type::HORN:
        if (matgloss >= 0 && matgloss < raws.creatures.size())
            out = raws.creatures[matgloss]->name[0] + " ";
        out += "horn";
        break;
    case material_type::AMBER:
        out = "amber";
        break;
    case material_type::CORAL:
        out = "coral";
        break;
    case material_type::PEARL:
        if (matgloss >= 0 && matgloss < raws.creatures.size())
            out = raws.creatures[matgloss]->name[0] + " ";
        out += "pearl";
        break;
    case material_type::SHELL:
        if (matgloss >= 0 && matgloss < raws.creatures.size())
            out = raws.creatures[matgloss]->name[0] + " ";
        out += "shell";
        break;
    case material_type::LEATHER:
        if (matgloss >= 0 && matgloss < raws.creatures.size())
            out = raws.creatures[matgloss]->name[0] + " ";
        out += "leather";
        break;
    case material_type::ADAMANTINE:
        out = "adamantine";
        break;
    case material_type::SILK:
        if (matgloss >= 0 && matgloss < raws.creatures.size())
            out = raws.creatures[matgloss]->name[0] + " ";
        out += "silk";
        break;
    case material_type::PLANT:
        if (matgloss >= 0 && matgloss < raws.matgloss.plant.size())
            out = raws.matgloss.plant[matgloss]->name;
        else
            out = "plant";
        break;
    case material_type::GLASS_GREEN:
        out = "green glass";
        break;
    case material_type::GLASS_CLEAR:
        out = "clear glass";
        break;
    case material_type::GLASS_CRYSTAL:
        out = "crystal glass";
        break;
    case material_type::SAND:
        out = "sand";
        break;
    case material_type::WATER:
        out = "water";
        break;
    case material_type::ZINC:
        out = "zinc";
        break;
    case material_type::TIN:
        out = "tin";
        break;
    case material_type::COAL:
        if (matgloss == 0)
            out = "charcoal";
        else if (matgloss == 1)
            out = "coke";
        else
            out = "coal";
        break;
    case material_type::BRONZE:
        out = "bronze";
        break;
    case material_type::BRASS:
        out = "brass";
        break;
    case material_type::STEEL:
        out = "steel";
        break;
    case material_type::PIGIRON:
        out = "pig iron";
        break;
    case material_type::PLATINUM:
        out = "platinum";
        break;
    case material_type::ELECTRUM:
        out = "electrum";
        break;
    case material_type::POTASH:
        out = "potash";
        break;
    case material_type::ASH:
        out = "ash";
        break;
    case material_type::PEARLASH:
        out = "pearlash";
        break;
    case material_type::LYE:
        out = "lye";
        break;
    case material_type::RENDERED_FAT:
        if (matgloss >= 0 && matgloss < raws.creatures.size())
            out = raws.creatures[matgloss]->name[0] + " ";
        out += "tallow";
        break;
    case material_type::SOAP_ANIMAL:
        if (matgloss >= 0 && matgloss < raws.creatures.size())
            out = raws.creatures[matgloss]->name[0] + " ";
        out += "tallow soap";
        break;
    case material_type::FAT:
        if (matgloss >= 0 && matgloss < raws.creatures.size())
            out = raws.creatures[matgloss]->name[0] + " ";
        out += "fat";
        break;
    case material_type::MUD:
        out = "mud";
        break;
    case material_type::VOMIT:
        out = "vomit";
        break;
    case material_type::BLOOD:
        out = "blood";    // TODO - matgloss supposedly indicates color, so we can get "ichor", "pus", "goo", etc.
        break;
    case material_type::SLIME:
        out = "slime";
        break;
    default:
        out = "unknown material";
        break;
    }
    return out;
}

int DFHack::getMaterialValue(t_materialType material, t_materialSubtype matgloss)
{
    auto &raws = df::global::world->raws;
    switch (material)
    {
    case material_type::STONE_GRAY:
    case material_type::STONE_LIGHT:
    case material_type::STONE_DARK:
        if (matgloss >= 0 && matgloss < raws.matgloss.stone.size())
        {
            auto raw = raws.matgloss.stone[matgloss];
            if (raw->flags.bits.SHARP)
                return 3;
            if (raw->flags.bits.DARK || raw->flags.bits.LIGHT)
                return 2;
        }
        return 1;

    case material_type::GOLD:
    case material_type::STEEL:
        return 30;

    case material_type::IRON:
    case material_type::SILVER:
    case material_type::GLASS_CRYSTAL:
    case material_type::PIGIRON:
        return 10;

    case material_type::COPPER:
    case material_type::JADE:
    case material_type::AMBER:
    case material_type::CORAL:
    case material_type::GLASS_GREEN:
    case material_type::ZINC:
    case material_type::TIN:
    case material_type::COAL:
    case material_type::LYE:
        return 2;

    case material_type::GEM_ORNAMENTAL:
    case material_type::GEM_SEMI:
    case material_type::GEM_PRECIOUS:
    case material_type::GEM_RARE:
        if (matgloss >= 0 && matgloss < raws.matgloss.gem.size())
            return raws.matgloss.gem[matgloss]->value;
        return 0;

    case material_type::BONE:
    case material_type::HORN:
    case material_type::SHELL:
    case material_type::LEATHER:
    case material_type::RENDERED_FAT:
    case material_type::FAT:
        if (matgloss >= 0 && matgloss < raws.creatures.size())
            return raws.creatures[matgloss]->modvalue;
        return 0;

    case material_type::IVORY:
    case material_type::PEARL:
    case material_type::SILK:
        if (matgloss >= 0 && matgloss < raws.creatures.size())
            return 2 * raws.creatures[matgloss]->modvalue;
        return 0;

    case material_type::ADAMANTINE:
        return 300;

    case material_type::PLANT:
        if (matgloss >= 0 && matgloss < raws.matgloss.plant.size())
            return raws.matgloss.plant[matgloss]->value;
        return 0;

    case material_type::GLASS_CLEAR:
    case material_type::BRONZE:
        return 5;

    case material_type::BRASS:
        return 7;

    case material_type::PLATINUM:
        return 40;

    case material_type::ELECTRUM:
        return 20;

    case material_type::POTASH:
        return 3;

    case material_type::PEARLASH:
        return 4;

    case material_type::SOAP_ANIMAL:
        if (matgloss >= 0 && matgloss < raws.creatures.size())
            return 5 * raws.creatures[matgloss]->modvalue;
        return 0;
    }
    return 1;
}
