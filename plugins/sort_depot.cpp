// Improve "Bring to Depot" screen - show item value, and allow sorting by value or distance

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <MiscUtils.h>
#include <modules/Gui.h>
#include <modules/Screen.h>
#include <modules/Maps.h>
#include <modules/Items.h>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

#include <VTableInterpose.h>
#include "df/world.h"
#include "df/item.h"
#include "df/viewscreen_assigntradest.h"
#include "df/assign_trade_status.h"
#include "df/building_tradedepotst.h"
#include "df/ui.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::gps;

void OutputString(int8_t fg, int &x, int y, const std::string &text)
{
    Screen::paintString(Screen::Pen(' ', fg, 0), x, y, text);
    x += text.length();
}

int getTotalItemValue (df::item *item)
{
    if (item->flags.bits.artifact)
        return 0;

    int val = Items::getValue(item);
    std::vector<df::item *> item_contents;
    Items::getContainedItems(item, &item_contents);
    for (size_t i = 0; i < item_contents.size(); i++)
        val += getTotalItemValue(item_contents[i]);

    return val;
}

int getDistance (df::coord pos1, df::coord pos2)
{
    return ((pos1.x > pos2.x) ? (pos1.x - pos2.x) : (pos2.x - pos1.x)) +
           ((pos1.y > pos2.y) ? (pos1.y - pos2.y) : (pos2.y - pos1.y)) +
           ((pos1.z > pos2.z) ? (pos1.z - pos2.z) : (pos2.z - pos1.z));
}

int getPathTag (df::coord pos)
{
    df::map_block *blk = Maps::getTileBlock(pos);
    if (!blk)
        return -1;
    return blk->walkable[pos.x % 16][pos.y % 16];
}

typedef std::pair<df::assign_trade_status *,int> trade_pair;
bool trade_pair_sort (const trade_pair &t1, const trade_pair &t2)
{
    return t1.second < t2.second;
}


bool inHook = false;
df::viewscreen_assigntradest *inHook_viewscreen = NULL;
struct assigntrade_hook : df::viewscreen_assigntradest
{
    typedef df::viewscreen_assigntradest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        bool have_broker = (ui->nobles_arrived[profession::BROKER] > ui->units_killed[profession::BROKER]);

        if (Screen::isKeyPressed(interface_key::STRING_V_CAP) && have_broker)
        {
            vector<trade_pair> values;

            df::coord depot_pos(depot->centerx, depot->centery, depot->z);
            int depot_tag = getPathTag(depot_pos);

            for (size_t i = 0; i < info.size(); i++)
            {
                int val = getTotalItemValue(info[i]->item);
                int item_tag = getPathTag(Items::getPosition(info[i]->item));

                if (info[i]->status == 3)
                    val += 2000000000; // items for trade go at the very top
                else if (info[i]->status > 0)
                    val += 1000000000; // items pending for trade are next
                if (item_tag != depot_tag)
                    val -= 100000000; // inaccessible items go at the bottom of each category

                values.push_back(trade_pair(info[i], -val)); // negative value so we sort descending
            }

            std::sort(values.begin(), values.end(), trade_pair_sort);
            info.clear();
            for (size_t i = 0; i < values.size(); i++)
                info.push_back(values[i].first);
        }
        if (Screen::isKeyPressed(interface_key::STRING_D_CAP))
        {
            vector<trade_pair> dists;

            df::coord depot_pos(depot->centerx, depot->centery, depot->z);
            int depot_tag = getPathTag(depot_pos);

            for (size_t i = 0; i < info.size(); i++)
            {
                df::coord item_pos = Items::getPosition(info[i]->item);
                int dist = getDistance(item_pos, depot_pos);
                int item_tag = getPathTag(item_pos);

                if (info[i]->status < 1)
                    dist += 2000000000; // unselected items go at the bottom
                else if (info[i]->status < 3)
                    dist += 1000000000; // items pending for trade are in the middle
                if (item_tag != depot_tag)
                    dist += 100000000; // inaccessible items go at the bottom of each category

                dists.push_back(trade_pair(info[i], dist));
            }

            std::sort(dists.begin(), dists.end(), trade_pair_sort);
            info.clear();
            for (size_t i = 0; i < dists.size(); i++)
                info.push_back(dists[i].first);
        }
        inHook = true;
        inHook_viewscreen = this;
        INTERPOSE_NEXT(view)();
    }
};

DFhackCExport command_result plugin_onrender ( color_ostream &out)
{
    int x;
    if (inHook)
    {
        bool have_broker = (ui->nobles_arrived[profession::BROKER] > ui->units_killed[profession::BROKER]);

        df::coord depot_pos(inHook_viewscreen->depot->centerx, inHook_viewscreen->depot->centery, inHook_viewscreen->depot->z);
        int depot_tag = getPathTag(depot_pos);

        int page_first = inHook_viewscreen->cursor - (inHook_viewscreen->cursor % 19);
        int page_len = inHook_viewscreen->info.size() - page_first;
        if (page_len > 19) page_len = 19;
        for (size_t i = 0; i < page_len; i++)
        {
            df::item *item = inHook_viewscreen->info[page_first + i]->item;
            df::coord item_pos = Items::getPosition(item);
            int dist = getDistance(item_pos, depot_pos);
            int item_tag = getPathTag(item_pos);
            int val = getTotalItemValue(item);

            x = 40;

            if (item_tag != depot_tag)
                OutputString(12, x, i + 2, "Inaccessible     ");
            else
                OutputString(10, x, i + 2, stl_sprintf("Distance: %-6i ", dist));

            OutputString(14, x, i + 2, have_broker ? stl_sprintf("%10i\x0F", val) : "           ");
        }

        x = 2;
        if (have_broker)
        {
            OutputString(12, x, 23, Screen::getKeyDisplay(interface_key::STRING_V_CAP));
            OutputString(15, x, 23, ": sort by value, ");
        }

        OutputString(12, x, 23, Screen::getKeyDisplay(interface_key::STRING_D_CAP));
        OutputString(15, x, 23, ": sort by distance from depot.");

        inHook = false;
        inHook_viewscreen = NULL;
    }
    return CR_OK;
}

IMPLEMENT_VMETHOD_INTERPOSE(assigntrade_hook, view);

DFHACK_PLUGIN("sort_depot");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

DFhackCExport command_result plugin_enable(color_ostream &out, bool enable)
{
    if (!gps)
        return CR_FAILURE;

    if (enable != is_enabled)
    {
        if (!INTERPOSE_HOOK(assigntrade_hook, view).apply(enable))
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
    INTERPOSE_HOOK(assigntrade_hook, view).remove();
    return CR_OK;
}
