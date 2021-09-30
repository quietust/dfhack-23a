// Display building icons in the Assign to Cage/Chain UI

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <MiscUtils.h>
#include <modules/Gui.h>
#include <modules/Units.h>
#include <modules/Screen.h>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

#include <VTableInterpose.h>
#include "df/world.h"
#include "df/ui.h"
#include "df/unit.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/building_actual.h"
#include "df/building_cagest.h"
#include "df/building_chainst.h"
#include "df/general_ref.h"
#include "df/buildings_other_id.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::gps;
using df::global::ui_building_in_assign;
using df::global::ui_building_item_cursor;
using df::global::ui_building_assign_units;

void OutputString(int8_t color, int &x, int y, const std::string &text)
{
    Screen::paintString(Screen::Pen(' ', color, 0), x, y, text);
    x += text.length();
}

const df::interface_key hotkey = interface_key::STRING_Z;

bool inHook_dwarfmode = false;

struct dwarfmode_hook : df::viewscreen_dwarfmodest
{
    typedef df::viewscreen_dwarfmodest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        inHook_dwarfmode = false;

        if (ui->main.mode == ui_sidebar_mode::QueryBuilding && world->selected_building && *ui_building_in_assign) do
        {
            // Only activate with Cages and Chains
            if ((world->selected_building->getType() == df::building_type::Chain) || (world->selected_building->getType() == df::building_type::Cage))
                inHook_dwarfmode = true;
        } while (0);
        INTERPOSE_NEXT(view)();
    }
};

df::building_chainst *getAssignedChain (df::unit *unit)
{
    for (int i = 0; i < world->buildings.other[df::buildings_other_id::CHAIN].size(); i++)
    {
        df::building *bld = world->buildings.other[df::buildings_other_id::CHAIN][i];
        if (bld->getType() == df::building_type::Chain)
        {
            df::building_chainst *chain = (df::building_chainst *)bld;
            if (chain->assigned == unit)
                return chain;
        }
    }
    return NULL;
}

df::building_cagest *getAssignedCage (df::unit *unit)
{
    for (int i = 0; i < world->buildings.other[df::buildings_other_id::CAGE].size(); i++)
    {
        df::building *bld = world->buildings.other[df::buildings_other_id::CAGE][i];
        if (bld->getType() == df::building_type::Cage)
        {
            df::building_cagest *cage = (df::building_cagest *)bld;
            for (int j = 0; j < cage->assigned_creature.size(); j++)
                if (cage->assigned_creature[j] == unit->id)
                    return cage;
        }
    }
    return NULL;
}

bool drawChainIcon (df::unit *unit, df::building_chainst *chain, int x, int y)
{
    if (unit)
    {
        df::general_ref *ref = Units::getGeneralRef(unit, df::general_ref_type::BUILDING_CHAIN);
        if (!ref)
            return false;

        df::building *bld = ref->getBuilding();
        if (!bld)
            return false;
        
        if (bld->getType() != df::building_type::Chain)
            return false;
        chain = (df::building_chainst *)bld;
    }
    else if (!chain)
        return false;

    if (!chain->contained_items.size())
        return false;

    df::item *item = chain->contained_items[0]->item;
    if (!item)
        return false;

    item->setDisplayColor(0);
    gps->screen[x][y].chr = item->getTile();
    gps->screen[x][y].fore = gps->screenf;
    gps->screen[x][y].back = gps->screenb;
    gps->screen[x][y].bright = gps->screenbright;
    return true;
}

bool drawCageIcon (df::unit *unit, df::building_cagest *cage, int x, int y)
{
    df::item *item = NULL;
    if (unit)
    {
        df::general_ref *ref = Units::getGeneralRef(unit, df::general_ref_type::CONTAINED_IN_ITEM);
        if (!ref)
            return false;

        item = ref->getItem();
    }
    else if (cage)
    {
        if (!cage->contained_items.size())
            return false;

        item = cage->contained_items[0]->item;
    }
    else
        return false;

    if (!item)
        return false;

    item->setDisplayColor(0);
    gps->screen[x][y].chr = item->getTile();
    gps->screen[x][y].fore = gps->screenf;
    gps->screen[x][y].back = gps->screenb;
    gps->screen[x][y].bright = gps->screenbright;
    return true;
}

DFhackCExport command_result plugin_onrender ( color_ostream &out)
{
    if (inHook_dwarfmode)
    {
        auto dims = Gui::getDwarfmodeViewDims();

        int start_idx = *ui_building_item_cursor - *ui_building_item_cursor % 14;
        int num_assign = ui_building_assign_units->size();

        for (int i = 0; i < 14; i++)
        {
            int y = 4 + i;
            int cur_idx = start_idx + i;
            if ((cur_idx < 0) || (cur_idx >= num_assign))
                break;

            if (ui_building_assign_units->at(cur_idx) == NULL)
                continue;

            df::unit *unit = ui_building_assign_units->at(cur_idx);

            df::building_chainst *chain = getAssignedChain(unit);
            df::building_cagest *cage = getAssignedCage(unit);

            if (!unit->flags1.bits.caged && !unit->flags1.bits.chained && !chain && !cage)
                continue;

            int x = dims.menu_x2 - 3;
            if (unit->flags1.bits.caged || unit->flags1.bits.chained)
                OutputString(15, x, y, "[");
            else
                OutputString(8, x, y, "[");

            do
            {
                if (unit->flags1.bits.chained && drawChainIcon(unit, NULL, x, y))
                    break;

                if (unit->flags1.bits.caged && drawCageIcon(unit, NULL, x, y))
                    break;

                if (chain && drawChainIcon(NULL, chain, x, y))
                    break;

                if (cage && drawCageIcon(NULL, cage, x, y))
                    break;

                Screen::paintTile(Screen::Pen('X', 4, 0, true), x, y);
            } while (0);
            x++;

            if (unit->flags1.bits.caged || unit->flags1.bits.chained)
                OutputString(15, x, y, "]");
            else
                OutputString(8, x, y, "]");

        }

        // All done with this hook
        inHook_dwarfmode = false;
    }
    return CR_OK;
}

IMPLEMENT_VMETHOD_INTERPOSE(dwarfmode_hook, view);

DFHACK_PLUGIN("assign_icon");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

DFhackCExport command_result plugin_enable(color_ostream &out, bool enable)
{
    if (!gps)
        return CR_FAILURE;

    if (enable != is_enabled)
    {
        if (!INTERPOSE_HOOK(dwarfmode_hook, view).apply(enable))
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
    return CR_OK;
}
