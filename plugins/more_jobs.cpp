// Add some additional jobs at certain appropriate workshops
// Carpenter's Workshop - wooden blocks, wooden trap components
// Metalsmith's/Magma Forge - metal mechanisms
// [Magma] Glass Furnace - glass trap components
// Also extends Blocks stockpiles to allow selection of Wood

// Currently does not add these to the Job Manager search list

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <MiscUtils.h>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

#include <VTableInterpose.h>
#include "df/world.h"
#include "df/ui.h"
#include "df/ui_sidebar_menus.h"
#include "df/interface_button_building_new_jobst.h"
#include "df/building_workshopst.h"
#include "df/building_furnacest.h"
#include "df/interface_key.h"
#include "df/itemdef_trapcompst.h"
#include "df/historical_entity.h"
#include "df/viewscreen_layer_stockpilest.h"
#include "df/layer_object_listst.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::ui_sidebar_menus;

struct workshop_hook : df::building_workshopst
{
    typedef df::building_workshopst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, populateTaskList, ())
    {
        df::interface_button_building_new_jobst *button;

        INTERPOSE_NEXT(populateTaskList)();
        if (type == workshop_type::Carpenters)
        {
            button = df::allocate<df::interface_button_building_new_jobst>();

            button->hotkey_id = -1;
            button->is_hidden = false;
            button->building = this;
            button->job_type = job_type::ConstructBlocks;
            button->item_subtype = -1;
            button->material = material_type::WOOD;
            button->item_category.whole = 0;

            auto ins1 = ui_sidebar_menus->workshop_job.choices_all.begin();
            auto ins2 = ui_sidebar_menus->workshop_job.choices_visible.begin();
            while (ins1 != ui_sidebar_menus->workshop_job.choices_all.end())
            {
                if ((*ins1)->hotkey_id == interface_key::HOTKEY_CARPENTER_BUCKET)
                    break;
                ins1++;
            }
            while (ins2 != ui_sidebar_menus->workshop_job.choices_visible.end())
            {
                if ((*ins2)->hotkey_id == interface_key::HOTKEY_CARPENTER_BUCKET)
                    break;
                ins2++;
            }
            ui_sidebar_menus->workshop_job.choices_all.insert(ins1, button);
            ui_sidebar_menus->workshop_job.choices_visible.insert(ins2, button);

            df::historical_entity *entity = df::historical_entity::find(ui->civ_id);
            if (!entity)
                return;
            for (size_t i = 0; i < entity->resources.trapcomp_type.size(); i++)
            {
                df::itemdef_trapcompst *itemdef = df::itemdef_trapcompst::find(entity->resources.trapcomp_type[i]);
                if (!itemdef)
                    continue;

                // 23a didn't have material flags for trap components, so we need to filter by damage type
                if ((itemdef->damage_type != damage_type::BLUDGEON) && (itemdef->damage_type != damage_type::PIERCE))
                    continue;

                button = df::allocate<df::interface_button_building_new_jobst>();

                button->hotkey_id = -1;
                button->is_hidden = false;
                button->building = this;
                button->job_type = job_type::MakeTrapComponent;
                button->item_subtype = itemdef->subtype;
                button->material = material_type::WOOD;
                button->item_category.whole = 0;

                ui_sidebar_menus->workshop_job.choices_all.push_back(button);
                ui_sidebar_menus->workshop_job.choices_visible.push_back(button);
            }
        }
        // Forge, Trap Components section, non-Adamantine material selected
        if (((type == workshop_type::MetalsmithsForge) || (type == workshop_type::MagmaForge)) &&
            (ui_sidebar_menus->workshop_job.category_id == 4) &&
            (ui_sidebar_menus->workshop_job.material != -1) &&
            (ui_sidebar_menus->workshop_job.material != material_type::ADAMANTINE))
        {
            button = df::allocate<df::interface_button_building_new_jobst>();

            button->hotkey_id = -1;
            button->is_hidden = false;
            button->building = this;
            button->job_type = job_type::ConstructMechanisms;
            button->item_subtype = -1;
            button->material = ui_sidebar_menus->workshop_job.material;
            button->item_category.whole = 0;

            // last entry of choices_all is a material_selector, so we need to insert before it
            ui_sidebar_menus->workshop_job.choices_all.insert(ui_sidebar_menus->workshop_job.choices_all.end() - 1, button);
            ui_sidebar_menus->workshop_job.choices_visible.push_back(button);
        }
    }
};

struct furnace_hook : df::building_furnacest
{
    typedef df::building_furnacest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, populateTaskList, ())
    {
        df::interface_button_building_new_jobst *button;

        INTERPOSE_NEXT(populateTaskList)();

        // Glass furnaces, material selected
        if (((type == furnace_type::GlassFurnace) || (type == furnace_type::MagmaGlassFurnace)) &&
             (ui_sidebar_menus->workshop_job.material != -1))
        {
            df::historical_entity *entity = df::historical_entity::find(ui->civ_id);
            if (!entity)
                return;
            for (size_t i = 0; i < entity->resources.trapcomp_type.size(); i++)
            {
                df::itemdef_trapcompst *itemdef = df::itemdef_trapcompst::find(entity->resources.trapcomp_type[i]);
                if (!itemdef)
                    continue;

                button = df::allocate<df::interface_button_building_new_jobst>();

                button->hotkey_id = -1;
                button->is_hidden = false;
                button->building = this;
                button->job_type = job_type::MakeTrapComponent;
                button->item_subtype = itemdef->subtype;
                button->material = ui_sidebar_menus->workshop_job.material;
                button->item_category.whole = 0;

                // last entry of choices_all is a material_selector, so we need to insert before it
                ui_sidebar_menus->workshop_job.choices_all.insert(ui_sidebar_menus->workshop_job.choices_all.end() - 1, button);
                ui_sidebar_menus->workshop_job.choices_visible.push_back(button);
            }
        }
    }
};

struct stockpile_hook : df::viewscreen_layer_stockpilest
{
    typedef df::viewscreen_layer_stockpilest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        INTERPOSE_NEXT(view)();
        if (layer_objects.size() < 3)
            return;
        df::layer_object_listst *col1 = (df::layer_object_listst *)layer_objects[0];
        df::layer_object_listst *col2 = (df::layer_object_listst *)layer_objects[1];
        df::layer_object_listst *col3 = (df::layer_object_listst *)layer_objects[2];
        // Bars/Blocks
        if (col1->cursor != 9)
            return;
        // Blocks
        if (col2->cursor != 1)
            return;
        // Make sure it hasn't been updated already
        if (col3->num_entries != 16)
            return;
        col3->num_entries = 17;
        item_names.insert(item_names.begin(), new stl::string("wood"));
        item_status.insert(item_status.begin(), (bool *)&settings->bars_blocks.blocks_mats[material_type::WOOD]);
    }
};

IMPLEMENT_VMETHOD_INTERPOSE(workshop_hook, populateTaskList);
IMPLEMENT_VMETHOD_INTERPOSE(furnace_hook, populateTaskList);
IMPLEMENT_VMETHOD_INTERPOSE(stockpile_hook, view);

DFHACK_PLUGIN("more_jobs");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

DFhackCExport command_result plugin_enable(color_ostream &out, bool enable)
{
    if (enable != is_enabled)
    {
        if (!INTERPOSE_HOOK(workshop_hook, populateTaskList).apply(enable))
            return CR_FAILURE;
        if (!INTERPOSE_HOOK(furnace_hook, populateTaskList).apply(enable))
            return CR_FAILURE;
        if (!INTERPOSE_HOOK(stockpile_hook, view).apply(enable))
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
    INTERPOSE_HOOK(workshop_hook, populateTaskList).remove();
    INTERPOSE_HOOK(furnace_hook, populateTaskList).remove();
    INTERPOSE_HOOK(stockpile_hook, view).remove();
    return CR_OK;
}
