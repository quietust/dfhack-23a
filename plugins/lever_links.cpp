// Highlight buildings linked to levers, and vice-versa

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <MiscUtils.h>
#include <modules/Gui.h>
#include <modules/Items.h>
#include <modules/Screen.h>
#include <modules/Maps.h>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

#include <VTableInterpose.h>
#include "df/world.h"
#include "df/ui.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/building_actual.h"
#include "df/building_drawbuffer.h"
#include "df/trap_type.h"
#include "df/general_ref.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::gps;
using df::global::cursor;
using df::global::ui_building_item_cursor;
using df::global::window_x;
using df::global::window_y;

void OutputString(int8_t color, int &x, int y, const std::string &text)
{
    Screen::paintString(Screen::Pen(' ', color, 0), x, y, text);
    x += text.length();
}

const df::interface_key hotkey = interface_key::STRING_Z;

bool inHook_dwarfmode_builditems = false;
df::building *target_bld = NULL;
int target_idx = -1;

df::general_ref_type getBuildingRefType (df::building *bld)
{
    if (!bld)
        return general_ref_type::BUILDING;

    // is it fully constructed?
    if (bld->getBuildStage() != bld->getMaxBuildStage())
        return general_ref_type::BUILDING;

    switch (bld->getType())
    {
    case df::building_type::Trap:
        // only levers and pressure plates are relevant
        if (bld->getSubtype() == trap_type::Lever || bld->getSubtype() == trap_type::PressurePlate)
            return general_ref_type::BUILDING_TRIGGERTARGET;
        break;
    case df::building_type::Bridge:
    case df::building_type::Door:
    case df::building_type::Floodgate:
    case df::building_type::Support:
    case df::building_type::Chain:
    case df::building_type::Cage:
        return general_ref_type::BUILDING_TRIGGER;
        break;
    }

    return general_ref_type::BUILDING;
}

df::general_ref *getLinkedBuildingRef (df::building_actual *bld, int slot, df::general_ref_type reftype)
{
    // have building?
    if (!bld)
        return NULL;

    // slot in range?
    if (slot < 0 || slot > bld->contained_items.size())
        return NULL;

    // slot is building component?
    if (bld->contained_items[slot]->use_mode != 2)
        return NULL;

    // is it a mechanism?
    df::item *item = bld->contained_items[slot]->item;
    if (!item || (item->getType() != item_type::TRAPPARTS))
        return NULL;

    // Return the matching reference, if it exists
    return Items::getGeneralRef(item, reftype);
}

struct dwarfmode_hook : df::viewscreen_dwarfmodest
{
    typedef df::viewscreen_dwarfmodest interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        df::general_ref_type reftype;
        df::building_actual *bld;
        df::general_ref *ref;

        inHook_dwarfmode_builditems = false;

        if (ui->main.mode == ui_sidebar_mode::BuildingItems && world->selected_building) do
        {
            // make sure the building is valid, and determine its linkage type
            reftype = getBuildingRefType(world->selected_building);
            if (reftype == general_ref_type::BUILDING)
                break;

            // we've established above that the selected building is Actual
            bld = (df::building_actual *)world->selected_building;
            int id = bld->id;

            // check if there are any linked mechanisms in the building
            for (int i = 0; i < bld->contained_items.size(); i++)
            {
                ref = getLinkedBuildingRef(bld, i, reftype);
                if (ref)
                    break;
            }
            if (!ref)
                break;

            // okay to show keybinding
            inHook_dwarfmode_builditems = true;
            target_bld = NULL;
            target_idx = -1;

            // check if the currently selected item is a linking mechanism
            ref = getLinkedBuildingRef(bld, *ui_building_item_cursor, reftype);
            if (!ref)
                break;

            // Get the target building, and determine its reference type
            target_bld = ref->getBuilding();
            reftype = getBuildingRefType(target_bld);
            if (reftype == general_ref_type::BUILDING)
                break;

            bld = (df::building_actual *)target_bld;

            // find the mechanism at the other end so we can zoom back and forth
            for (int i = 0; i < bld->contained_items.size(); i++)
            {
                ref = getLinkedBuildingRef(bld, i, reftype);
                if (!ref)
                    continue;

                // does it point back to the original building?
                if (ref->getID() == id)
                {
                    target_idx = i;
                    break;
                }
            }

            // if we have a valid target, allow pressing the hotkey
            if ((target_bld != NULL) && Screen::isKeyPressed(hotkey))
            {
                world->selected_building = target_bld;
                // if the index is somehow missing, then just jump to the beginning of the list so it doesn't crash
                if (target_idx != -1)
                    *ui_building_item_cursor = target_idx;
                else
                    *ui_building_item_cursor = 0;
                cursor->x = target_bld->centerx;
                cursor->y = target_bld->centery;
                cursor->z = target_bld->z;
                Gui::revealInDwarfmodeMap(df::coord(cursor->x, cursor->y, cursor->z), true);
            }
        } while (0);
        INTERPOSE_NEXT(view)();
    }
};

DFhackCExport command_result plugin_onrender ( color_ostream &out)
{
    auto dims = Gui::getDwarfmodeViewDims();
    int x, y;
    if (inHook_dwarfmode_builditems)
    {
        df::tile_designation flags = *Maps::getTileDesignation(Gui::getCursorPos());

        // Do we have a linkage to display?
        if (target_bld)
        {
            // Print the linked building's name
            stl::string name;
            target_bld->getName(&name);
            y = 18;
            x = dims.menu_x1 + 1;
            OutputString(15, x, y, "Link: ");
            target_bld->getNameColor();
            OutputString(gps->screenf + gps->screenbright * 8, x, y, name.c_str());

            // Check if it's on-screen, and make sure the flashing interval is right (out of phase with normal flashing)
            if (target_bld->isVisible() && (gps->map_renderer.cur_tick_count % 250 >= 125))
            {
                // Render the building
                df::building_drawbuffer drawbuf;
                target_bld->getDrawExtents(&drawbuf);
                target_bld->drawBuilding(&drawbuf);

                // then highlight it on the screen (by recoloring matching tiles)
                for (int bx = 0; bx <= drawbuf.x2 - drawbuf.x1; bx++)
                {
                    for (int by = 0; by <= drawbuf.y2 - drawbuf.y1; by++)
                    {
                        // ignore transparent tiles
                        if (drawbuf.tile[bx][by] == ' ')
                            continue;

                        // translate to viewport coordinates
                        int px = drawbuf.x1 + bx - *window_x + dims.map_x1;
                        int py = drawbuf.y1 + by - *window_y + dims.y1;

                        // make sure they're inside the viewport
                        if ((px < dims.map_x1) || (px > dims.map_x2) || (py < dims.y1) || (py > dims.y2))
                            continue;

                        // only flash the color if the character matches
                        if (gps->screen[px][py].chr == (char)drawbuf.tile[bx][by])
                        {
                            gps->screen[px][py].fore = 2;
                            gps->screen[px][py].back = 0;
                            gps->screen[px][py].bright = 1;
                        }
                    }
                }
            }
        }

        // Finally, display the hotkey label (and color it if Zoom will actually work)
        y = 23;
        x = dims.menu_x1 + 1;
        OutputString(10, x, y, Screen::getKeyDisplay(hotkey));
        OutputString(target_bld ? 15 : 8, x, y, ": Zoom to linkage");

        // All done with this hook
        inHook_dwarfmode_builditems = false;
    }
    return CR_OK;
}

IMPLEMENT_VMETHOD_INTERPOSE(dwarfmode_hook, view);

DFHACK_PLUGIN("lever_links");
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
