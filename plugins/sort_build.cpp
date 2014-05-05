// Improve building material selection menu - allow sorting by distance and finer control over building material

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
#include "df/viewscreen_dwarfmodest.h"
#include "df/ui_build_selector.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::ui_build_selector;
using df::global::gps;

void OutputString(int8_t fg, int &x, int y, const std::string &text)
{
    Screen::paintString(Screen::Pen(' ', fg, 0), x, y, text);
    x += text.length();
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

struct build_entry
{
    df::item *item;
    int8_t selected;
    int16_t quantity;
    int distance;
};

bool build_entry_sort (const build_entry &b1, const build_entry &b2)
{
    return b1.distance < b2.distance;
}

void moveScreen(int x, int y)
{
    using df::global::cursor;
    using df::global::window_x;
    using df::global::window_y;

    *window_x += x;
    cursor->x += x;

    *window_y += y;
    cursor->y += y;
}

bool inHook = false;
struct dwarfmode_hook : df::viewscreen_dwarfmodest
{
    typedef df::viewscreen_dwarfmodest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        if ((ui->main.mode == ui_sidebar_mode::Build) && (world->build_type.building_type != -1) &&
            (ui_build_selector->categories_total >= ui_build_selector->categories_required) &&
            (ui_build_selector->cur_category != -1))
        {
            if (Screen::isKeyPressed(interface_key::STRING_X))
            {
                vector<build_entry> items;
                df::coord cursor_pos = Gui::getCursorPos();
                int cursor_tag = getPathTag(cursor_pos);
                if (cursor_tag == -1)
                    cursor_tag--;

                for (size_t i = 0; i < ui_build_selector->visible.items.size(); i++)
                {
                    build_entry item;
                    item.item = ui_build_selector->visible.items[i];
                    item.selected = ui_build_selector->visible.selected[i];
                    item.quantity = ui_build_selector->visible.quantity[i];
                    if (item.item)
                    {
                        df::coord item_pos = Items::getPosition(item.item);
                        item.distance = getDistance(cursor_pos, item_pos);
                        if (getPathTag(item_pos) != cursor_tag)
                            item.distance += 1000000000;
                    }
                    else
                        item.distance = -1;
                    items.push_back(item);
                }
                auto begin = items.begin();
                auto end = items.end();
                // keep "Dirt" at the top, as well as selected items
                while ((begin != end) && ((begin->item == NULL) || (begin->selected)))
                    begin++;
                // move all selected items to the top, retaining their current order
                for (auto iter = begin; iter != end; iter++)
                {
                    if (iter->selected)
                    {
                        iter_swap(iter, begin);
                        begin++;
                    }
                }
                sort(begin, end, build_entry_sort);
                ui_build_selector->visible.items.clear();
                ui_build_selector->visible.selected.clear();
                ui_build_selector->visible.quantity.clear();
                for (auto iter = items.begin(); iter != items.end(); iter++)
                {
                    ui_build_selector->visible.items.push_back(iter->item);
                    ui_build_selector->visible.selected.push_back(iter->selected);
                    ui_build_selector->visible.quantity.push_back(iter->quantity);
                }
            }
            inHook = true;
        }
        INTERPOSE_NEXT(view)();
    }
};

DFhackCExport command_result plugin_onrender ( color_ostream &out)
{
    if (inHook)
    {
        int x;
        auto dims = Gui::getDwarfmodeViewDims();
        df::coord cursor_pos = Gui::getCursorPos();
        df::coord viewport_pos = Gui::getViewportPos();
        int cursor_tag = getPathTag(cursor_pos);
        if (cursor_tag == -1)
            cursor_tag--;

        Screen::paintString(Screen::Pen(' ', 14, 0), cursor_pos.x - viewport_pos.x + 1, cursor_pos.y - viewport_pos.y + 1, "X");

        x = dims.menu_x1 + 1;
        OutputString(15, x, 3, "Item");
        x = 73;
        OutputString(15, x, 3, "Dist");

        int page_first = ui_build_selector->cursor_position - (ui_build_selector->cursor_position % 14);
        int page_len = ui_build_selector->visible.items.size() - page_first;
        if (page_len > 14) page_len = 14;

        for (int i = 0; i < page_len; i++)
        {
            x = 73;
            if (ui_build_selector->visible.items[page_first + i])
            {
                df::coord item_pos = Items::getPosition(ui_build_selector->visible.items[page_first + i]);
                OutputString(getPathTag(item_pos) == cursor_tag ? 10 : 12, x, 4 + i, stl_sprintf("%-4i", getDistance(cursor_pos, item_pos)));
            }
        }

        x = dims.menu_x1 + 18;
        OutputString(12, x, 22, Screen::getKeyDisplay(interface_key::STRING_X));
        OutputString(15, x, 22, ": Sort");


            // When in build mode, change the normal cursor speed to 1 tile
            if (Screen::isKeyPressed(interface_key::CURSOR_UP) || Screen::isKeyPressed(interface_key::CURSOR_UPLEFT) || Screen::isKeyPressed(interface_key::CURSOR_UPRIGHT))
                moveScreen(0, 9);
            if (Screen::isKeyPressed(interface_key::CURSOR_DOWN) || Screen::isKeyPressed(interface_key::CURSOR_DOWNLEFT) || Screen::isKeyPressed(interface_key::CURSOR_DOWNRIGHT))
                moveScreen(0, -9);
            if (Screen::isKeyPressed(interface_key::CURSOR_LEFT) || Screen::isKeyPressed(interface_key::CURSOR_UPLEFT) || Screen::isKeyPressed(interface_key::CURSOR_DOWNLEFT))
                moveScreen(9, 0);
            if (Screen::isKeyPressed(interface_key::CURSOR_RIGHT) || Screen::isKeyPressed(interface_key::CURSOR_UPRIGHT) || Screen::isKeyPressed(interface_key::CURSOR_DOWNRIGHT))
                moveScreen(-9, 0);

            // Also change the fast cursor speed to 10 tiles
            if (Screen::isKeyPressed(interface_key::CURSOR_UP_FAST) || Screen::isKeyPressed(interface_key::CURSOR_UPLEFT_FAST) || Screen::isKeyPressed(interface_key::CURSOR_UPRIGHT_FAST))
                moveScreen(0, 10);
            if (Screen::isKeyPressed(interface_key::CURSOR_DOWN_FAST) || Screen::isKeyPressed(interface_key::CURSOR_DOWNLEFT_FAST) || Screen::isKeyPressed(interface_key::CURSOR_DOWNRIGHT_FAST))
                moveScreen(0, -10);
            if (Screen::isKeyPressed(interface_key::CURSOR_LEFT_FAST) || Screen::isKeyPressed(interface_key::CURSOR_UPLEFT_FAST) || Screen::isKeyPressed(interface_key::CURSOR_DOWNLEFT_FAST))
                moveScreen(10, 0);
            if (Screen::isKeyPressed(interface_key::CURSOR_RIGHT_FAST) || Screen::isKeyPressed(interface_key::CURSOR_UPRIGHT_FAST) || Screen::isKeyPressed(interface_key::CURSOR_DOWNRIGHT_FAST))
                moveScreen(-10, 0);


        inHook = false;
    }
    return CR_OK;
}

IMPLEMENT_VMETHOD_INTERPOSE(dwarfmode_hook, view);

DFHACK_PLUGIN("sort_build");
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
