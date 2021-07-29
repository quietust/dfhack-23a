// Various improvements to the Stocks screen
// - preserve cursor position in right pane when pressing Tab
// - color items which are part of buildings
// - fix item counts to take stack size and flags into account

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
#include "df/item.h"
#include "df/viewscreen_storesst.h"
#include "df/interfacest.h"
#include "df/graphic.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::gps;
using df::global::world;
using df::global::ui;
using df::global::ui_build_selector;
using df::global::gview;

typedef bool (__thiscall *itemFunc)(df::item *);

itemFunc item_isHiddenFromStocks = (itemFunc)0x5e97b0;
itemFunc item_isLocallyOwned = (itemFunc)0x5e97d0;

bool inHook = false;
df::viewscreen_storesst *inHook_viewscreen = NULL;
bool init_stores = true;
struct stocks_hook : df::viewscreen_storesst
{
    typedef df::viewscreen_storesst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        if (init_stores)
        {
            init_stores = false;
            for (int i = 0; i < category_total.size(); i++)
                category_total[i] = 0;
            for (int i = 0; i < category_busy.size(); i++)
                category_busy[i] = 0;
            for (int i = 0; i < world->items.other[0].size(); i++)
            {
                df::item *item = world->items.other[0][i];
                if (item_isHiddenFromStocks(item))
                    continue;
                if (item->flags.bits.in_building || !item_isLocallyOwned(item))
                    category_busy[item->getType()] += item->getStackSize();
                else
                    category_total[item->getType()] += item->getStackSize();
            }
        }

        int new_cursor = -1;
        if ((in_right_list) && (item_cursor >= 0) && (Screen::isKeyPressed(interface_key::CHANGETAB)))
        {
            if ((in_group_mode) && (item_cursor < group.item_type.size()))
            {
                for (int i = 0; i < items.size(); i++)
                {
                    if ((items[i]->getType() == group.item_type[item_cursor]) &&
                        (items[i]->getSubtype() == group.item_subtype[item_cursor]) &&
                        (items[i]->getMaterial() == group.material[item_cursor]) &&
                        (items[i]->getMatgloss() == group.matgloss[item_cursor]))
                    {
                        new_cursor = i;
                        break;
                    }
                }
            }
            if ((!in_group_mode) && (item_cursor < items.size()))
            {
                for (int i = 0; i < group.count.size(); i++)
                {
                    if ((items[item_cursor]->getType() == group.item_type[i]) &&
                        (items[item_cursor]->getSubtype() == group.item_subtype[i]) &&
                        (items[item_cursor]->getMaterial() == group.material[i]) &&
                        (items[item_cursor]->getMatgloss() == group.matgloss[i]))
                    {
                        new_cursor = i;
                        break;
                    }
                }
            }
        }
        inHook = true;
        inHook_viewscreen = this;
        INTERPOSE_NEXT(view)();
        if (breakdown_level != interface_breakdown_types::NONE)
        {
            init_stores = true;
            return;
        }
        if (new_cursor != -1)
            item_cursor = new_cursor;
    }
};

DFhackCExport command_result plugin_onrender ( color_ostream &out)
{
    if (inHook && inHook_viewscreen)
    {
        if (!inHook_viewscreen->in_group_mode)
        {
            int sel = inHook_viewscreen->item_cursor % 19;
            int top = inHook_viewscreen->item_cursor - sel;
            for (int i = 0; i < 19; i++)
            {
                if (top + i >= inHook_viewscreen->items.size())
                    break;
                if (!inHook_viewscreen->items[top+i]->flags.bits.in_building)
                    continue;
                if (inHook_viewscreen->items[top+i]->flags.bits.forbid)
                    continue;
                int y = i + 2;
                for (int x = 40; x < 80; x++)
                {
                    if ((x == 76) && (gps->screen[x][y].chr == 'M') && (gps->screen[x][y].fore == 4) && gps->screen[x][y].bright)
                        continue;
                    if ((x == 78) && (gps->screen[x][y].chr == 'C') && (gps->screen[x][y].fore == 0) && gps->screen[x][y].bright)
                        continue;
                    if ((x == 79) && (gps->screen[x][y].chr == '\xDB'))
                        continue;
                    gps->screen[x][y].fore = 3;
                }
            }
        }
        inHook = false;
        inHook_viewscreen = NULL;
    }
    return CR_OK;
}


IMPLEMENT_VMETHOD_INTERPOSE(stocks_hook, view);

DFHACK_PLUGIN("better_stocks");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

DFhackCExport command_result plugin_enable(color_ostream &out, bool enable)
{
    if (enable != is_enabled)
    {
        if (!INTERPOSE_HOOK(stocks_hook, view).apply(enable))
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
    INTERPOSE_HOOK(stocks_hook, view).remove();
    return CR_OK;
}
