// Name various containers according to their contents

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <MiscUtils.h>
#include <modules/Materials.h>
#include <modules/Items.h>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

#include <VTableInterpose.h>
#include "df/world.h"
#include "df/unit.h"
#include "df/creature_raw.h"
#include "df/item_cagest.h"
#include "df/item_flaskst.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::world;

struct cage_hook : df::item_cagest
{
    typedef df::item_cagest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, getItemDescription, (stl::string *str, int8_t mode))
    {
        std::set<int16_t> races;
        for (int i = 0; i < general_refs.size(); i++)
            if (general_refs[i]->getType() == general_ref_type::CONTAINS_UNIT)
                races.insert(general_refs[i]->getUnit()->race);
        if (races.size() == 0)
        {
            INTERPOSE_NEXT(getItemDescription)(str, mode);
            return;
        }
        if (races.size() == 1)
        {
            int r = *races.begin();
            if ((r >= 0) && (r <= world->raws.creatures.size()))
                str->append(world->raws.creatures[r]->name[0]);
        }
        else
            str->append("Shared");
        if (material == material_type::GLASS_GREEN || material == material_type::GLASS_CLEAR || material == material_type::GLASS_CRYSTAL)
        {
            bool aquarium = false;
            std::vector<df::item*> contents;
            Items::getContainedItems(this, &contents);
            for (int i = 0; i < contents.size(); i++)
                if (contents[i]->isLiquid())
                    aquarium = true;
            if (aquarium)
                str->append(" aquarium (");
            else
                str->append(" terrarium (");
        }
        else
            str->append(" cage (");
        str->append(getMaterialDescription(material, matgloss));
        str->append(")");
    }
};

struct flask_hook : df::item_flaskst
{
    typedef df::item_flaskst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, getItemDescription, (stl::string *str, int8_t mode))
    {
        std::vector<df::item*> contents;
        Items::getContainedItems(this, &contents);
        
        if (contents.size() == 0)
        {
            INTERPOSE_NEXT(getItemDescription)(str, mode);
            return;
        }
        if (contents.size() == 1)
        {
            if ((material == material_type::LEATHER) && (contents[0]->getMaterial() == material_type::WATER))
                str->append("Filled");
            else
                contents[0]->getItemDescription(str, 1);
        }
        else
            str->append("Mixed");

        if (material == material_type::LEATHER)
            str->append(" waterskin (");
        else if (material == material_type::GLASS_GREEN || material == material_type::GLASS_CLEAR || material == material_type::GLASS_CRYSTAL)
            str->append(" vial (");
        else
            str->append(" flask (");
        str->append(getMaterialDescription(material, matgloss));
        str->append(")");
    }
};

IMPLEMENT_VMETHOD_INTERPOSE(cage_hook, getItemDescription);
IMPLEMENT_VMETHOD_INTERPOSE(flask_hook, getItemDescription);

DFHACK_PLUGIN("container_names");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

DFhackCExport command_result plugin_enable(color_ostream &out, bool enable)
{
    if (enable != is_enabled)
    {
        if (!INTERPOSE_HOOK(cage_hook, getItemDescription).apply(enable))
            return CR_FAILURE;
        if (!INTERPOSE_HOOK(flask_hook, getItemDescription).apply(enable))
            return CR_FAILURE;

        is_enabled = enable;
    }

    return CR_OK;
}

DFhackCExport command_result plugin_init ( color_ostream &out, vector <PluginCommand> &commands)
{
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    INTERPOSE_HOOK(cage_hook, getItemDescription).remove();
    INTERPOSE_HOOK(flask_hook, getItemDescription).remove();
    return CR_OK;
}
