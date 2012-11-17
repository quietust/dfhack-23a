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
#include <sstream>
#include <vector>
#include <cstdio>
#include <map>
#include <set>
using namespace std;

#include "Types.h"
#include "VersionInfo.h"
#include "MemAccess.h"
#include "modules/Materials.h"
#include "modules/Items.h"
#include "modules/Units.h"
#include "modules/MapCache.h"
#include "ModuleFactory.h"
#include "Core.h"
#include "Error.h"
#include "MiscUtils.h"

#include "df/ui.h"
#include "df/world.h"
#include "df/item.h"
#include "df/building.h"
#include "df/building_actual.h"
#include "df/itemdef_weaponst.h"
#include "df/itemdef_trapcompst.h"
#include "df/itemdef_toyst.h"
#include "df/itemdef_instrumentst.h"
#include "df/itemdef_armorst.h"
#include "df/itemdef_ammost.h"
#include "df/itemdef_siegeammost.h"
#include "df/itemdef_glovesst.h"
#include "df/itemdef_shoesst.h"
#include "df/itemdef_shieldst.h"
#include "df/itemdef_helmst.h"
#include "df/itemdef_pantsst.h"
#include "df/itemdef_foodst.h"
#include "df/general_ref.h"
#include "df/general_ref_unit_itemownerst.h"
#include "df/general_ref_contains_itemst.h"
#include "df/general_ref_contained_in_itemst.h"
#include "df/general_ref_building_holderst.h"
#include "df/general_ref_projectile.h"
#include "df/viewscreen_itemst.h"
#include "df/vermin.h"
#include "df/proj_itemst.h"
#include "df/proj_list_link.h"

#include "df/unit_inventory_item.h"
#include "df/body_part_raw.h"
#include "df/unit.h"
#include "df/creature_raw.h"
#include "df/general_ref_unit_holderst.h"

using namespace DFHack;
using namespace df::enums;
using df::global::world;
using df::global::ui;
using df::global::ui_selected_unit;
using df::global::proj_next_id;

#define ITEMDEF_VECTORS \
    ITEM(WEAPON, weapons, itemdef_weaponst) \
    ITEM(TRAPCOMP, trapcomps, itemdef_trapcompst) \
    ITEM(TOY, toys, itemdef_toyst) \
    ITEM(INSTRUMENT, instruments, itemdef_instrumentst) \
    ITEM(ARMOR, armor, itemdef_armorst) \
    ITEM(AMMO, ammo, itemdef_ammost) \
    ITEM(SIEGEAMMO, siegeammo, itemdef_siegeammost) \
    ITEM(GLOVES, gloves, itemdef_glovesst) \
    ITEM(SHOES, shoes, itemdef_shoesst) \
    ITEM(SHIELD, shields, itemdef_shieldst) \
    ITEM(HELM, helms, itemdef_helmst) \
    ITEM(PANTS, pants, itemdef_pantsst) \
    ITEM(FOOD, food, itemdef_foodst)

int Items::getSubtypeCount(df::item_type itype)
{
    using namespace df::enums::item_type;

    df::world_raws::T_itemdefs &defs = df::global::world->raws.itemdefs;

    switch (itype) {
#define ITEM(type,vec,tclass) \
    case type: \
        return defs.vec.size();
ITEMDEF_VECTORS
#undef ITEM

    default:
        return -1;
    }
}

df::itemdef *Items::getSubtypeDef(df::item_type itype, int subtype)
{
    using namespace df::enums::item_type;

    df::world_raws::T_itemdefs &defs = df::global::world->raws.itemdefs;

    switch (itype) {
#define ITEM(type,vec,tclass) \
    case type: \
        return vector_get(defs.vec, subtype);
ITEMDEF_VECTORS
#undef ITEM

    default:
        return NULL;
    }
}

bool ItemTypeInfo::decode(df::item_type type_, int16_t subtype_)
{
    type = type_;
    subtype = subtype_;
    custom = Items::getSubtypeDef(type_, subtype_);

    return isValid();
}

bool ItemTypeInfo::decode(df::item *ptr)
{
    if (!ptr)
        return decode(item_type::NONE);
    else
        return decode(ptr->getType(), ptr->getSubtype());
}

std::string ItemTypeInfo::getToken()
{
    std::string rv = ENUM_KEY_STR(item_type, type);
    if (custom)
        rv += ":" + custom->id;
    else if (subtype != -1)
        rv += stl_sprintf(":%d", subtype);
    return rv;
}

std::string ItemTypeInfo::toString()
{
    using namespace df::enums::item_type;

    switch (type) {
#define ITEM(type,vec,tclass) \
    case type: \
        if (VIRTUAL_CAST_VAR(cv, df::tclass, custom)) \
            return cv->name;
ITEMDEF_VECTORS
#undef ITEM

    default:
        break;
    }

    const char *name = ENUM_ATTR(item_type, caption, type);
    if (name)
        return name;

    return toLower(ENUM_KEY_STR(item_type, type));
}

bool ItemTypeInfo::find(const std::string &token)
{
    using namespace df::enums::item_type;

    std::vector<std::string> items;
    split_string(&items, token, ":");

    type = NONE;
    subtype = -1;
    custom = NULL;

    if (items.size() < 1 || items.size() > 2)
        return false;

    if (items[0] == "NONE")
        return true;

    if (!find_enum_item(&type, items[0]))
        return false;
    if (type == NONE)
        return false;
    if (items.size() == 1)
        return true;

    df::world_raws::T_itemdefs &defs = df::global::world->raws.itemdefs;

    switch (type) {
#define ITEM(type,vec,tclass) \
    case type: \
        for (size_t i = 0; i < defs.vec.size(); i++) { \
            if (defs.vec[i]->id == items[1]) { \
                subtype = i; custom = defs.vec[i]; return true; \
            } \
        } \
        break;
ITEMDEF_VECTORS
#undef ITEM

    default:
        break;
    }

    return (subtype >= 0);
}

bool Items::isCreatureMaterial(df::item_type itype)
{
    return ENUM_ATTR(item_type, is_creature_mat, itype);
}
bool Items::isPlantMaterial(df::item_type itype)
{
    return ENUM_ATTR(item_type, is_plant_mat, itype);
}

df::item * Items::findItemByID(int32_t id)
{
    if (id < 0)
        return 0;
    return df::item::find(id);
}

bool Items::copyItem(df::item * itembase, DFHack::dfh_item &item)
{
    if(!itembase)
        return false;
    df::item * itreal = (df::item *) itembase;
    item.origin = itembase;
    item.x = itreal->pos.x;
    item.y = itreal->pos.y;
    item.z = itreal->pos.z;
    item.id = itreal->id;
    item.age = itreal->age;
    item.flags = itreal->flags;
    item.matdesc.item_type = itreal->getType();
    item.matdesc.item_subtype = itreal->getSubtype();
    item.matdesc.mat_type = itreal->getMaterial();
    item.matdesc.mat_subtype = itreal->getMatgloss();
    item.wear_level = itreal->getWear();
    item.quality = itreal->getQuality();
    item.quantity = itreal->getStackSize();
    return true;
}

df::general_ref *Items::getGeneralRef(df::item *item, df::general_ref_type type)
{
    CHECK_NULL_POINTER(item);

    return findRef(item->general_refs, type);
}

df::specific_ref *Items::getSpecificRef(df::item *item, df::specific_ref_type type)
{
    CHECK_NULL_POINTER(item);

    return findRef(item->specific_refs, type);
}

df::unit *Items::getOwner(df::item * item)
{
    auto ref = getGeneralRef(item, general_ref_type::UNIT_ITEMOWNER);

    return ref ? ref->getUnit() : NULL;
}

bool Items::setOwner(df::item *item, df::unit *unit)
{
    CHECK_NULL_POINTER(item);

    for (int i = item->general_refs.size()-1; i >= 0; i--)
    {
        df::general_ref *ref = item->general_refs[i];

        if (!strict_virtual_cast<df::general_ref_unit_itemownerst>(ref))
            continue;

        if (auto cur = ref->getUnit())
        {
            if (cur == unit)
                return true;

            erase_from_vector(cur->owned_items, item->id);
        }

        delete ref;
        vector_erase_at(item->general_refs, i);
    }

    item->flags.bits.owned = false;

    if (unit)
    {
        auto ref = df::allocate<df::general_ref_unit_itemownerst>();
        if (!ref)
            return false;

        item->flags.bits.owned = true;
        ref->unit_id = unit->id;

        insert_into_vector(unit->owned_items, item->id);
        item->general_refs.push_back(ref);
    }

    return true;
}

df::item *Items::getContainer(df::item * item)
{
    auto ref = getGeneralRef(item, general_ref_type::CONTAINED_IN_ITEM);

    return ref ? ref->getItem() : NULL;
}

void Items::getContainedItems(df::item *item, std::vector<df::item*> *items)
{
    CHECK_NULL_POINTER(item);

    items->clear();

    for (size_t i = 0; i < item->general_refs.size(); i++)
    {
        df::general_ref *ref = item->general_refs[i];
        if (ref->getType() != general_ref_type::CONTAINS_ITEM)
            continue;

        auto child = ref->getItem();
        if (child)
            items->push_back(child);
    }
}

df::building *Items::getHolderBuilding(df::item * item)
{
    auto ref = getGeneralRef(item, general_ref_type::BUILDING_HOLDER);

    return ref ? ref->getBuilding() : NULL;
}

df::unit *Items::getHolderUnit(df::item * item)
{
    auto ref = getGeneralRef(item, general_ref_type::UNIT_HOLDER);

    return ref ? ref->getUnit() : NULL;
}

df::coord Items::getPosition(df::item *item)
{
    CHECK_NULL_POINTER(item);

    /* Function reverse-engineered from DF code. */

    if (item->flags.bits.removed)
        return df::coord();

    if (item->flags.bits.in_inventory)
    {
        for (size_t i = 0; i < item->general_refs.size(); i++)
        {
            df::general_ref *ref = item->general_refs[i];

            switch (ref->getType())
            {
            case general_ref_type::CONTAINED_IN_ITEM:
                if (auto item2 = ref->getItem())
                    return getPosition(item2);
                break;

            case general_ref_type::UNIT_HOLDER:
                if (auto unit = ref->getUnit())
                    return Units::getPosition(unit);
                break;

            /*case general_ref_type::BUILDING_HOLDER:
                if (auto bld = ref->getBuilding())
                    return df::coord(bld->centerx, bld->centery, bld->z);
                break;*/

            default:
                break;
            }
        }

        for (size_t i = 0; i < item->specific_refs.size(); i++)
        {
            df::specific_ref *ref = item->specific_refs[i];

            switch (ref->type)
            {
            case specific_ref_type::VERMIN_ESCAPED_PET:
                return ref->vermin->pos;

            default:
                break;
            }
        }

        return df::coord();
    }

    return item->pos;
}

static char quality_table[] = { 0, '-', '+', '*', '=', '@' };

static void addQuality(stl::string &tmp, int quality)
{
    if (quality > 0 && quality <= 5) {
        char c = quality_table[quality];
        tmp = c + tmp + c;
    }
}

std::string Items::getDescription(df::item *item, int type, bool decorate)
{
    CHECK_NULL_POINTER(item);

    stl::string tmp;
    item->getItemDescription(&tmp, type);

    if (decorate) {
        if (item->flags.bits.foreign)
            tmp = "(" + tmp + ")";

        addQuality(tmp, item->getQuality());

        if (item->isImproved()) {
            tmp = "<" + tmp + ">";
            addQuality(tmp, item->getImprovementQuality());
        }
    }

    return tmp;
}

static void resetUnitInvFlags(df::unit *unit, df::unit_inventory_item *inv_item)
{
    if (inv_item->mode == df::unit_inventory_item::Worn)
    {
        unit->flags2.bits.calculated_inventory = false;
        unit->flags2.bits.calculated_insulation = false;
    }
}

static bool detachItem(MapExtras::MapCache &mc, df::item *item)
{
    if (!item->specific_refs.empty())
        return false;

    for (size_t i = 0; i < item->general_refs.size(); i++)
    {
        df::general_ref *ref = item->general_refs[i];

        switch (ref->getType())
        {
        case general_ref_type::PROJECTILE:
        case general_ref_type::BUILDING_HOLDER:
        case general_ref_type::BUILDING_CAGED:
        case general_ref_type::BUILDING_TRIGGER:
        case general_ref_type::BUILDING_TRIGGERTARGET:
        case general_ref_type::BUILDING_CIVZONE_ASSIGNED:
            return false;

        default:
            continue;
        }
    }

    if (item->flags.bits.on_ground)
    {
        if (!mc.removeItemOnGround(item))
            Core::printerr("Item was marked on_ground, but not in block: %d (%d,%d,%d)\n",
                           item->id, item->pos.x, item->pos.y, item->pos.z);

        item->flags.bits.on_ground = false;
        return true;
    }
    else if (item->flags.bits.in_inventory)
    {
        bool found = false;

        for (int i = item->general_refs.size()-1; i >= 0; i--)
        {
            df::general_ref *ref = item->general_refs[i];

            switch (ref->getType())
            {
            case general_ref_type::CONTAINED_IN_ITEM:
                if (auto item2 = ref->getItem())
                {
                    // Viewscreens hold general_ref_contains_itemst pointers
                    for (auto screen = Core::getTopViewscreen(); screen; screen = screen->parent)
                    {
                        auto vsitem = strict_virtual_cast<df::viewscreen_itemst>(screen);
                        if (vsitem && vsitem->item == item2)
                            return false;
                    }

                    removeRef(item2->general_refs, general_ref_type::CONTAINS_ITEM, item->id);
                }
                break;

            case general_ref_type::UNIT_HOLDER:
                if (auto unit = ref->getUnit())
                {
                    // Unit view sidebar holds inventory item pointers
                    if (ui->main.mode == ui_sidebar_mode::ViewUnits &&
                        (!ui_selected_unit ||
                         vector_get(world->units.active, *ui_selected_unit) == unit))
                        return false;

                    for (int i = unit->inventory.size()-1; i >= 0; i--)
                    {
                        df::unit_inventory_item *inv_item = unit->inventory[i];
                        if (inv_item->item != item)
                            continue;

                        resetUnitInvFlags(unit, inv_item);

                        vector_erase_at(unit->inventory, i);
                        delete inv_item;
                    }
                }
                break;

            default:
                continue;
            }

            found = true;
            vector_erase_at(item->general_refs, i);
            delete ref;
        }

        if (!found)
            return false;

        item->flags.bits.in_inventory = false;
        return true;
    }
    else if (item->flags.bits.removed)
    {
        item->flags.bits.removed = false;

        if (item->flags.bits.garbage_collect)
        {
            item->flags.bits.garbage_collect = false;
            // TODO - this isn't a vmethod, so we need a hook for it
            Items::item_categorize(item,true);
        }

        return true;
    }
    else
        return false;
}

static void putOnGround(MapExtras::MapCache &mc, df::item *item, df::coord pos)
{
    item->pos = pos;
    item->flags.bits.on_ground = true;

    if (!mc.addItemOnGround(item))
        Core::printerr("Could not add item %d to ground at (%d,%d,%d)\n",
                       item->id, pos.x, pos.y, pos.z);
}

bool DFHack::Items::moveToGround(MapExtras::MapCache &mc, df::item *item, df::coord pos)
{
    CHECK_NULL_POINTER(item);

    if (!detachItem(mc, item))
        return false;

    putOnGround(mc, item, pos);
    return true;
}

bool DFHack::Items::moveToContainer(MapExtras::MapCache &mc, df::item *item, df::item *container)
{
    CHECK_NULL_POINTER(item);
    CHECK_NULL_POINTER(container);

    auto cpos = getPosition(container);
    if (!cpos.isValid())
        return false;

    auto ref1 = df::allocate<df::general_ref_contains_itemst>();
    auto ref2 = df::allocate<df::general_ref_contained_in_itemst>();

    if (!ref1 || !ref2)
    {
        delete ref1; delete ref2;
        Core::printerr("Could not allocate container refs.\n");
        return false;
    }

    if (!detachItem(mc, item))
    {
        delete ref1; delete ref2;
        return false;
    }

    item->pos = container->pos;
    item->flags.bits.in_inventory = true;

    container->flags.bits.container = true;

    ref1->item_id = item->id;
    container->general_refs.push_back(ref1);

    ref2->item_id = container->id;
    item->general_refs.push_back(ref2);

    return true;
}

bool DFHack::Items::moveToBuilding(MapExtras::MapCache &mc, df::item *item, df::building_actual *building,int16_t use_mode)
{
    CHECK_NULL_POINTER(item);
    CHECK_NULL_POINTER(building);
    CHECK_INVALID_ARGUMENT(use_mode == 0 || use_mode == 2);

    auto ref = df::allocate<df::general_ref_building_holderst>();
    if(!ref)
    {
        delete ref;
        Core::printerr("Could not allocate building holder refs.\n");
        return false;
    }

    if (!detachItem(mc, item))
    {
        delete ref;
        return false;
    }

    item->pos.x=building->centerx;
    item->pos.y=building->centery;
    item->pos.z=building->z;
    item->flags.bits.in_building=true;

    ref->building_id=building->id;
    item->general_refs.push_back(ref);

    auto con=new df::building_actual::T_contained_items;
    con->item=item;
    con->use_mode=use_mode;
    building->contained_items.push_back(con);

    return true;
}

bool DFHack::Items::moveToInventory(
    MapExtras::MapCache &mc, df::item *item, df::unit *unit,
    df::unit_inventory_item::T_mode mode, int body_part
) {
    CHECK_NULL_POINTER(item);
    CHECK_NULL_POINTER(unit);
    CHECK_NULL_POINTER(unit->body.body_plan);
    CHECK_INVALID_ARGUMENT(is_valid_enum_item(mode));
    int body_plan_size = unit->body.body_plan->body_parts.size();
    CHECK_INVALID_ARGUMENT(body_part < 0 || body_part <= body_plan_size);

    auto holderReference = df::allocate<df::general_ref_unit_holderst>();
    if (!holderReference)
    {
        Core::printerr("Could not allocate UNIT_HOLDER reference.\n");
        return false;
    }

    if (!detachItem(mc, item))
    {
        delete holderReference;
        return false;
    }

    item->flags.bits.in_inventory = true;

    auto newInventoryItem = new df::unit_inventory_item();
    newInventoryItem->item = item;
    newInventoryItem->mode = mode;
    newInventoryItem->body_part_id = body_part;
    unit->inventory.push_back(newInventoryItem);

    holderReference->unit_id = unit->id;
    item->general_refs.push_back(holderReference);

    resetUnitInvFlags(unit, newInventoryItem);

    return true;
}

bool Items::remove(MapExtras::MapCache &mc, df::item *item, bool no_uncat)
{
    CHECK_NULL_POINTER(item);

    auto pos = getPosition(item);

    if (!detachItem(mc, item))
        return false;

    if (pos.isValid())
        item->pos = pos;

    if (!no_uncat)
        item_uncategorize(item);
    item->flags.bits.removed = true;
    item->flags.bits.garbage_collect = !no_uncat;
    return true;
}

df::proj_itemst *Items::makeProjectile(MapExtras::MapCache &mc, df::item *item)
{
    CHECK_NULL_POINTER(item);

    if (!world || !proj_next_id)
        return NULL;

    auto pos = getPosition(item);
    if (!pos.isValid())
        return NULL;

    auto ref = df::allocate<df::general_ref_projectile>();
    if (!ref)
        return NULL;

    if (!detachItem(mc, item))
    {
        delete ref;
        return NULL;
    }

    item->pos = pos;
    item->flags.bits.in_job = true;

    auto proj = new df::proj_itemst();
    proj->link = new df::proj_list_link();
    proj->link->item = proj;
    proj->id = (*proj_next_id)++;
    df::coord32 _pos; _pos.x = pos.x; _pos.y = pos.y; _pos.z = pos.z;

    proj->origin_pos = proj->target_pos = _pos;
    proj->cur_pos = proj->prev_pos = _pos;
    proj->item = item;

    ref->projectile_id = proj->id;
    item->general_refs.push_back(ref);

    linked_list_append(&world->proj_list, proj->link);

    return proj;
}

void Items::item_categorize(df::item *item, bool in_play)
{
    static void *func_ptr = NULL;
    if (!func_ptr && !Core::getInstance().vinfo->getAddress("func_item_categorize", func_ptr))
        return; // should probably kill program

    __asm
    {
        movzx eax, in_play
        push eax
        mov edi, item
        mov eax, func_ptr
        call eax
    }
}
void Items::item_uncategorize(df::item *item)
{
    static void *func_ptr = NULL;
    if (!func_ptr && !Core::getInstance().vinfo->getAddress("func_item_uncategorize", func_ptr))
        return; // should probably kill program

    __asm
    {
        push ebx

        mov eax, item
        mov ebx, func_ptr
        call ebx

        pop ebx
    }
}
