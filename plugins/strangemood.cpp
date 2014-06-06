// Triggers a strange mood using (mostly) the same logic used in-game

#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "modules/Gui.h"
#include "modules/Units.h"
#include "modules/Items.h"
#include "modules/Job.h"
#include "modules/Translation.h"
#include "modules/Random.h"

#include "DataDefs.h"
#include "df/world.h"
#include "df/ui.h"
#include "df/unit.h"
#include "df/skill_rating.h"
#include "df/unit_skill.h"
#include "df/unit_preference.h"
#include "df/map_block.h"
#include "df/job.h"
#include "df/material_type.h"
#include "df/historical_entity.h"
#include "df/entity_raw.h"
#include "df/general_ref_unit_workerst.h"

using std::string;
using std::vector;
using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::created_item_count;
using df::global::created_item_type;
using df::global::created_item_subtype;
using df::global::created_item_material;
using df::global::created_item_matgloss;

Random::MersenneRNG rng;

bool isUnitMoodable (df::unit *unit)
{
    if (!Units::isCitizen(unit))
        return false;
    if (!unit->status2.limbs_grasp_count)
        return false;
    if (unit->mood != mood_type::None)
        return false;
    if (!ENUM_ATTR(profession,moodable,unit->profession))
        return false;
    return true;
}

df::job_skill getMoodSkill (df::unit *unit)
{
    vector<df::job_skill> skills;
    df::skill_rating level = skill_rating::Dabbling;
    for (size_t i = 0; i < unit->status.skills.size(); i++)
    {
        df::unit_skill *skill = unit->status.skills[i];
        switch (skill->id)
        {
        case job_skill::MINING:
        case job_skill::CARPENTRY:
        case job_skill::DETAILSTONE:
        case job_skill::MASONRY:
        case job_skill::TANNER:
        case job_skill::WEAVING:
        case job_skill::CLOTHESMAKING:
        case job_skill::SMELT:
        case job_skill::EXTRACT_ADAMANTINE:
        case job_skill::FORGE_WEAPON:
        case job_skill::FORGE_ARMOR:
        case job_skill::FORGE_FURNITURE:
        case job_skill::CUTGEM:
        case job_skill::ENCRUSTGEM:
        case job_skill::WOODCRAFT:
        case job_skill::STONECRAFT:
        case job_skill::METALCRAFT:
        case job_skill::GLASSMAKER:
        case job_skill::ADAMANTINE_WORK:
        case job_skill::ADAMANTINE_SMELT:
        case job_skill::ADAMANTINE_WEAVE:
        case job_skill::LEATHERWORK:
        case job_skill::BONECARVE:
        case job_skill::BOWYER:
        case job_skill::MECHANICS:
            if (skill->rating > level)
            {
                skills.clear();
                level = skill->rating;
            }
            if (skill->rating == level)
                skills.push_back(skill->id);
            break;
        }
    }
    if (!skills.size())
    {
        skills.push_back(job_skill::WOODCRAFT);
        skills.push_back(job_skill::STONECRAFT);
        skills.push_back(job_skill::BONECARVE);
    }
    return skills[rng.df_trandom(skills.size())];
}

void selectWord (const df::language_word_table &table, int32_t &word, df::part_of_speech &part, int mode)
{
    if (table.parts[mode].size())
    {
        int offset = rng.df_trandom(table.parts[mode].size());
        word = table.words[mode][offset];
        part = table.parts[mode][offset];
    }
    else
    {
        word = rng.df_trandom(world->raws.language.words.size());
        part = (df::part_of_speech)(rng.df_trandom(9));
        Core::getInstance().getConsole().printerr("Impoverished Word Selector");
    }
}

void generateName(df::language_name &output, int language, int mode, const df::language_word_table &table1, const df::language_word_table &table2)
{
    for (int i = 0; i < 100; i++)
    {
        if (mode != 8 && mode != 9)
        {
            output = df::language_name();
            if (language == -1)
                language = rng.df_trandom(world->raws.language.translations.size());
            output.unknown = mode;
            output.language = language;
        }
        output.has_name = 1;
        if (output.language == -1)
            output.language = rng.df_trandom(world->raws.language.translations.size());
        int r, r2, r3;
        switch (mode)
        {
        case 0: case 9: case 10:
            if (mode != 9)
            {
                int32_t word; df::part_of_speech part;
                output.first_name.clear();
                selectWord(table1, word, part, 2);
                if (word >= 0 && word < world->raws.language.words.size())
                    output.first_name = *world->raws.language.translations[language]->words[word];
            }
            if (mode != 10)
            {
        case 4: case 37: // this is not a typo
                if (rng.df_trandom(2))
                {
                    selectWord(table2, output.parts[0].word, output.parts[0].part_of_speech, 0);
                    selectWord(table1, output.parts[1].word, output.parts[1].part_of_speech, 1);
                }
                else
                {
                    selectWord(table1, output.parts[0].word, output.parts[0].part_of_speech, 0);
                    selectWord(table2, output.parts[1].word, output.parts[1].part_of_speech, 1);
                }
            }
            break;

        case 1: case 13: case 20:
            r = rng.df_trandom(3);
            if (r == 0 || r == 1)
            {
                if (rng.df_trandom(2))
                {
                    selectWord(table2, output.parts[0].word, output.parts[0].part_of_speech, 0);
                    selectWord(table1, output.parts[1].word, output.parts[1].part_of_speech, 1);
                }
                else
                {
                    selectWord(table1, output.parts[0].word, output.parts[0].part_of_speech, 0);
                    selectWord(table2, output.parts[1].word, output.parts[1].part_of_speech, 1);
                }
            }
            if (r == 1 || r == 2)
            {
        case 3: case 8: case 11: // this is not a typo either
                r2 = rng.df_trandom(2);
                if (r2)
                    selectWord(table1, output.parts[5].word, output.parts[5].part_of_speech, 2);
                else
                    selectWord(table2, output.parts[5].word, output.parts[5].part_of_speech, 2);
                r3 = rng.df_trandom(3);
                if (rng.df_trandom(50))
                    r3 = rng.df_trandom(2);
                switch (r3)
                {
                case 0:
                case 2:
                    if (r3 == 2)
                        r2 = rng.df_trandom(2);
                    if (r2)
                        selectWord(table2, output.parts[6].word, output.parts[6].part_of_speech, 5);
                    else
                        selectWord(table1, output.parts[6].word, output.parts[6].part_of_speech, 5);
                    if (r3 == 0)
                        break;
                    r2 = -r2;
                case 1:
                    if (r2)
                        selectWord(table1, output.parts[2].word, output.parts[2].part_of_speech, 3);
                    else
                        selectWord(table2, output.parts[2].word, output.parts[2].part_of_speech, 3);
                    if (!(rng.df_trandom(100)))
                        selectWord(table1, output.parts[3].word, output.parts[3].part_of_speech, 3);
                    break;
                }
            }
            if (rng.df_trandom(100))
            {
                if (rng.df_trandom(2))
                    selectWord(table1, output.parts[4].word, output.parts[4].part_of_speech, 4);
                else
                    selectWord(table2, output.parts[4].word, output.parts[4].part_of_speech, 4);
            }
            if ((mode == 3) && (output.parts[5].part_of_speech == part_of_speech::Noun) && (output.parts[5].word != -1) && (world->raws.language.words[output.parts[5].word]->forms[1].length()))
                output.parts[5].part_of_speech = part_of_speech::NounPlural;
            break;

        case 2: case 5: case 6: case 12: case 14: case 15: case 16: case 17: case 18: case 19:
        case 21: case 22: case 23: case 24: case 25: case 26: case 27: case 28: case 29: case 30:
        case 31: case 32: case 33: case 34: case 35: case 36: case 38: case 39:
            selectWord(table1, output.parts[5].word, output.parts[5].part_of_speech, 2);
            r3 = rng.df_trandom(3);
            if (rng.df_trandom(50))
                r3 = rng.df_trandom(2);
            switch (r3)
            {
            case 0:
            case 2:
                selectWord(table2, output.parts[6].word, output.parts[6].part_of_speech, 5);
                if (r3 == 0)
                    break;
            case 1:
                selectWord(table2, output.parts[2].word, output.parts[2].part_of_speech, 3);
                if (!(rng.df_trandom(100)))
                    selectWord(table2, output.parts[3].word, output.parts[3].part_of_speech, 3);
                break;
            }
            if (rng.df_trandom(100))
                selectWord(table2, output.parts[4].word, output.parts[4].part_of_speech, 4);
            break;

        case 7:
            r = rng.df_trandom(3);
            if (r == 0 || r == 1)
            {
                selectWord(table2, output.parts[0].word, output.parts[0].part_of_speech, 0);
                selectWord(table1, output.parts[1].word, output.parts[1].part_of_speech, 1);
            }
            if (r == 1 || r == 2)
            {
                r2 = rng.df_trandom(2);
                if (r == 2 || r2 == 1)
                    selectWord(table1, output.parts[5].word, output.parts[5].part_of_speech, 2);
                else
                    selectWord(table2, output.parts[5].word, output.parts[5].part_of_speech, 2);
                r3 = rng.df_trandom(3);
                if (rng.df_trandom(50))
                    r3 = rng.df_trandom(2);
                switch (r3)
                {
                case 0:
                case 2:
                    selectWord(table1, output.parts[6].word, output.parts[6].part_of_speech, 5);
                    if (r3 == 0)
                        break;
                case 1:
                    selectWord(table2, output.parts[2].word, output.parts[2].part_of_speech, 3);
                    if (!(rng.df_trandom(100)))
                        selectWord(table2, output.parts[3].word, output.parts[3].part_of_speech, 3);
                    break;
                }
            }
            if (rng.df_trandom(100))
                selectWord(table2, output.parts[4].word, output.parts[4].part_of_speech, 4);
            break;
        }
        if (output.parts[2].word != -1 && output.parts[3].word != -1 &&
            world->raws.language.words[output.parts[3].word]->adj_dist < world->raws.language.words[output.parts[2].word]->adj_dist)
        {
            std::swap(output.parts[2].word, output.parts[3].word);
            std::swap(output.parts[2].part_of_speech, output.parts[3].part_of_speech);
        }
        bool next = false;
        if ((output.parts[5].part_of_speech == df::part_of_speech::NounPlural) && (output.parts[6].part_of_speech == df::part_of_speech::NounPlural))
            next = true;
        if (output.parts[0].word != -1)
        {
            if (output.parts[6].word == -1) next = true;
            if (output.parts[4].word == -1) next = true;
            if (output.parts[2].word == -1) next = true;
            if (output.parts[3].word == -1) next = true;
            if (output.parts[5].word == -1) next = true;
        }
        if (output.parts[1].word != -1)
        {
            if (output.parts[6].word == -1) next = true;
            if (output.parts[4].word == -1) next = true;
            if (output.parts[2].word == -1) next = true;
            if (output.parts[3].word == -1) next = true;
            if (output.parts[5].word == -1) next = true;
        }
        if (output.parts[4].word != -1)
        {
            if (output.parts[6].word == -1) next = true;
            if (output.parts[2].word == -1) next = true;
            if (output.parts[3].word == -1) next = true;
            if (output.parts[5].word == -1) next = true;
        }
        if (output.parts[2].word != -1)
        {
            if (output.parts[6].word == -1) next = true;
            if (output.parts[3].word == -1) next = true;
            if (output.parts[5].word == -1) next = true;
        }
        if (output.parts[3].word != -1)
        {
            if (output.parts[6].word == -1) next = true;
            if (output.parts[5].word == -1) next = true;
        }
        if (output.parts[5].word != -1)
        {
            if (output.parts[6].word == -1) next = true;
        }
        if (!next)
            return;
    }
}

command_result df_strangemood (color_ostream &out, vector <string> & parameters)
{
    if (!Translation::IsValid())
    {
        out.printerr("Translation data unavailable!\n");
        return CR_FAILURE;
    }
    bool force = false;
    df::unit *unit = NULL;
    df::mood_type type = mood_type::None;
    df::job_skill skill = job_skill::NONE;

    for (size_t i = 0; i < parameters.size(); i++)
    {
        if(parameters[i] == "help" || parameters[i] == "?")
            return CR_WRONG_USAGE;
        else if(parameters[i] == "-force")
            force = true;
        else if(parameters[i] == "-unit")
        {
            unit = DFHack::Gui::getSelectedUnit(out);
            if (!unit)
                return CR_FAILURE;
        }
        else if (parameters[i] == "-type")
        {
            i++;
            if (i == parameters.size())
            {
                out.printerr("No mood type specified!\n");
                return CR_WRONG_USAGE;
            }
            if (parameters[i] == "fey")
                type = mood_type::Fey;
            else if (parameters[i] == "secretive")
                type = mood_type::Secretive;
            else if (parameters[i] == "possessed")
                type = mood_type::Possessed;
            else if (parameters[i] == "fell")
                type = mood_type::Fell;
            else if (parameters[i] == "macabre")
                type = mood_type::Macabre;
            else
            {
                out.printerr("Mood type '%s' not recognized!\n", parameters[i].c_str());
                return CR_WRONG_USAGE;
            }
        }
        else if (parameters[i] == "-skill")
        {
            i++;
            if (i == parameters.size())
            {
                out.printerr("No mood skill specified!\n");
                return CR_WRONG_USAGE;
            }
            else if (parameters[i] == "miner")
                skill = job_skill::MINING;
            else if (parameters[i] == "carpenter")
                skill = job_skill::CARPENTRY;
            else if (parameters[i] == "engraver")
                skill = job_skill::DETAILSTONE;
            else if (parameters[i] == "mason")
                skill = job_skill::MASONRY;
            else if (parameters[i] == "tanner")
                skill = job_skill::TANNER;
            else if (parameters[i] == "weaver")
                skill = job_skill::WEAVING;
            else if (parameters[i] == "clothesmaker")
                skill = job_skill::CLOTHESMAKING;
            else if (parameters[i] == "furnaceoperator")
                skill = job_skill::SMELT;
            else if (parameters[i] == "adamantineextractor")
                skill = job_skill::EXTRACT_ADAMANTINE;
            else if (parameters[i] == "weaponsmith")
                skill = job_skill::FORGE_WEAPON;
            else if (parameters[i] == "armorsmith")
                skill = job_skill::FORGE_ARMOR;
            else if (parameters[i] == "metalsmith")
                skill = job_skill::FORGE_FURNITURE;
            else if (parameters[i] == "gemcutter")
                skill = job_skill::CUTGEM;
            else if (parameters[i] == "jeweler")
                skill = job_skill::ENCRUSTGEM;
            else if (parameters[i] == "woodcrafter")
                skill = job_skill::WOODCRAFT;
            else if (parameters[i] == "stonecrafter")
                skill = job_skill::STONECRAFT;
            else if (parameters[i] == "metalcrafter")
                skill = job_skill::METALCRAFT;
            else if (parameters[i] == "glassmaker")
                skill = job_skill::GLASSMAKER;
            else if (parameters[i] == "adamantineworker")
                skill = job_skill::ADAMANTINE_WORK;
            else if (parameters[i] == "adamantinesmelter")
                skill = job_skill::ADAMANTINE_SMELT;
            else if (parameters[i] == "adamantineweaver")
                skill = job_skill::ADAMANTINE_WEAVE;
            else if (parameters[i] == "leatherworker")
                skill = job_skill::LEATHERWORK;
            else if (parameters[i] == "bonecarver")
                skill = job_skill::BONECARVE;
            else if (parameters[i] == "bowyer")
                skill = job_skill::BOWYER;
            else if (parameters[i] == "mechanic")
                skill = job_skill::MECHANICS;
            else
            {
                out.printerr("Mood skill '%s' not recognized!\n", parameters[i].c_str());
                return CR_WRONG_USAGE;
            }
        }
        else
        {
            out.printerr("Unrecognized parameter: %s\n", parameters[i].c_str());
            return CR_WRONG_USAGE;
        }
    }

    CoreSuspender suspend;

    // First, check if moods are enabled at all
    if (*df::global::debug_nomoods)
    {
        out.printerr("Strange moods disabled via debug flag!\n");
        return CR_FAILURE;
    }
    if (ui->mood_cooldown && !force)
    {
        out.printerr("Last strange mood happened too recently!\n");
        return CR_FAILURE;
    }

    // Also, make sure there isn't a mood already running
    for (size_t i = 0; i < world->units.active.size(); i++)
    {
        df::unit *cur = world->units.active[i];
        if (Units::isCitizen(cur) && cur->flags1.bits.has_mood)
        {
            ui->mood_cooldown = 1000;
            out.printerr("A strange mood is already in progress!\n");
            return CR_FAILURE;
        }
    }

    // See which units are eligible to enter moods
    vector<df::unit *> moodable_units;
    bool mood_available = false;
    for (size_t i = 0; i < world->units.active.size(); i++)
    {
        df::unit *cur = world->units.active[i];
        if (!isUnitMoodable(cur))
            continue;
        if (!cur->flags1.bits.had_mood)
            mood_available = true;
        moodable_units.push_back(cur);
    }
    if (!mood_available)
    {
        out.printerr("No dwarves are available to enter a mood!\n");
        return CR_FAILURE;
    }

    // If unit was manually selected, redo checks explicitly
    if (unit)
    {
        if (!isUnitMoodable(unit))
        {
            out.printerr("Selected unit is not eligible to enter a strange mood!\n");
            return CR_FAILURE;
        }
        if (unit->flags1.bits.had_mood)
        {
            out.printerr("Selected unit has already had a strange mood!\n");
            return CR_FAILURE;
        }
    }

    // Obey in-game mood limits
    if (!force)
    {
        if (moodable_units.size() < 20)
        {
            out.printerr("Fortress is not eligible for a strange mood at this time - not enough moodable units.\n");
            return CR_FAILURE;
        }
        int num_items = 0;
        for (size_t i = 0; i < created_item_count->size(); i++)
            num_items += created_item_count->at(i);

        if ((ui->max_dig_depth - ui->min_dig_depth) / 20 < ui->tasks.num_artifacts)
        {
            out.printerr("Fortress is not eligible for a strange mood at this time - haven't dug deep enough.\n");
            return CR_FAILURE;
        }
        if (num_items / 200 < ui->tasks.num_artifacts)
        {
            out.printerr("Fortress is not eligible for a strange mood at this time - not enough items created\n");
            return CR_FAILURE;
        }
    }

    // Randomly select a unit to enter a mood
    if (!unit)
    {
        vector<int32_t> tickets;
        for (size_t i = 0; i < moodable_units.size(); i++)
        {
            df::unit *cur = moodable_units[i];
            if (cur->flags1.bits.had_mood)
                continue;
            if (cur->relations.dragger_id != -1)
                continue;
            if (cur->relations.draggee_id != -1)
                continue;
            tickets.push_back(i);
            switch (cur->profession)
            {
            case profession::CARPENTER:
            case profession::MASON:
                for (int j = 0; j < 5; j++)
                    tickets.push_back(i);
                break;
            case profession::CRAFTSMAN:
            case profession::METALSMITH:
            case profession::JEWELER:
                for (int j = 0; j < 15; j++)
                    tickets.push_back(i);
                break;
            }
        }
        if (!tickets.size())
        {
            out.printerr("No units are eligible to enter a mood!\n");
            return CR_FAILURE;
        }
        unit = moodable_units[tickets[rng.df_trandom(tickets.size())]];
    }

    // Cancel selected unit's current job
    if (unit->job.current_job)
    {
        // TODO: cancel job
        out.printerr("Chosen unit '%s' has active job, cannot start mood!\n", Translation::TranslateName(&unit->name, false).c_str());
        return CR_FAILURE;
    }
    // TODO: remove moody dwarf from squad

    ui->mood_cooldown = 1000;
    // If no mood type was specified, pick one randomly
    if (type == mood_type::None)
    {
        if (rng.df_trandom(100) > unit->status.happiness)
        {
            switch (rng.df_trandom(2))
            {
            case 0: type = mood_type::Fell;    break;
            case 1: type = mood_type::Macabre; break;
            }
        }
        else
        {
            switch (rng.df_trandom(3))
            {
            case 0: type = mood_type::Fey;         break;
            case 1: type = mood_type::Secretive;   break;
            case 2: type = mood_type::Possessed;   break;
            }
        }
    }

    // Display announcement and start setting up the mood job
    int color = 0;
    bool bright = false;
    string msg = Translation::TranslateName(&unit->name, false) + ", " + Units::getProfessionName(unit);

    switch (type)
    {
    case mood_type::Fey:
        color = 7;
        bright = true;
        msg += " is taken by a fey mood!";
        break;
    case mood_type::Secretive:
        color = 7;
        bright = false;
        msg += " withdraws from society...";
        break;
    case mood_type::Possessed:
        color = 5;
        bright = true;
        msg += " has been possessed!";
        break;
    case mood_type::Macabre:
        color = 0;
        bright = true;
        msg += " begins to stalk and brood...";
        break;
    case mood_type::Fell:
        color = 5;
        bright = false;
        msg += " looses a roaring laughter, fell and terrible!";
        break;
    default:
        out.printerr("Invalid mood type selected?\n");
        return CR_FAILURE;
    }

    unit->mood = type;
    Gui::showZoomAnnouncement(unit->pos, msg, color, bright);
    
    unit->status.happiness = 100;
    // TODO: make sure unit drops any wrestle items
    unit->job.mood_timeout = 50000;
    unit->flags1.bits.has_mood = true;
    unit->flags1.bits.had_mood = true;
    if (skill == job_skill::NONE)
        skill = getMoodSkill(unit);

    unit->job.mood_skill = skill;
    df::job *job = new df::job();
    Job::linkIntoWorld(job);

    // Choose the job type
    if (unit->mood == mood_type::Fell)
        job->job_type = job_type::StrangeMoodFell;
    else if (unit->mood == mood_type::Macabre)
        job->job_type = job_type::StrangeMoodBrooding;
    else
    {
        switch (skill)
        {
        case job_skill::MINING:
        case job_skill::MASONRY:
            job->job_type = job_type::StrangeMoodMason;
            break;
        case job_skill::CARPENTRY:
            job->job_type = job_type::StrangeMoodCarpenter;
            break;
        case job_skill::DETAILSTONE:
        case job_skill::EXTRACT_ADAMANTINE:
        case job_skill::WOODCRAFT:
        case job_skill::STONECRAFT:
        case job_skill::BONECARVE:
            job->job_type = job_type::StrangeMoodCrafter;
            break;
        case job_skill::TANNER:
        case job_skill::LEATHERWORK:
            job->job_type = job_type::StrangeMoodTanner;
            break;
        case job_skill::WEAVING:
        case job_skill::CLOTHESMAKING:
            job->job_type = job_type::StrangeMoodWeaver;
            break;
        case job_skill::SMELT:
        case job_skill::FORGE_WEAPON:
        case job_skill::FORGE_ARMOR:
        case job_skill::FORGE_FURNITURE:
        case job_skill::METALCRAFT:
            if (ui->tasks.found_magma)
                job->job_type = job_type::StrangeMoodMagmaForge;
            else
                job->job_type = job_type::StrangeMoodForge;
            break;
        case job_skill::CUTGEM:
        case job_skill::ENCRUSTGEM:
            job->job_type = job_type::StrangeMoodJeweller;
            break;
        case job_skill::GLASSMAKER:
            job->job_type = job_type::StrangeMoodGlassmaker;
            break;
        case job_skill::ADAMANTINE_WORK:
        case job_skill::ADAMANTINE_SMELT:
        case job_skill::ADAMANTINE_WEAVE:
            job->job_type = job_type::StrangeMoodMagmaForge;
            break;
        case job_skill::BOWYER:
            job->job_type = job_type::StrangeMoodBowyer;
            break;
        case job_skill::MECHANICS:
            job->job_type = job_type::StrangeMoodMechanics;
            break;
        }
    }

    // The dwarf will want 1-3 of the base material
    int base_item_count = 1 + rng.df_trandom(3);
    // Gem Cutters and Gem Setters have a 50% chance of using only one base item
    if (((skill == job_skill::CUTGEM) || (skill == job_skill::ENCRUSTGEM)) && (rng.df_trandom(2)))
        base_item_count = 1;

    int deepest_dug_tile = ui->max_dig_depth; // actually a separate global, but the offset isn't recorded

    // Choose the base material
    if (job->job_type == job_type::StrangeMoodBrooding)
    {
        switch (rng.df_trandom(3))
        {
        case 0:
            unit->job.mood_item_type.push_back(item_type::REMAINS);
            unit->job.mood_item_subtype.push_back(-1);
            unit->job.mood_material.push_back(material_type::NONE);
            unit->job.mood_matgloss.push_back(-1);
            break;
        case 1:
            unit->job.mood_item_type.push_back(item_type::BONES);
            unit->job.mood_item_subtype.push_back(-1);
            unit->job.mood_material.push_back(material_type::NONE);
            unit->job.mood_matgloss.push_back(-1);
            break;
        case 2:
            unit->job.mood_item_type.push_back(item_type::SKULL);
            unit->job.mood_item_subtype.push_back(-1);
            unit->job.mood_material.push_back(material_type::NONE);
            unit->job.mood_matgloss.push_back(-1);
            break;
        }
    }
    else if (job->job_type != job_type::StrangeMoodFell)
    {
        bool found_pref;
        bool c_tin, c_iron, c_gold, c_silver, c_copper, c_platinum;
        bool w_iron, w_copper;
        int metal;
        switch (skill)
        {
        case job_skill::MINING:
        case job_skill::DETAILSTONE:
        case job_skill::MASONRY:
        case job_skill::STONECRAFT:
        case job_skill::MECHANICS:
            unit->job.mood_item_type.push_back(item_type::STONE);
            unit->job.mood_item_subtype.push_back(-1);

            found_pref = false;
            for (size_t i = 0; i < unit->status.preferences.size(); i++)
            {
                df::unit_preference *pref = unit->status.preferences[i];
                if (pref->active == 1 &&
                    pref->type == df::unit_preference::T_type::LikeMaterial &&
                    (pref->material == material_type::STONE_GRAY || pref->material == material_type::STONE_DARK || pref->material == material_type::STONE_LIGHT))
                {
                    unit->job.mood_material.push_back(pref->material);
                    found_pref = true;
                    break;
                }
            }
            if (!found_pref)
            {
                switch (rng.df_trandom(3))
                {
                case 0:
                    unit->job.mood_material.push_back(material_type::STONE_GRAY);
                    break;
                case 1:
                    unit->job.mood_material.push_back(material_type::STONE_DARK);
                    break;
                case 2:
                    unit->job.mood_material.push_back(material_type::STONE_LIGHT);
                    break;
                }
            }
            unit->job.mood_matgloss.push_back(-1);
            break;

        case job_skill::CARPENTRY:
        case job_skill::WOODCRAFT:
        case job_skill::BOWYER:
            unit->job.mood_item_type.push_back(item_type::WOOD);
            unit->job.mood_item_subtype.push_back(-1);
            unit->job.mood_material.push_back(material_type::WOOD);
            unit->job.mood_matgloss.push_back(-1);
            break;

        case job_skill::TANNER:
        case job_skill::LEATHERWORK:
            unit->job.mood_item_type.push_back(item_type::SKIN_TANNED);
            unit->job.mood_item_subtype.push_back(-1);
            unit->job.mood_material.push_back(material_type::LEATHER);
            unit->job.mood_matgloss.push_back(-1);
            break;

        case job_skill::WEAVING:
        case job_skill::CLOTHESMAKING:
            unit->job.mood_item_type.push_back(item_type::CLOTH);
            unit->job.mood_item_subtype.push_back(-1);

            found_pref = false;
            for (size_t i = 0; i < unit->status.preferences.size(); i++)
            {
                df::unit_preference *pref = unit->status.preferences[i];
                if (pref->active == 1 &&
                    pref->type == df::unit_preference::T_type::LikeMaterial &&
                    ((pref->material == material_type::PLANT) || (pref->material == material_type::SILK)))
                {
                    unit->job.mood_material.push_back(pref->material);
                    unit->job.mood_matgloss.push_back(-1);
                    found_pref = true;
                    break;
                }
            }
            if (!found_pref)
            {
                unit->job.mood_material.push_back(rng.df_trandom(2) ? material_type::SILK : material_type::PLANT);
                unit->job.mood_matgloss.push_back(-1);
            }
            break;

        case job_skill::SMELT:
        case job_skill::FORGE_FURNITURE:
        case job_skill::METALCRAFT:
            unit->job.mood_item_type.push_back(item_type::ORE);
            unit->job.mood_item_subtype.push_back(-1);

            c_tin = c_iron = c_gold = c_silver = c_copper = c_platinum = false;
            for (size_t i = 0; i < unit->status.preferences.size(); i++)
            {
                df::unit_preference *pref = unit->status.preferences[i];
                if (pref->active == 1 &&
                    pref->type == df::unit_preference::T_type::LikeMaterial)
                {
                    if (pref->material == material_type::TIN)
                        c_tin = true;
                    if (pref->material == material_type::IRON)
                        c_iron = true;
                    if (pref->material == material_type::GOLD)
                        c_gold = true;
                    if (pref->material == material_type::SILVER)
                        c_silver = true;
                    if (pref->material == material_type::COPPER)
                        c_copper = true;
                    if (pref->material == material_type::PLATINUM)
                        c_platinum = true;
                }
            }
            if (deepest_dug_tile < 265)
                c_iron = c_gold = c_platinum = false;
            while (1)
            {
                metal = (deepest_dug_tile < 265) ? (rng.df_trandom(3) + 3) : rng.df_trandom(6);
                if (c_iron || c_gold || c_silver || c_copper || c_tin || c_platinum)
                {
                    if (metal == 0 && c_iron)
                        break;
                    if (metal == 1 && c_gold)
                        break;
                    if (metal == 2 && c_platinum)
                        break;
                    if (metal == 3 && c_silver)
                        break;
                    if (metal == 4 && c_copper)
                        break;
                    if (metal == 5 && c_tin)
                        break;
                }
                else
                    break;
            }
            // this isn't strictly necessary, but the game still does it
            if (deepest_dug_tile < 265)
            {
                if (metal == 0)
                    metal = 5;
                else if (metal == 1)
                    metal = 4;
                else if (metal == 2)
                    metal = 3;
            }
            switch (metal)
            {
            case 0:
                unit->job.mood_material.push_back(material_type::IRON);
                break;
            case 1:
                unit->job.mood_material.push_back(material_type::GOLD);
                break;
            case 2:
                unit->job.mood_material.push_back(material_type::PLATINUM);
                break;
            case 3:
                unit->job.mood_material.push_back(material_type::SILVER);
                break;
            case 4:
                unit->job.mood_material.push_back(material_type::COPPER);
                break;
            case 5:
                unit->job.mood_material.push_back(material_type::TIN);
                break;
            }
            unit->job.mood_matgloss.push_back(-1);
            break;

        case job_skill::EXTRACT_ADAMANTINE:
        case job_skill::ADAMANTINE_WORK:
        case job_skill::ADAMANTINE_SMELT:
        case job_skill::ADAMANTINE_WEAVE:
            unit->job.mood_item_type.push_back(item_type::ORE);
            unit->job.mood_item_subtype.push_back(-1);
            unit->job.mood_material.push_back(material_type::ADAMANTINE);
            unit->job.mood_matgloss.push_back(-1);
            break;

        case job_skill::FORGE_WEAPON:
        case job_skill::FORGE_ARMOR:
            unit->job.mood_item_type.push_back(item_type::ORE);
            unit->job.mood_item_subtype.push_back(-1);

            w_iron = w_copper = false;
            for (size_t i = 0; i < unit->status.preferences.size(); i++)
            {
                df::unit_preference *pref = unit->status.preferences[i];
                if (pref->active == 1 &&
                    pref->type == df::unit_preference::T_type::LikeMaterial)
                {
                    if (pref->material == material_type::IRON)
                        w_iron = true;
                    if (pref->material == material_type::COPPER)
                        w_copper = true;
                }
            }
            if (deepest_dug_tile < 265)
                w_iron = false;
            while (1)
            {
                metal = rng.df_trandom(2);
                if (w_iron || w_copper)
                {
                    if (metal == 0 && w_iron)
                        break;
                    if (metal == 1 && w_copper)
                        break;
                }
                else
                    break;
            }
            // this isn't strictly necessary, but the game still does it
            if (deepest_dug_tile < 265)
                metal = 1;
            switch (metal)
            {
            case 0:
                unit->job.mood_material.push_back(material_type::IRON);
                break;
            case 1:
                unit->job.mood_material.push_back(material_type::COPPER);
                break;
            }
            unit->job.mood_matgloss.push_back(-1);
            break;

        case job_skill::CUTGEM:
        case job_skill::ENCRUSTGEM:
            unit->job.mood_item_type.push_back(item_type::ROUGH);
            unit->job.mood_item_subtype.push_back(-1);

            found_pref = false;
            for (size_t i = 0; i < unit->status.preferences.size(); i++)
            {
                df::unit_preference *pref = unit->status.preferences[i];
                if (pref->active == 1 &&
                    pref->type == df::unit_preference::T_type::LikeMaterial &&
                    (pref->material == material_type::GEM_ORNAMENTAL || pref->material == material_type::GEM_SEMI || pref->material == material_type::GEM_PRECIOUS || pref->material == material_type::GEM_RARE))
                {
                    unit->job.mood_material.push_back(pref->material);
                    unit->job.mood_matgloss.push_back(pref->matgloss);
                    found_pref = true;
                    break;
                }
            }
            if (!found_pref)
            {
                unit->job.mood_material.push_back(material_type::NONE);
                unit->job.mood_matgloss.push_back(-1);
            }
            break;

        case job_skill::GLASSMAKER:
            unit->job.mood_item_type.push_back(item_type::ROUGH);
            unit->job.mood_item_subtype.push_back(-1);

            found_pref = false;
            for (size_t i = 0; i < unit->status.preferences.size(); i++)
            {
                df::unit_preference *pref = unit->status.preferences[i];
                if (pref->active == 1 &&
                    pref->type == df::unit_preference::T_type::LikeMaterial &&
                    ((pref->material == material_type::GLASS_GREEN) ||
                     (pref->material == material_type::GLASS_CLEAR) ||
                     (pref->material == material_type::GLASS_CRYSTAL)))
                {
                    unit->job.mood_material.push_back(pref->material);
                    unit->job.mood_matgloss.push_back(pref->matgloss);
                    found_pref = true;
                }
            }
            if (!found_pref)
            {
                switch (rng.df_trandom(3))
                {
                case 0:
                    unit->job.mood_material.push_back(material_type::GLASS_GREEN);
                    break;
                case 1:
                    unit->job.mood_material.push_back(material_type::GLASS_CLEAR);
                    break;
                case 2:
                    unit->job.mood_material.push_back(material_type::GLASS_CRYSTAL);
                    break;
                }
            }
            unit->job.mood_matgloss.push_back(-1);
            break;

        case job_skill::BONECARVE:
            found_pref = false;
            for (size_t i = 0; i < unit->status.preferences.size(); i++)
            {
                df::unit_preference *pref = unit->status.preferences[i];
                if (pref->active == 1 &&
                    pref->type == df::unit_preference::T_type::LikeMaterial &&
                    (pref->material == material_type::BONE || pref->material == material_type::SHELL))
                {
                    if (pref->material == material_type::BONE)
                        unit->job.mood_item_type.push_back(item_type::BONES);
                    else
                        unit->job.mood_item_type.push_back(item_type::SHELL);
                    unit->job.mood_item_subtype.push_back(-1);
                    unit->job.mood_material.push_back(material_type::NONE);
                    unit->job.mood_matgloss.push_back(-1);
                    found_pref = true;
                    break;
                }
            }
            if (!found_pref)
            {
                unit->job.mood_item_type.push_back(rng.df_trandom(2) ? item_type::SHELL : item_type::BONES);
                unit->job.mood_item_subtype.push_back(-1);
                unit->job.mood_material.push_back(material_type::NONE);
                unit->job.mood_matgloss.push_back(-1);
            }
            break;
        }
    }
    if (job->job_type != job_type::StrangeMoodFell && base_item_count > 1)
    {
        for (int i = 1; i < base_item_count; i++)
        {
            unit->job.mood_item_type.push_back(unit->job.mood_item_type[0]);
            unit->job.mood_item_subtype.push_back(unit->job.mood_item_subtype[0]);
            unit->job.mood_material.push_back(unit->job.mood_material[0]);
            unit->job.mood_matgloss.push_back(unit->job.mood_matgloss[0]);
        }
    }
    

    // Choose additional mood materials
    // Gem cutters/setters using a single gem require nothing else, and fell moods need only their corpse
    if (!(
         (((skill == job_skill::CUTGEM) || (skill == job_skill::ENCRUSTGEM)) && base_item_count == 1) ||
         (job->job_type == job_type::StrangeMoodFell)
       ))
    {
        int extra_items = std::min(rng.df_trandom((ui->tasks.num_artifacts * 20 + moodable_units.size()) / 20 + 1), 7);
        df::item_type avoid_type = item_type::NONE;
        int avoid_glass = 0;
        switch (skill)
        {
        case job_skill::MINING:
        case job_skill::DETAILSTONE:
        case job_skill::MASONRY:
        case job_skill::STONECRAFT:
            avoid_type = item_type::BLOCKS;
            break;
        case job_skill::CARPENTRY:
        case job_skill::WOODCRAFT:
        case job_skill::BOWYER:
            avoid_type = item_type::WOOD;
            break;
        case job_skill::TANNER:
        case job_skill::LEATHERWORK:
            avoid_type = item_type::SKIN_TANNED;
            break;
        case job_skill::WEAVING:
        case job_skill::CLOTHESMAKING:
            avoid_type = item_type::CLOTH;
            break;
        case job_skill::SMELT:
        case job_skill::FORGE_WEAPON:
        case job_skill::FORGE_ARMOR:
        case job_skill::FORGE_FURNITURE:
        case job_skill::METALCRAFT:
            avoid_type = item_type::BAR;
            break;
        case job_skill::CUTGEM:
        case job_skill::ENCRUSTGEM:
            avoid_type = item_type::SMALLGEM;
        case job_skill::GLASSMAKER:
            avoid_glass = 1;
            break;
        }
        for (size_t i = 0; i < extra_items; i++)
        {
            if ((job->job_type == job_type::StrangeMoodBrooding) && (rng.df_trandom(2)))
            {
                switch (rng.df_trandom(3))
                {
                case 0:
                    unit->job.mood_item_type.push_back(item_type::REMAINS);
                    break;
                case 1:
                    unit->job.mood_item_type.push_back(item_type::BONES);
                    break;
                case 2:
                    unit->job.mood_item_type.push_back(item_type::SKULL);
                    break;
                }
                unit->job.mood_item_subtype.push_back(-1);
                unit->job.mood_material.push_back(material_type::NONE);
                unit->job.mood_matgloss.push_back(-1);
            }
            else
            {
                df::item_type item_type;
                df::material_type material;
                do
                {
                    item_type = item_type::NONE;
                    material = material_type::NONE;
                    switch (rng.df_trandom(12))
                    {
                    case 0:
                        item_type = item_type::WOOD;
                        material = material_type::NONE;
                        break;
                    case 1:
                        item_type = item_type::BAR;
                        switch ((deepest_dug_tile < 265) ? (rng.df_trandom(5) + 5) : rng.df_trandom(10))
                        {
                        case 0:
                            material = material_type::IRON;
                            break;
                        case 1:
                            material = material_type::GOLD;
                            break;
                        case 2:
                            material = material_type::STEEL;
                            break;
                        case 3:
                            if (ui->nobles_enabled[profession::DUNGEONMASTER])
                                material = material_type::ELECTRUM;
                            else
                                material = material_type::SILVER;
                            break;
                        case 4:
                            material = material_type::PLATINUM;
                            break;
                        case 5:
                            material = material_type::SILVER;
                            break;
                        case 6:
                            material = material_type::COPPER;
                            break;
                        case 7:
                            material = material_type::BRONZE;
                            break;
                        case 8:
                            material = material_type::BRASS;
                            break;
                        case 9:
                            material = material_type::TIN;
                            break;
                        }
                        break;
                    case 2:
                        item_type = item_type::SMALLGEM;
                        material = material_type::NONE;
                        break;
                    case 3:
                        item_type = item_type::BLOCKS;
                        switch (rng.df_trandom(3))
                        {
                        case 0:
                            material = material_type::STONE_GRAY;
                            break;
                        case 1:
                            material = material_type::STONE_DARK;
                            break;
                        case 2:
                            material = material_type::STONE_LIGHT;
                            break;
                        }
                        break;
                    case 4:
                        item_type = item_type::ORE;
                        switch ((deepest_dug_tile < 265) ? (rng.df_trandom(4) + 3) : rng.df_trandom(7))
                        {
                        case 0:
                            material = material_type::IRON;
                            break;
                        case 1:
                            material = material_type::GOLD;
                            break;
                        case 2:
                            material = material_type::PLATINUM;
                            break;
                        case 3:
                            material = material_type::SILVER;
                            break;
                        case 4:
                            material = material_type::COPPER;
                            break;
                        case 5:
                            material = material_type::ZINC;
                            break;
                        case 6:
                            material = material_type::TIN;
                            break;
                        }
                        break;

                    case 5:
                        item_type = item_type::ROUGH;
                        material = material_type::NONE;
                        break;
                    case 6:
                        item_type = item_type::STONE;
                        switch (rng.df_trandom(3))
                        {
                        case 0:
                            material = material_type::STONE_GRAY;
                            break;
                        case 1:
                            material = material_type::STONE_DARK;
                            break;
                        case 2:
                            material = material_type::STONE_LIGHT;
                            break;
                        }
                        break;
                    case 7:
                        item_type = item_type::BONES;
                        material = material_type::NONE;
                        break;
                    case 8:
                        item_type = item_type::SHELL;
                        material = material_type::NONE;
                        break;
                    case 9:
                        item_type = item_type::SKIN_TANNED;
                        material = material_type::NONE;
                        break;
                    case 10:
                        item_type = item_type::CLOTH;
                        switch (rng.df_trandom(2))
                        {
                        case 0:
                            material = material_type::PLANT;
                            break;
                        case 1:
                            material = material_type::SILK;
                            break;
                        }
                        break;
                    case 11:
                        item_type = item_type::ROUGH;
                        switch (rng.df_trandom(3))
                        {
                        case 0:
                            material = material_type::GLASS_GREEN;
                            break;
                        case 1:
                            material = material_type::GLASS_CLEAR;
                            break;
                        case 2:
                            material = material_type::GLASS_CRYSTAL;
                            break;
                        }
                        break;
                    }
                    if (unit->job.mood_item_type[0] == item_type && unit->job.mood_material[0] == material)
                        continue;
                    if (item_type == avoid_type)
                        continue;
                    if (avoid_glass && ((material == material_type::GLASS_GREEN) || (material == material_type::GLASS_CLEAR) || (material == material_type::GLASS_CRYSTAL)))
                        continue;
                    break;
                } while (1);

                unit->job.mood_item_type.push_back(item_type);
                unit->job.mood_item_subtype.push_back(-1);
                unit->job.mood_material.push_back(material);
                unit->job.mood_matgloss.push_back(-1);
            }
        }
    }

    // Attach the Strange Mood job to the dwarf
    unit->path.dest.x = -1;
    job->flags.bits.special = true;
    df::general_ref *ref = df::allocate<df::general_ref_unit_workerst>();
    ref->setID(unit->id);
    job->general_refs.push_back(ref);
    unit->job.current_job = job;

    // Generate the artifact's name
    if (type == mood_type::Fell || type == mood_type::Macabre)
        generateName(unit->status.artifact_name, unit->name.language, 1, world->raws.language.word_table[0][2], world->raws.language.word_table[1][2]);
    else
    {
        generateName(unit->status.artifact_name, unit->name.language, 1, world->raws.language.word_table[0][1], world->raws.language.word_table[1][1]);
        if (!rng.df_trandom(100))
            unit->status.artifact_name = unit->name;
    }
    unit->mood_claimedWorkshop = 0;
    return CR_OK;
}

DFHACK_PLUGIN("strangemood");

DFhackCExport command_result plugin_init (color_ostream &out, std::vector<PluginCommand> &commands)
{
    commands.push_back(PluginCommand("strangemood", "Force a strange mood to happen.\n", df_strangemood, false,
        "Options:\n"
         "  -force         - Ignore standard mood preconditions.\n"
         "  -unit          - Use the selected unit instead of picking one randomly.\n"
         "  -type <type>   - Force the mood to be of a specific type.\n"
         "                   Valid types: fey, secretive, possessed, fell, macabre\n"
         "  -skill <skill> - Force the mood to use a specific skill.\n"
         "                   Skill name must be lowercase and without spaces.\n"
         "                   Example: miner, gemcutter, metalcrafter, bonecarver, mason\n"
    ));
    rng.init();

    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( color_ostream &out )
{
    return CR_OK;
}
