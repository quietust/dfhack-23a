// Allow reviewing previous trade agreements

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
#include "df/viewscreen_entityst.h"
#include "df/viewscreen_tradeagreementst.h"
#include "df/viewscreen_requestagreementst.h"
#include "df/meeting_event.h"
#include "df/historical_entity.h"
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

struct entity_hook : df::viewscreen_entityst
{
    typedef df::viewscreen_entityst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        if ((page == 2) &&
            (agreement_cursor >= 0) && (agreement_cursor < agreements.size()) &&
            (Screen::isKeyPressed(interface_key::SELECT)))
        {
            df::meeting_event *event = agreements[agreement_cursor];
            if (event->type == meeting_event_type::ImportAgreement)
            {
                df::viewscreen_requestagreementst *vs = df::allocate<df::viewscreen_requestagreementst>();
                vs->requests = event->buy_prices;
                vs->civ = entity;
                vs->civ_id = entity->id;
                vs->cursor = 0;
                sprintf(vs->title, "  Trade Agreement with %s  ", Translation::TranslateName(&entity->name, false).c_str());
                if (Screen::show(vs))
                {
                    gview->current_key = 0;
                    vs->view();
                    return;
                }
                delete vs;
            }
            if (event->type == meeting_event_type::ExportAgreement)
            {
                df::viewscreen_tradeagreementst *vs = df::allocate<df::viewscreen_tradeagreementst>();
                vs->requests = event->sell_prices;
                vs->civ = entity;
                vs->civ_id = entity->id;
                vs->cursor_left = 0;
                vs->cursor_right = 0;
                sprintf(vs->title, "  Trade Agreement with %s  ", Translation::TranslateName(&entity->name, false).c_str());
                if (Screen::show(vs))
                {
                    gview->current_key = 0;
                    vs->view();
                    return;
                }
                delete vs;
            }
        }
        INTERPOSE_NEXT(view)();
    }
};

IMPLEMENT_VMETHOD_INTERPOSE(entity_hook, view);

DFHACK_PLUGIN("trade_review");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

DFhackCExport command_result plugin_enable(color_ostream &out, bool enable)
{
    if (enable != is_enabled)
    {
        if (!INTERPOSE_HOOK(entity_hook, view).apply(enable))
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
    INTERPOSE_HOOK(entity_hook, view).remove();
    return CR_OK;
}
