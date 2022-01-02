// Improve "Bring to Depot" screen - show item value, and allow sorting by value or distance
// Also add a Search field for finding desired trade goods

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
#include "df/interfacest.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::gps;
using df::global::gview;

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

stl::vector<df::assign_trade_status *> all_trades;
std::string filter_string;
bool in_filter;

struct assigntrade_hook : df::viewscreen_assigntradest
{
    typedef df::viewscreen_assigntradest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        bool have_broker = (ui->nobles_arrived[profession::BROKER] > ui->units_killed[profession::BROKER]);

        if (!all_trades.size())
            all_trades = info;
        if (in_filter)
        {
            std::string filter = filter_string;
            if (Screen::isKeyPressed(interface_key::STRING_BACKSPACE)) { if (!filter_string.empty()) filter_string.pop_back(); }
            else if (Screen::isKeyPressed(interface_key::STRING_SPACE)) filter_string.push_back(' ');
            else if (Screen::isKeyPressed(interface_key::STRING_A) || Screen::isKeyPressed(interface_key::STRING_A_CAP)) filter_string.push_back('A');
            else if (Screen::isKeyPressed(interface_key::STRING_B) || Screen::isKeyPressed(interface_key::STRING_B_CAP)) filter_string.push_back('B');
            else if (Screen::isKeyPressed(interface_key::STRING_C) || Screen::isKeyPressed(interface_key::STRING_C_CAP)) filter_string.push_back('C');
            else if (Screen::isKeyPressed(interface_key::STRING_D) || Screen::isKeyPressed(interface_key::STRING_D_CAP)) filter_string.push_back('D');
            else if (Screen::isKeyPressed(interface_key::STRING_E) || Screen::isKeyPressed(interface_key::STRING_E_CAP)) filter_string.push_back('E');
            else if (Screen::isKeyPressed(interface_key::STRING_F) || Screen::isKeyPressed(interface_key::STRING_F_CAP)) filter_string.push_back('F');
            else if (Screen::isKeyPressed(interface_key::STRING_G) || Screen::isKeyPressed(interface_key::STRING_G_CAP)) filter_string.push_back('G');
            else if (Screen::isKeyPressed(interface_key::STRING_H) || Screen::isKeyPressed(interface_key::STRING_H_CAP)) filter_string.push_back('H');
            else if (Screen::isKeyPressed(interface_key::STRING_I) || Screen::isKeyPressed(interface_key::STRING_I_CAP)) filter_string.push_back('I');
            else if (Screen::isKeyPressed(interface_key::STRING_J) || Screen::isKeyPressed(interface_key::STRING_J_CAP)) filter_string.push_back('J');
            else if (Screen::isKeyPressed(interface_key::STRING_K) || Screen::isKeyPressed(interface_key::STRING_K_CAP)) filter_string.push_back('K');
            else if (Screen::isKeyPressed(interface_key::STRING_L) || Screen::isKeyPressed(interface_key::STRING_L_CAP)) filter_string.push_back('L');
            else if (Screen::isKeyPressed(interface_key::STRING_M) || Screen::isKeyPressed(interface_key::STRING_M_CAP)) filter_string.push_back('M');
            else if (Screen::isKeyPressed(interface_key::STRING_N) || Screen::isKeyPressed(interface_key::STRING_N_CAP)) filter_string.push_back('N');
            else if (Screen::isKeyPressed(interface_key::STRING_O) || Screen::isKeyPressed(interface_key::STRING_O_CAP)) filter_string.push_back('O');
            else if (Screen::isKeyPressed(interface_key::STRING_P) || Screen::isKeyPressed(interface_key::STRING_P_CAP)) filter_string.push_back('P');
            else if (Screen::isKeyPressed(interface_key::STRING_Q) || Screen::isKeyPressed(interface_key::STRING_Q_CAP)) filter_string.push_back('Q');
            else if (Screen::isKeyPressed(interface_key::STRING_R) || Screen::isKeyPressed(interface_key::STRING_R_CAP)) filter_string.push_back('R');
            else if (Screen::isKeyPressed(interface_key::STRING_S) || Screen::isKeyPressed(interface_key::STRING_S_CAP)) filter_string.push_back('S');
            else if (Screen::isKeyPressed(interface_key::STRING_T) || Screen::isKeyPressed(interface_key::STRING_T_CAP)) filter_string.push_back('T');
            else if (Screen::isKeyPressed(interface_key::STRING_U) || Screen::isKeyPressed(interface_key::STRING_U_CAP)) filter_string.push_back('U');
            else if (Screen::isKeyPressed(interface_key::STRING_V) || Screen::isKeyPressed(interface_key::STRING_V_CAP)) filter_string.push_back('V');
            else if (Screen::isKeyPressed(interface_key::STRING_W) || Screen::isKeyPressed(interface_key::STRING_W_CAP)) filter_string.push_back('W');
            else if (Screen::isKeyPressed(interface_key::STRING_X) || Screen::isKeyPressed(interface_key::STRING_X_CAP)) filter_string.push_back('X');
            else if (Screen::isKeyPressed(interface_key::STRING_Y) || Screen::isKeyPressed(interface_key::STRING_Y_CAP)) filter_string.push_back('Y');
            else if (Screen::isKeyPressed(interface_key::STRING_Z) || Screen::isKeyPressed(interface_key::STRING_Z_CAP)) filter_string.push_back('Z');
            else if (Screen::isKeyPressed(interface_key::SELECT)) in_filter = false;
            if (filter_string.length() > 30) { filter_string[29] = filter_string[30]; filter_string.pop_back(); }
            gview->current_key = 0;
            if (filter != filter_string)
            {
                filter = filter_string;
                std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
                info.clear();
                for (int i = 0; i < all_trades.size(); i++)
                {
                    stl::string _name;
                    all_trades[i]->item->getItemDescription(&_name, 0);
                    std::string name(_name);
                    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
                    if (name.find(filter) != std::string::npos)
                        info.push_back(all_trades[i]);
                }
                if (cursor > info.size())
                    cursor = info.size() - 1;
            }
        }
        if (Screen::isKeyPressed(interface_key::STRING_S))
            in_filter = true;
        if (Screen::isKeyPressed(interface_key::LEAVESCREEN))
        {
            info = all_trades;
            all_trades.clear();
            filter_string.clear();
        }
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
    if (inHook)
    {
        int x;
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

        x = 34;
        OutputString(12, x, 22, Screen::getKeyDisplay(interface_key::STRING_S));
        OutputString(in_filter ? 15 : 7, x, 22, ": Select " + filter_string + "_");

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
