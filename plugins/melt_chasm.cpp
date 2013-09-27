// Melt/Chasm shortcut keys from more places

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <MiscUtils.h>
#include <modules/Gui.h>
#include <modules/Screen.h>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

#include <VTableInterpose.h>
#include "df/world.h"
#include "df/ui.h"
#include "df/ui_look_list.h"
#include "df/init.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/viewscreen_itemst.h"
#include "df/interface_key.h"
#include "df/item.h"
#include "df/items_other_id.h"
#include "df/building_actual.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::gps;
using df::global::ui_look_list;
using df::global::ui_look_cursor;
using df::global::ui_building_item_cursor;

void OutputString(int8_t color, int &x, int y, const std::string &text)
{
    Screen::paintString(Screen::Pen(' ', color, 0), x, y, text);
    x += text.length();
}

bool isChasmable (df::item *item)
{
    if (item->flags.bits.artifact)
        return false;
    return true;
}

bool isMeltable (df::item *item)
{
    if (item->flags.bits.in_building)
        return false;
    if (item->flags.bits.owned)
        return false;
    if (item->flags.bits.removed)
        return false;
    if (item->flags.bits.construction)
        return false;
    if (item->flags.bits.artifact)
        return false;
    switch (item->getType())
    {
    case item_type::CORPSE:
    case item_type::CORPSEPIECE:
    case item_type::REMAINS:
    case item_type::MEAT:
    case item_type::FISH:
    case item_type::FISH_RAW:
    case item_type::VERMIN:
    case item_type::PET:
    case item_type::SEEDS:
    case item_type::PLANT:
    case item_type::SKIN_RAW:
    case item_type::BONES:
    case item_type::SHELL:
    case item_type::LEAVES:
    case item_type::EXTRACT:
    case item_type::POTION:
    case item_type::DRINK:
    case item_type::POWDER_ORGANIC:
    case item_type::CHEESE:
    case item_type::FOOD:
        return false;
    }
    switch (item->getMaterial())
    {
    case material_type::GOLD:
    case material_type::IRON:
    case material_type::SILVER:
    case material_type::COPPER:
    case material_type::ZINC:
    case material_type::TIN:
    case material_type::BRONZE:
    case material_type::BRASS:
    case material_type::STEEL:
    case material_type::PIGIRON:
    case material_type::PLATINUM:
    case material_type::ELECTRUM:
        return true;
    }
    return false;
}

void item_cancelMeltJob (df::item *item)
{
    // TODO: if there is an active "melt item" job for this item, cancel it
}

void item_cancelChasmJob (df::item *item)
{
    // TODO: if the item is considered to be "refuse", unmark it for melting
    // otherwise, cancel any active "chasm item" job
    // "Refuse" items are as follows:
    // * Items marked for chasming
    // * "Worn-out" items in appropriate categories matching standing orders for refuse dumping
    // * * "Worn-out" items include anything with wear level 3, rotten, or non-corpse.
}

bool item_isMelt (df::item *item)
{
    auto &vec = world->items.other[items_other_id::ANY_MELT];
    for (int i = 0; i < vec.size(); i++)
        if (vec[i] == item)
            return true;
    return false;
}

bool item_removeMelt (df::item *item)
{
    auto &vec = world->items.other[items_other_id::ANY_MELT];
    for (int i = 0; i < vec.size(); i++)
        if (vec[i] == item)
        {
            vec.erase(vec.begin() + i);
            item_cancelMeltJob(item);
            return true;
        }
    return false;
}

void item_addMelt (df::item *item)
{
    auto &vec = world->items.other[items_other_id::ANY_MELT];
    vec.push_back(item);
    item->flags.bits.dump = 0;
    item_cancelChasmJob(item);
}

bool inHook_dwarfmode_look = false;
bool inHook_dwarfmode_build = false;
struct dwarfmode_hook : df::viewscreen_dwarfmodest
{
    typedef df::viewscreen_dwarfmodest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        if (ui->economy_active)
        {
            df::item *item = NULL;
            if (ui->main.mode == ui_sidebar_mode::LookAround)
            {
                inHook_dwarfmode_look = true;
                if (*ui_look_cursor < ui_look_list->items.size())
                    if (ui_look_list->items[*ui_look_cursor]->type == df::ui_look_list::T_items::Item)
                        item = ui_look_list->items[*ui_look_cursor]->item;
            }
            if (ui->main.mode == ui_sidebar_mode::BuildingItems)
            {
                if (world->selected_building && world->selected_building->isActual())
                {
                    inHook_dwarfmode_build = true;
                    df::building_actual *bld = (df::building_actual *)world->selected_building;
                    if (*ui_building_item_cursor < bld->contained_items.size())
                        item = bld->contained_items[*ui_building_item_cursor]->item;
                }
            }
            if (item)
            {
                if (Screen::isKeyPressed(interface_key::STORES_MELT) && isMeltable(item))
                {
                    if (!item_removeMelt(item))
                        item_addMelt(item);
                }
                if (ui->activity_stats.found_chasm && Screen::isKeyPressed(interface_key::STORES_CHASM) && isChasmable(item))
                {
                    item->flags.bits.dump = !item->flags.bits.dump;
                    if (item->flags.bits.dump)
                        item_removeMelt(item);
                    else
                        item_cancelChasmJob(item);
                }
            }
        }
        INTERPOSE_NEXT(view)();
    }
};

bool inHook_item = false;
df::item *itemhook_item;
struct item_hook : df::viewscreen_itemst
{
    typedef df::viewscreen_itemst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        if (ui->economy_active)
        {
            inHook_item = true;
            itemhook_item = item;

            if (Screen::isKeyPressed(interface_key::STORES_MELT) && isMeltable(item))
            {
                if (!item_removeMelt(item))
                    item_addMelt(item);
            }
            if (ui->activity_stats.found_chasm && Screen::isKeyPressed(interface_key::STORES_CHASM) && isChasmable(item))
            {
                item->flags.bits.dump = !item->flags.bits.dump;
                if (item->flags.bits.dump)
                    item_removeMelt(item);
                else
                    item_cancelChasmJob(item);
            }
        }
        INTERPOSE_NEXT(view)();
    }
};

DFhackCExport command_result plugin_onrender ( color_ostream &out)
{
    auto dims = Gui::getDwarfmodeViewDims();
    int x, y;
    if (inHook_dwarfmode_look)
    {
        int start_item = *ui_look_cursor - *ui_look_cursor % 19;
        int num_items = ui_look_list->items.size();
        for (int i = 0; i < 19; i++)
        {
            y = 2 + i;
            int cur_item = start_item + i;
            if (cur_item >= num_items)
                break;
            if (ui_look_list->items[cur_item]->type != df::ui_look_list::T_items::Item)
                continue;
            df::item *item = ui_look_list->items[cur_item]->item;
            if (item_isMelt(item))
            {
                x = dims.menu_x2 - 2;
                OutputString(12, x, y, "M");
            }
            if (ui->activity_stats.found_chasm && item->flags.bits.dump)
            {
                x = dims.menu_x2 - 1;
                OutputString(8, x, y, "C");
            }
        }
        if (ui_look_list->items[*ui_look_cursor]->type == df::ui_look_list::T_items::Item)
        {
            df::item *item = ui_look_list->items[*ui_look_cursor]->item;
            if (ui->activity_stats.found_chasm)
            {
                x = dims.menu_x1 + 12;
                OutputString(10, x, 21, Screen::getKeyDisplay(interface_key::STORES_CHASM));
                OutputString(isChasmable(item) ? 15 : 8, x, 21, ": Chasm");
            }

            x = dims.menu_x1 + 22;
            y = 21;
            OutputString(10, x, y, Screen::getKeyDisplay(interface_key::STORES_MELT));
            OutputString(isMeltable(item) ? 15 : 8, x, y, ": Melt");
        }
        inHook_dwarfmode_look = false;
    }
    if (inHook_dwarfmode_build)
    {
        df::building_actual *bld = (df::building_actual *)world->selected_building;
        int start_item = *ui_building_item_cursor - *ui_building_item_cursor % 14;
        int num_items = bld->contained_items.size();
        for (int i = 0; i < 14; i++)
        {
            y = 4 + i;
            int cur_item = start_item + i;
            if (cur_item >= num_items)
                break;
            df::item *item = bld->contained_items[cur_item]->item;
            if (item_isMelt(item))
            {
                x = dims.menu_x2 - 5;
                OutputString(12, x, y, "M");
            }
            if (ui->activity_stats.found_chasm && item->flags.bits.dump)
            {
                x = dims.menu_x2 - 4;
                OutputString(8, x, y, "C");
            }
        }
        df::item *item = bld->contained_items[*ui_building_item_cursor]->item;
        if (ui->activity_stats.found_chasm)
        {
            x = dims.menu_x1 + 12;
            y = 19;
            OutputString(10, x, y, Screen::getKeyDisplay(interface_key::STORES_CHASM));
            OutputString(isChasmable(item) ? 15 : 8, x, y, ": Chasm");
        }

        x = dims.menu_x1 + 22;
        y = 19;
        OutputString(10, x, y, Screen::getKeyDisplay(interface_key::STORES_MELT));
        OutputString(isMeltable(item) ? 15 : 8, x, y, ": Melt");
        inHook_dwarfmode_build = false;
    }
    if (inHook_item)
    {
        df::item *item = itemhook_item;
        if (item_isMelt(item))
        {
            x = 76;
            y = 2;
            OutputString(12, x, y, "M");
        }
        if (ui->activity_stats.found_chasm && item->flags.bits.dump)
        {
            x = 77;
            y = 2;
            OutputString(8, x, y, "C");
        }

        if (isMeltable(item))
        {
            x = 55;
            y = 20;
            OutputString(10, x, y, Screen::getKeyDisplay(interface_key::STORES_MELT));
            OutputString(7, x, y, ": Melt");
        }
        if (ui->activity_stats.found_chasm && isChasmable(item))
        {
            x = 55;
            y = 21;
            OutputString(10, x, y, Screen::getKeyDisplay(interface_key::STORES_CHASM));
            OutputString(7, x, y, ": Chasm");
        }
        inHook_item = false;
    }
    return CR_OK;
}

IMPLEMENT_VMETHOD_INTERPOSE(dwarfmode_hook, view);
IMPLEMENT_VMETHOD_INTERPOSE(item_hook, view);

DFHACK_PLUGIN("melt_chasm");

DFhackCExport command_result plugin_init ( color_ostream &out, vector <PluginCommand> &commands)
{
    if (!gps || !INTERPOSE_HOOK(dwarfmode_hook, view).apply() || !INTERPOSE_HOOK(item_hook, view).apply())
        out.printerr("Could not insert Melt/Chasm hooks!\n");
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    INTERPOSE_HOOK(dwarfmode_hook, view).remove();
    INTERPOSE_HOOK(item_hook, view).remove();
    return CR_OK;
}
