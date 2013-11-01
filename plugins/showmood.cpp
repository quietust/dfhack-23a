// Show details of currently active strange mood, if any

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "modules/Materials.h"
#include "modules/Translation.h"
#include "modules/Items.h"

#include "DataDefs.h"
#include "df/world.h"
#include "df/job.h"
#include "df/matgloss_metal.h"
#include "df/general_ref.h"
#include "df/unit.h"
#include "df/building.h"

using std::string;
using std::vector;
using namespace DFHack;
using namespace df::enums;

using df::global::world;

command_result df_showmood (color_ostream &out, vector <string> & parameters)
{
    if (!parameters.empty())
        return CR_WRONG_USAGE;

    if (!Translation::IsValid())
    {
        out.printerr("Translation data unavailable!\n");
        return CR_FAILURE;
    }

    CoreSuspender suspend;

    bool found = false;
    for (df::job_list_link *cur = world->job_list.next; cur != NULL; cur = cur->next)
    {
        df::job *job = cur->item;
        if ((job->job_type < job_type::StrangeMoodCrafter) || (job->job_type > job_type::StrangeMoodMechanics))
            continue;
        found = true;
        df::unit *unit = NULL;
        df::building *building = NULL;
        for (size_t i = 0; i < job->general_refs.size(); i++)
        {
            df::general_ref *ref = job->general_refs[i];
            if (ref->getType() == general_ref_type::UNIT_WORKER)
                unit = ref->getUnit();
            if (ref->getType() == general_ref_type::BUILDING_HOLDER)
                building = ref->getBuilding();
        }
        if (!unit)
        {
            out.printerr("Found strange mood not attached to any dwarf!\n");
            continue;
        }
        if (unit->mood == mood_type::None)
        {
            out.printerr("Dwarf with strange mood does not have a mood type!\n");
            continue;
        }
        out.print("%s is currently ", Translation::TranslateName(&unit->name, false).c_str());
        switch (unit->mood)
        {
        case mood_type::Macabre:
            out.print("in a macabre mood");
            if (job->job_type != job_type::StrangeMoodBrooding)
                out.print(" (but isn't actually in a macabre mood?)");
            break;

        case mood_type::Fell:
            out.print("in a fell mood");
            if (job->job_type != job_type::StrangeMoodFell)
                out.print(" (but isn't actually in a fell mood?)");
            break;

        case mood_type::Fey:
        case mood_type::Secretive:
        case mood_type::Possessed:
            switch (unit->mood)
            {
            case mood_type::Fey:
                out.print("in a fey mood");
                break;
            case mood_type::Secretive:
                out.print("in a secretive mood");
                break;
            case mood_type::Possessed:
                out.print("possessed");
                break;
            }
            out.print(" with intent to ");
            switch (job->job_type)
            {
            case job_type::StrangeMoodCrafter:
                out.print("claim a Craftsdwarf's Workshop");
                break;
            case job_type::StrangeMoodJeweller:
                out.print("claim a Jeweler's Workshop");
                break;
            case job_type::StrangeMoodForge:
                out.print("claim a Metalsmith's Forge");
                break;
            case job_type::StrangeMoodMagmaForge:
                out.print("claim a Magma Forge");
                break;
            case job_type::StrangeMoodCarpenter:
                out.print("claim a Carpenter's Workshop");
                break;
            case job_type::StrangeMoodMason:
                out.print("claim a Mason's Workshop");
                break;
            case job_type::StrangeMoodBowyer:
                out.print("claim a Boywer's Workshop");
                break;
            case job_type::StrangeMoodTanner:
                out.print("claim a Leather Works");
                break;
            case job_type::StrangeMoodWeaver:
                out.print("claim a Clothier's Shop");
                break;
            case job_type::StrangeMoodGlassmaker:
                out.print("claim a Glass Furnace");
                break;
            case job_type::StrangeMoodMechanics:
                out.print("claim a Mechanic's Workshop");
                break;
            case job_type::StrangeMoodBrooding:
                out.print("enter a macabre mood?");
                break;
            case job_type::StrangeMoodFell:
                out.print("enter a fell mood?");
                break;
            default:
                out.print("do something else...");
                break;
            }
            out.print(" and become a legendary %s", ENUM_ATTR_STR(job_skill, caption_noun, unit->job.mood_skill));
            if (unit->mood == mood_type::Possessed)
                out.print(" (but not really)");
            break;
        default:
            out.print("insane?");
            break;
        }
        out.print(".\n");
        if (unit->sex)
            out.print("He has ");
        else
            out.print("She has ");
        if (building)
        {
            stl::string name;
            building->getName(&name);
            out.print("claimed a %s and wants", name.c_str());
        }
        else
            out.print("not yet claimed a workshop but will want");
        out.print(" the following items:\n");

        // total amount of stuff fetched so far
        int count_got = job->items.size();

        for (size_t i = 0; i < unit->job.mood_item_type.size(); i++)
        {
            df::item_type item_type = unit->job.mood_item_type[i];
            int16_t item_subtype = unit->job.mood_item_subtype[i];
            df::material_type material = unit->job.mood_material[i];
            int16_t matgloss = unit->job.mood_matgloss[i];

            out.print("Item %i: ", i + 1);
            MaterialInfo matinfo(material, matgloss);
            string mat_name = matinfo.toString();

            switch (item_type)
            {
            case item_type::STONE:
                out.print("%s stone", mat_name.c_str());
                break;
            case item_type::BLOCKS:
                out.print("%s blocks", mat_name.c_str());
                break;
            case item_type::WOOD:
                out.print("%s logs", mat_name.c_str());
                break;
            case item_type::BAR:
                if (material == material_type::METAL &&
                    matgloss >= 0 && matgloss < df::global::world->raws.matgloss.metal.size() &&
                    df::global::world->raws.matgloss.metal[matgloss]->flags.is_set(matgloss_metal_flags::WAFERS))
                    out.print("%s wafers", mat_name.c_str());
                else
                    out.print("%s bars", mat_name.c_str());
                break;
            case item_type::SMALLGEM:
                out.print("%s cut gems", mat_name.c_str());
                break;
            case item_type::ROUGH:
                if (material == material_type::STONE)
                {
                    if (matgloss == -1)
                        mat_name = "any";
                    out.print("%s rough gems", mat_name.c_str());
                }
                else
                    out.print("raw %s", mat_name.c_str());
                break;
            case item_type::SKIN_TANNED:
                out.print("%s leather", mat_name.c_str());
                break;
            case item_type::CLOTH:
                switch (material)
                {
                case material_type::PLANT:
                    mat_name = "any plant fiber";
                    break;
                case material_type::SILK:
                    mat_name = "any silk";
                    break;
                default:
                    mat_name = "any";
                }
                out.print("%s cloth", mat_name.c_str());
                break;
            case item_type::REMAINS:
                out.print("%s remains", mat_name.c_str());
                break;
            case item_type::CORPSE:
                out.print("%s corpse", mat_name.c_str());
                break;
            default:
                {
                    ItemTypeInfo itinfo(item_type, item_subtype);

                    out.print("item %s material %s",
                                 itinfo.toString().c_str(), mat_name.c_str());
                    break;
                }
            }

            // total amount of stuff fetched for this requirement
            // XXX may fail with cloth/thread/bars if need 1 and fetch 2
            if (count_got)
                out.print(" (collected)");
            out.print("\n");
            count_got--;
        }
    }
    if (!found)
        out.print("No strange moods currently active.\n");

    return CR_OK;
}

DFHACK_PLUGIN("showmood");

DFhackCExport command_result plugin_init (color_ostream &out, std::vector<PluginCommand> &commands)
{
    commands.push_back(PluginCommand("showmood", "Shows items needed for current strange mood.", df_showmood, false,
        "Run this command without any parameters to display information on the currently active Strange Mood."));
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}
