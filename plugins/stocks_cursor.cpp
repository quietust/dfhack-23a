// Preserve the cursor position on the Stocks screen when changing mode

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <MiscUtils.h>
#include <modules/Screen.h>
#include <modules/Translation.h>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

#include <VTableInterpose.h>
#include "df/world.h"
#include "df/item.h"
#include "df/viewscreen_storesst.h"
#include "df/interfacest.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::ui_build_selector;
using df::global::gview;

struct stocks_hook : df::viewscreen_storesst
{
    typedef df::viewscreen_storesst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
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
        INTERPOSE_NEXT(view)();
        if (new_cursor != -1)
            item_cursor = new_cursor;
    }
};

IMPLEMENT_VMETHOD_INTERPOSE(stocks_hook, view);

DFHACK_PLUGIN("stocks_cursor");
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
