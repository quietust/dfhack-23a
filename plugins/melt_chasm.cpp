// Forbid/Melt/Chasm shortcut keys from more places
// Also allow bulk forbid/melt/chasm from the Stocks screen Category view

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
#include "df/viewscreen_storesst.h"
#include "df/interface_key.h"
#include "df/item.h"
#include "df/items_other_id.h"
#include "df/building_actual.h"
#include "df/job.h"
#include "df/job_item_ref.h"

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
    // Cancelling jobs is non-trivial, so this is harder than it sounds,
    // though we could probably just trigger a "job item misplaced" instead
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

void item_removeFromJobs (df::item *item)
{
    bool clear_job_flag = false;
    for (size_t i = 0; i < item->specific_refs.size(); i++)
    {
        if (item->specific_refs[i]->type != specific_ref_type::JOB)
            continue;
        df::job *job = item->specific_refs[i]->job;
        bool collapse = false;
        for (size_t j = 0; j < job->items.size(); j++)
        {
            if (job->items[j]->item != item)
                continue;
            if (job->items[j]->role == 2)
                collapse = true;
            if (job->items[j]->role != 6)
                clear_job_flag = true;
            delete job->items[j];
            job->items.erase(job->items.begin() + j--);
        }
        if (((job->job_type == job_type::ConstructBuilding) && (collapse)) || (job->job_type != job_type::ConstructBuilding))
            job->flags.bits.item_lost = 1;
        delete item->specific_refs[i];
        item->specific_refs.erase(item->specific_refs.begin() + i--);
    }
    if (clear_job_flag)
    {
        for (size_t i = 0; i < item->specific_refs.size(); i++)
            if (item->general_refs[i]->getType() == general_ref_type::PROJECTILE)
            {
                clear_job_flag = false;
                break;
            }
        if (clear_job_flag)
            item->flags.bits.in_job = 0;
    }
}

bool isBuildingValid (df::building *building)
{
    if (!building)
        return false;
    if (!building->isActual())
        return false;
    if (building->getBuildStage() < building->getMaxBuildStage())
        return false;
    for (size_t i = 0; i < building->jobs.size(); i++)
        if (building->jobs[i]->job_type == job_type::DestroyBuilding)
            return false;
    return true;
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
                if (isBuildingValid(world->selected_building))
                {
                    inHook_dwarfmode_build = true;
                    df::building_actual *bld = (df::building_actual *)world->selected_building;
                    if ((*ui_building_item_cursor >= 0) && (*ui_building_item_cursor < bld->contained_items.size()))
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
                if (ui->tasks.found_chasm && Screen::isKeyPressed(interface_key::STORES_CHASM) && isChasmable(item))
                {
                    item->flags.bits.dump = !item->flags.bits.dump;
                    if (item->flags.bits.dump)
                        item_removeMelt(item);
                    else
                        item_cancelChasmJob(item);
                }
                if (Screen::isKeyPressed(interface_key::STRING_F))
                {
                    item->flags.bits.forbid = !item->flags.bits.forbid;
                    if (item->flags.bits.forbid)
                        item_removeFromJobs(item);
                }
            }
        }
        INTERPOSE_NEXT(view)();
    }
};

bool inHook_item = false;
df::item *itemhook_item = NULL;
struct item_hook : df::viewscreen_itemst
{
    typedef df::viewscreen_itemst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        if (ui->economy_active)
        {
            inHook_item = true;
            itemhook_item = item;

            if (Screen::isKeyPressed(interface_key::STRING_F))
            {
                item->flags.bits.forbid = !item->flags.bits.forbid;
                if (item->flags.bits.forbid)
                    item_removeFromJobs(item);
            }
            if (Screen::isKeyPressed(interface_key::STORES_MELT) && isMeltable(item))
            {
                if (!item_removeMelt(item))
                    item_addMelt(item);
            }
            if (ui->tasks.found_chasm && Screen::isKeyPressed(interface_key::STORES_CHASM) && isChasmable(item))
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

bool inHook_stores = false;
df::viewscreen_storesst *storeshook_screen = NULL;
struct stores_hook : df::viewscreen_storesst
{
    typedef df::viewscreen_storesst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        inHook_stores = true;
        storeshook_screen = this;
        if (in_right_list && item_cursor >= 0 && item_cursor <= items.size())
        {
            if (!in_group_mode && Screen::isKeyPressed(interface_key::STRING_F))
            {
                df::item *item = items[item_cursor];

                item->flags.bits.forbid = !item->flags.bits.forbid;
                if (item->flags.bits.forbid)
                    item_removeFromJobs(item);
            }
            if (in_group_mode && (Screen::isKeyPressed(interface_key::STRING_F) || (ui->tasks.found_chasm && Screen::isKeyPressed(interface_key::STORES_CHASM)) || Screen::isKeyPressed(interface_key::STORES_MELT)))
            {
                bool set = true;
                for (int i = 0; i < items.size(); i++)
                {
                    auto item = items[i];
                    if ((item->getType() == group.item_type[item_cursor]) &&
                        (item->getSubtype() == group.item_subtype[item_cursor]) &&
                        (item->getMaterial() == group.material[item_cursor]) &&
                        (item->getMatgloss() == group.matgloss[item_cursor]))
                    {
                        if (Screen::isKeyPressed(interface_key::STRING_F) && item->flags.bits.forbid)
                            set = false;
                        if (ui->tasks.found_chasm && Screen::isKeyPressed(interface_key::STORES_CHASM) && item->flags.bits.dump)
                            set = false;
                        if (Screen::isKeyPressed(interface_key::STORES_MELT) && item_isMelt(item))
                            set = false;
                    }
                }
                for (int i = 0; i < items.size(); i++)
                {
                    auto item = items[i];
                    if ((item->getType() == group.item_type[item_cursor]) &&
                        (item->getSubtype() == group.item_subtype[item_cursor]) &&
                        (item->getMaterial() == group.material[item_cursor]) &&
                        (item->getMatgloss() == group.matgloss[item_cursor]))
                    {
                        if (Screen::isKeyPressed(interface_key::STRING_F))
                        {
                            if (item->flags.bits.forbid != set)
                                item_removeFromJobs(item);
                            item->flags.bits.forbid = set;
                        }
                        if (ui->tasks.found_chasm && Screen::isKeyPressed(interface_key::STORES_CHASM) && isChasmable(item))
                        {
                            if (item->flags.bits.dump != set)
                            {
                                if (set)
                                    item_removeMelt(item);
                                else
                                    item_cancelChasmJob(item);
                                item->flags.bits.dump = set;
                            }
                        }
                        if (Screen::isKeyPressed(interface_key::STORES_MELT) && isMeltable(item))
                        {
                            if ((item_isMelt(item) != set) && !item_removeMelt(item))
                                item_addMelt(item);
                        }
                    }
                }
            }
        }
        INTERPOSE_NEXT(view)();
    }
};

DFhackCExport command_result plugin_onrender ( color_ostream &out)
{
    if (inHook_dwarfmode_look)
    {
        auto dims = Gui::getDwarfmodeViewDims();
        int x, y;
        int start_item = *ui_look_cursor - *ui_look_cursor % 19;
        int num_items = ui_look_list->items.size();
        for (int i = 0; i < 19; i++)
        {
            y = 2 + i;
            int cur_item = start_item + i;
            if ((cur_item < 0) || (cur_item >= num_items))
                break;
            if (ui_look_list->items[cur_item]->type != df::ui_look_list::T_items::Item)
                continue;
            df::item *item = ui_look_list->items[cur_item]->item;
            if (item_isMelt(item))
            {
                x = dims.menu_x2 - 2;
                OutputString(12, x, y, "M");
            }
            if (ui->tasks.found_chasm && item->flags.bits.dump)
            {
                x = dims.menu_x2 - 1;
                OutputString(8, x, y, "C");
            }
        }
        if ((*ui_look_cursor >= 0) && (*ui_look_cursor < num_items) && (ui_look_list->items[*ui_look_cursor]->type == df::ui_look_list::T_items::Item))
        {
            df::item *item = ui_look_list->items[*ui_look_cursor]->item;

            x = dims.menu_x1 + 1;
            y = 21;
            OutputString(10, x, y, Screen::getKeyDisplay(interface_key::STRING_F));
            OutputString(15, x, y, item->flags.bits.forbid ? ": Claim " : ": Forbid");

            if (ui->tasks.found_chasm)
            {
                x = dims.menu_x1 + 12;
                y = 21;
                OutputString(10, x, y, Screen::getKeyDisplay(interface_key::STORES_CHASM));
                OutputString(isChasmable(item) ? 15 : 8, x, y, ": Chasm");
            }

            x = dims.menu_x1 + 22;
            y = 21;
            OutputString(10, x, y, Screen::getKeyDisplay(interface_key::STORES_MELT));
            OutputString(isMeltable(item) ? 15 : 8, x, y, ": Melt");
        }
        inHook_dwarfmode_look = false;
        return CR_OK;
    }
    if (inHook_dwarfmode_build)
    {
        auto dims = Gui::getDwarfmodeViewDims();
        int x, y;
        df::building_actual *bld = (df::building_actual *)world->selected_building;
        int start_item = *ui_building_item_cursor - *ui_building_item_cursor % 14;
        int num_items = bld->contained_items.size();
        for (int i = 0; i < 14; i++)
        {
            y = 4 + i;
            int cur_item = start_item + i;
            if ((cur_item < 0) || (cur_item >= num_items))
                break;
            df::item *item = bld->contained_items[cur_item]->item;
            if (item_isMelt(item))
            {
                x = dims.menu_x2 - 5;
                OutputString(12, x, y, "M");
            }
            if (ui->tasks.found_chasm && item->flags.bits.dump)
            {
                x = dims.menu_x2 - 4;
                OutputString(8, x, y, "C");
            }
        }
        if (*ui_building_item_cursor >= 0 && *ui_building_item_cursor < num_items)
        {
            df::item *item = bld->contained_items[*ui_building_item_cursor]->item;

            x = dims.menu_x1 + 1;
            y = 19;
            OutputString(10, x, y, Screen::getKeyDisplay(interface_key::STRING_F));
            OutputString(15, x, y, item->flags.bits.forbid ? ": Claim " : ": Forbid");

            if (ui->tasks.found_chasm)
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
        }
        inHook_dwarfmode_build = false;
        return CR_OK;
    }
    if (inHook_item)
    {
        int x, y;
        df::item *item = itemhook_item;
        if (item_isMelt(item))
        {
            x = 76;
            y = 2;
            OutputString(12, x, y, "M");
        }
        if (ui->tasks.found_chasm && item->flags.bits.dump)
        {
            x = 77;
            y = 2;
            OutputString(8, x, y, "C");
        }

        x = 55;
        y = 19;
        OutputString(10, x, y, Screen::getKeyDisplay(interface_key::STRING_F));
        OutputString(7, x, y, item->flags.bits.forbid ? ": Claim " : ": Forbid");

        if (isMeltable(item))
        {
            x = 55;
            y = 20;
            OutputString(10, x, y, Screen::getKeyDisplay(interface_key::STORES_MELT));
            OutputString(7, x, y, ": Melt");
        }
        if (ui->tasks.found_chasm && isChasmable(item))
        {
            x = 55;
            y = 21;
            OutputString(10, x, y, Screen::getKeyDisplay(interface_key::STORES_CHASM));
            OutputString(7, x, y, ": Chasm");
        }
        inHook_item = false;
        return CR_OK;
    }
    if (inHook_stores)
    {
        if (storeshook_screen->in_group_mode)
        {
            vector<int> num_melt, num_forbid, num_chasm;
            num_melt.resize(storeshook_screen->group.count.size());
            num_forbid.resize(storeshook_screen->group.count.size());
            num_chasm.resize(storeshook_screen->group.count.size());
            for (int i = 0; i < storeshook_screen->items.size(); i++)
            {
                if (!storeshook_screen->items[i])
                    continue;
                auto item = storeshook_screen->items[i];
                auto it = item->getType();
                auto is = item->getSubtype();
                auto mt = item->getMaterial();
                auto mg = item->getMatgloss();
                for (int j = 0; j < storeshook_screen->group.count.size(); j++)
                {
                    if ((storeshook_screen->group.item_type[j] == it) && (storeshook_screen->group.item_subtype[j] == is) && 
                        (storeshook_screen->group.material[j] == mt) && (storeshook_screen->group.matgloss[j] == mg))
                    {
                        num_melt[j] += item_isMelt(item) ? item->getStackSize() : 0;
                        num_forbid[j] += item->flags.bits.forbid ? item->getStackSize() : 0;
                        num_chasm[j] += item->flags.bits.dump ? item->getStackSize() : 0;
                    }
                }
            }

            int start_group = storeshook_screen->item_cursor - storeshook_screen->item_cursor % 19;
            int num_groups = storeshook_screen->group.count.size();
            for (int i = 0; i < 19; i++)
            {
                int y = 2 + i;
                int cur_group = start_group + i;
                if ((cur_group < 0) || (cur_group >= num_groups))
                    break;

                int x = 76;
                if (num_melt[cur_group] == storeshook_screen->group.count[cur_group])
                    OutputString(12, x, y, "M");
                else if (num_melt[cur_group] != 0)
                    OutputString(4, x, y, "M");

                x = 77;
                if (num_forbid[cur_group] == storeshook_screen->group.count[cur_group])
                    OutputString(14, x, y, "F");
                else if (num_forbid[cur_group] != 0)
                    OutputString(6, x, y, "F");

                x = 78;
                if (num_chasm[cur_group] == storeshook_screen->group.count[cur_group])
                    OutputString(8, x, y, "C");
                else if (num_chasm[cur_group] != 0)
                    OutputString(7, x, y, "C");
            }
        }

        int x = 66, y = 21;
        OutputString(12, x, y, Screen::getKeyDisplay(interface_key::STRING_F));

        int fg = storeshook_screen->in_right_list ? 15 : 8;
        OutputString(fg, x, y, ": Forbid");

        x = 67; y = 22;
        fg = storeshook_screen->in_right_list ? 15 : 8;
        OutputString(fg, x, y, ": Melt");

        x = 67; y = 23;
        fg = storeshook_screen->in_right_list ? 15 : 8;
        OutputString(fg, x, y, ": Chasm");

        inHook_stores = false;
        return CR_OK;
    }
    return CR_OK;
}

IMPLEMENT_VMETHOD_INTERPOSE(dwarfmode_hook, view);
IMPLEMENT_VMETHOD_INTERPOSE(item_hook, view);
IMPLEMENT_VMETHOD_INTERPOSE(stores_hook, view);

DFHACK_PLUGIN("melt_chasm");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

DFhackCExport command_result plugin_enable(color_ostream &out, bool enable)
{
    if (!gps)
        return CR_FAILURE;

    if (enable != is_enabled)
    {
        if (!INTERPOSE_HOOK(dwarfmode_hook, view).apply(enable))
            return CR_FAILURE;
        if (!INTERPOSE_HOOK(item_hook, view).apply(enable))
            return CR_FAILURE;
        if (!INTERPOSE_HOOK(stores_hook, view).apply(enable))
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
    INTERPOSE_HOOK(dwarfmode_hook, view).remove();
    INTERPOSE_HOOK(item_hook, view).remove();
    INTERPOSE_HOOK(stores_hook, view).remove();
    return CR_OK;
}
