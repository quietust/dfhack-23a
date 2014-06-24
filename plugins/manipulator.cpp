// Dwarf Manipulator - a Therapist-style labor editor

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <MiscUtils.h>
#include <modules/Screen.h>
#include <modules/Translation.h>
#include <modules/Units.h>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

#include <VTableInterpose.h>
#include "df/world.h"
#include "df/ui.h"
#include "df/init.h"
#include "df/graphic.h"
#include "df/enabler.h"
#include "df/viewscreen_unitjobsst.h"
#include "df/interface_key.h"
#include "df/unit.h"
#include "df/unit_skill.h"
#include "df/creature_graphics_role.h"
#include "df/creature_raw.h"
#include "df/historical_entity.h"
#include "df/entity_raw.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::init;
using df::global::gps;
using df::global::enabler;

struct SkillLevel
{
    const char *name;
    int points;
    char abbrev;
};

#define NUM_SKILL_LEVELS (sizeof(skill_levels) / sizeof(SkillLevel))

// The various skill rankings. Zero skill is hardcoded to "Not" and '-'.
const SkillLevel skill_levels[] = {
    {"Dabbling",     500, '0'},
    {"Novice",       600, '1'},
    {"Adequate",     700, '2'},
    {"Competent",    800, '3'},
    {"Skilled",      900, '4'},
    {"Proficient",  1000, '5'},
    {"Talented",    1100, '6'},
    {"Adept",       1200, '7'},
    {"Expert",      1300, '8'},
    {"Professional",1400, '9'},
    {"Accomplished",1500, 'A'},
    {"Great",       1600, 'B'},
    {"Master",      1700, 'C'},
    {"High Master", 1800, 'D'},
    {"Grand Master",1900, 'E'},
    {"Legendary",   2000, 'U'},
    {"Legendary+1", 2100, 'V'},
    {"Legendary+2", 2200, 'W'},
    {"Legendary+3", 2300, 'X'},
    {"Legendary+4", 2400, 'Y'},
    {"Legendary+5",    0, 'Z'}
};

const char *armor_names[4] =
{
	"Clothes", "Leather", "Chain", "Plate"
};
const char *shield_names[3] =
{
	"None", "Buckler", "Shield"
};

enum skillcolumn_type {
    SKILL_NORMAL,   // normal
    SKILL_SPECIAL,  // deselect all other special skills when selected, force weapon skill indicated in ExtData
    SKILL_COMBAT,   // radio button rather than toggle, deselects all weapon skills first, disables special skills if weapon doesn't match
    SKILL_ARMOR,    // 0/1/2/3 for Cloth/Leather/Chain/Plate
    SKILL_SHIELD,   // 0/1/2 for None/Buckler/Shield
    SKILL_NUMWEAPON // 1/2
};

struct SkillColumn
{
    int group; // for navigation and mass toggling
    int8_t color; // for column headers
    df::profession profession; // to display graphical tiles
    df::unit_labor labor; // toggled when pressing Enter
    df::job_skill skill; // displayed rating
    char label[3]; // column header
    skillcolumn_type type;
    df::unit_labor extdata;
    bool isValidLabor (df::historical_entity *entity = NULL) const
    {
        if (labor == unit_labor::NONE)
            return false;
        if (entity && entity->entity_raw && !entity->entity_raw->jobs.permitted_labor[labor])
            return false;
        return true;
    }
};

#define NUM_COLUMNS (sizeof(columns) / sizeof(SkillColumn))

// All of the skill/labor columns we want to display.
const SkillColumn columns[] = {
// Mining
    {0, 7, profession::MINER, unit_labor::MINE, job_skill::MINING, "Mi", SKILL_SPECIAL, unit_labor::NONE},
// Woodworking
    {1, 14, profession::CARPENTER, unit_labor::CARPENTER, job_skill::CARPENTRY, "Ca"},
    {1, 14, profession::BOWYER, unit_labor::BOWYER, job_skill::BOWYER, "Bw"},
    {1, 14, profession::WOODCUTTER, unit_labor::CUTWOOD, job_skill::WOODCUTTING, "WC", SKILL_SPECIAL, unit_labor::AXE},
// Stoneworking
    {2, 15, profession::MASON, unit_labor::MASON, job_skill::MASONRY, "Ma"},
    {2, 15, profession::ENGRAVER, unit_labor::DETAIL, job_skill::DETAILSTONE, "En"},
// Hunting/Related
    {3, 2, profession::ANIMAL_TRAINER, unit_labor::ANIMALTRAIN, job_skill::ANIMALTRAIN, "Tn"},
    {3, 2, profession::ANIMAL_CARETAKER, unit_labor::ANIMALCARE, job_skill::ANIMALCARE, "Ca"},
    {3, 2, profession::HUNTER, unit_labor::HUNT, job_skill::SNEAK, "Hu"},
    {3, 2, profession::TRAPPER, unit_labor::TRAPPER, job_skill::TRAPPING, "Tr"},
    {3, 2, profession::ANIMAL_DISSECTOR, unit_labor::DISSECT_VERMIN, job_skill::DISSECT_VERMIN, "Di"},
// Farming/Related
    {4, 6, profession::BUTCHER, unit_labor::BUTCHER, job_skill::BUTCHER, "Bu"},
    {4, 6, profession::TANNER, unit_labor::TANNER, job_skill::TANNER, "Ta"},
    {4, 6, profession::PLANTER, unit_labor::PLANT, job_skill::PLANT, "Gr"},
    {4, 6, profession::DYER, unit_labor::DYER, job_skill::DYER, "Dy"},
    {4, 6, profession::SOAP_MAKER, unit_labor::SOAP_MAKER, job_skill::SOAP_MAKING, "So"},
    {4, 6, profession::WOOD_BURNER, unit_labor::BURN_WOOD, job_skill::WOOD_BURNING, "WB"},
    {4, 6, profession::POTASH_MAKER, unit_labor::POTASH_MAKING, job_skill::POTASH_MAKING, "Po"},
    {4, 6, profession::LYE_MAKER, unit_labor::LYE_MAKING, job_skill::LYE_MAKING, "Ly"},
    {4, 6, profession::MILLER, unit_labor::MILLER, job_skill::MILLING, "Ml"},
    {4, 6, profession::BREWER, unit_labor::BREWER, job_skill::BREWING, "Br"},
    {4, 6, profession::HERBALIST, unit_labor::HERBALIST, job_skill::HERBALISM, "He"},
    {4, 6, profession::THRESHER, unit_labor::PROCESS_PLANT, job_skill::PROCESSPLANTS, "Th"},
    {4, 6, profession::CHEESE_MAKER, unit_labor::MAKE_CHEESE, job_skill::CHEESEMAKING, "Ch"},
    {4, 6, profession::MILKER, unit_labor::MILK, job_skill::MILK, "Mk"},
    {4, 6, profession::COOK, unit_labor::COOK, job_skill::COOK, "Co"},
// Fishing/Related
    {5, 1, profession::FISHERMAN, unit_labor::FISH, job_skill::FISH, "Fi"},
    {5, 1, profession::FISH_CLEANER, unit_labor::CLEAN_FISH, job_skill::PROCESSFISH, "Cl"},
    {5, 1, profession::FISH_DISSECTOR, unit_labor::DISSECT_FISH, job_skill::DISSECT_FISH, "Di"},
// Metalsmithing
    {6, 8, profession::FURNACE_OPERATOR, unit_labor::SMELT, job_skill::SMELT, "Fu"},
    {6, 8, profession::WEAPONSMITH, unit_labor::FORGE_WEAPON, job_skill::FORGE_WEAPON, "We"},
    {6, 8, profession::ARMORER, unit_labor::FORGE_ARMOR, job_skill::FORGE_ARMOR, "Ar"},
    {6, 8, profession::BLACKSMITH, unit_labor::FORGE_FURNITURE, job_skill::FORGE_FURNITURE, "Bl"},
    {6, 8, profession::METALCRAFTER, unit_labor::METAL_CRAFT, job_skill::METALCRAFT, "Cr"},
// Jewelry
    {7, 10, profession::GEM_CUTTER, unit_labor::CUT_GEM, job_skill::CUTGEM, "Cu"},
    {7, 10, profession::GEM_SETTER, unit_labor::ENCRUST_GEM, job_skill::ENCRUSTGEM, "Se"},
// Crafts
    {8, 9, profession::LEATHERWORKER, unit_labor::LEATHER, job_skill::LEATHERWORK, "Le"},
    {8, 9, profession::WOODCRAFTER, unit_labor::WOOD_CRAFT, job_skill::WOODCRAFT, "Wo"},
    {8, 9, profession::STONECRAFTER, unit_labor::STONE_CRAFT, job_skill::STONECRAFT, "St"},
    {8, 9, profession::BONE_CARVER, unit_labor::BONE_CARVE, job_skill::BONECARVE, "Bo"},
    {8, 9, profession::GLASSMAKER, unit_labor::GLASSMAKER, job_skill::GLASSMAKER, "Gl"},
    {8, 9, profession::WEAVER, unit_labor::WEAVER, job_skill::WEAVING, "We"},
    {8, 9, profession::CLOTHIER, unit_labor::CLOTHESMAKER, job_skill::CLOTHESMAKING, "Cl"},
    {8, 9, profession::STRAND_EXTRACTOR, unit_labor::EXTRACT_STRAND, job_skill::EXTRACT_STRAND, "Ad"},
// Engineering
    {9, 12, profession::SIEGE_ENGINEER, unit_labor::SIEGECRAFT, job_skill::SIEGECRAFT, "En"},
    {9, 12, profession::SIEGE_OPERATOR, unit_labor::SIEGEOPERATE, job_skill::SIEGEOPERATE, "Op"},
    {9, 12, profession::MECHANIC, unit_labor::MECHANIC, job_skill::MECHANICS, "Me"},
    {9, 12, profession::PUMP_OPERATOR, unit_labor::OPERATE_PUMP, job_skill::OPERATE_PUMP, "Pu"},
// Hauling
    {10, 3, profession::NONE, unit_labor::HAUL_STONE, job_skill::NONE, "St"},
    {10, 3, profession::NONE, unit_labor::HAUL_WOOD, job_skill::NONE, "Wo"},
    {10, 3, profession::NONE, unit_labor::HAUL_ITEM, job_skill::NONE, "It"},
    {10, 3, profession::NONE, unit_labor::HAUL_BODY, job_skill::NONE, "Bu"},
    {10, 3, profession::NONE, unit_labor::HAUL_FOOD, job_skill::NONE, "Fo"},
    {10, 3, profession::NONE, unit_labor::HAUL_REFUSE, job_skill::NONE, "Re"},
    {10, 3, profession::NONE, unit_labor::HAUL_FURNITURE, job_skill::NONE, "Fu"},
    {10, 3, profession::NONE, unit_labor::HAUL_ANIMAL, job_skill::NONE, "An"},
// Other Jobs
    {11, 4, profession::ARCHITECT, unit_labor::ARCHITECT, job_skill::DESIGNBUILDING, "Ar"},
    {11, 4, profession::ALCHEMIST, unit_labor::ALCHEMIST, job_skill::ALCHEMY, "Al"},
    {11, 4, profession::NONE, unit_labor::HEALTHCARE, job_skill::NONE, "He"},
    {11, 4, profession::NONE, unit_labor::CLEAN, job_skill::NONE, "Cl"},
// Military - Weapons
    {12, 7, profession::WRESTLER, unit_labor::NONE, job_skill::UNARMED, "Wr", SKILL_COMBAT},
    {12, 7, profession::AXEMAN, unit_labor::AXE, job_skill::AXE, "Ax", SKILL_COMBAT},
    {12, 7, profession::SWORDSMAN, unit_labor::SWORD, job_skill::SWORD, "Sw", SKILL_COMBAT},
    {12, 7, profession::MACEMAN, unit_labor::MACE, job_skill::MACE, "Mc", SKILL_COMBAT},
    {12, 7, profession::HAMMERMAN, unit_labor::HAMMER, job_skill::HAMMER, "Ha", SKILL_COMBAT},
    {12, 7, profession::SPEARMAN, unit_labor::SPEAR, job_skill::SPEAR, "Sp", SKILL_COMBAT},
    {12, 7, profession::CROSSBOWMAN, unit_labor::CROSSBOW, job_skill::CROSSBOW, "Cb", SKILL_COMBAT},
    {12, 7, profession::THIEF, unit_labor::DAGGER, job_skill::DAGGER, "Kn", SKILL_COMBAT},
    {12, 7, profession::BOWMAN, unit_labor::BOW, job_skill::BOW, "Bo", SKILL_COMBAT},
    {12, 7, profession::BLOWGUNMAN, unit_labor::BLOWGUN, job_skill::BLOWGUN, "Bl", SKILL_COMBAT},
    {12, 7, profession::PIKEMAN, unit_labor::PIKE, job_skill::PIKE, "Pk", SKILL_COMBAT},
    {12, 7, profession::LASHER, unit_labor::WHIP, job_skill::WHIP, "La", SKILL_COMBAT},
// Military - Other Combat
    {13, 15, profession::NONE, unit_labor::ARMOR, job_skill::ARMOR, "Ar", SKILL_ARMOR},
    {13, 15, profession::NONE, unit_labor::SHIELD, job_skill::SHIELD, "Sh", SKILL_SHIELD},
    {13, 15, profession::NONE, unit_labor::WEAPON_NUMBER, job_skill::NONE, "W#", SKILL_NUMWEAPON},
// Social
    {14, 3, profession::NONE, unit_labor::NONE, job_skill::PERSUASION, "Pe"},
    {14, 3, profession::NONE, unit_labor::NONE, job_skill::NEGOTIATION, "Ne"},
    {14, 3, profession::NONE, unit_labor::NONE, job_skill::JUDGING_INTENT, "Ju"},
    {14, 3, profession::NONE, unit_labor::NONE, job_skill::LYING, "Li"},
    {14, 3, profession::NONE, unit_labor::NONE, job_skill::INTIMIDATION, "In"},
    {14, 3, profession::NONE, unit_labor::NONE, job_skill::CONVERSATION, "Cn"},
    {14, 3, profession::NONE, unit_labor::NONE, job_skill::COMEDY, "Cm"},
    {14, 3, profession::NONE, unit_labor::NONE, job_skill::FLATTERY, "Fl"},
    {14, 3, profession::NONE, unit_labor::NONE, job_skill::CONSOLE, "Cs"},
    {14, 3, profession::NONE, unit_labor::NONE, job_skill::PACIFY, "Pc"},
// Noble
    {15, 5, profession::TRADER, unit_labor::NONE, job_skill::APPRAISAL, "Ap"},
    {15, 5, profession::ADMINISTRATOR, unit_labor::NONE, job_skill::ORGANIZATION, "Or"},
    {15, 5, profession::CLERK, unit_labor::NONE, job_skill::RECORD_KEEPING, "RK"},
// Miscellaneous
    {16, 3, profession::NONE, unit_labor::NONE, job_skill::THROW, "Th"},
    {16, 3, profession::NONE, unit_labor::NONE, job_skill::SWIMMING, "Sw"},
    {16, 5, profession::NONE, unit_labor::NONE, job_skill::TRACKING, "Tr"},
    {16, 5, profession::NONE, unit_labor::NONE, job_skill::MAGIC_NATURE, "Dr"},
};

struct UnitInfo
{
    df::unit *unit;
    bool allowEdit;
    string name;
    string transname;
    string profession;
    int8_t color;
    int active_index;
};

enum altsort_mode {
    ALTSORT_NAME,
    ALTSORT_PROFESSION,
    ALTSORT_HAPPINESS,
    ALTSORT_ARRIVAL,
    ALTSORT_MAX
};

bool descending;
df::job_skill sort_skill;
df::unit_labor sort_labor;

bool sortByName (const UnitInfo *d1, const UnitInfo *d2)
{
    if (descending)
        return (d1->name > d2->name);
    else
        return (d1->name < d2->name);
}

bool sortByProfession (const UnitInfo *d1, const UnitInfo *d2)
{
    if (descending)
        return (d1->profession > d2->profession);
    else
        return (d1->profession < d2->profession);
}

bool sortByHappiness (const UnitInfo *d1, const UnitInfo *d2)
{
    if (descending)
        return (d1->unit->status.happiness > d2->unit->status.happiness);
    else
        return (d1->unit->status.happiness < d2->unit->status.happiness);
}

bool sortByArrival (const UnitInfo *d1, const UnitInfo *d2)
{
    if (descending)
        return (d1->active_index > d2->active_index);
    else
        return (d1->active_index < d2->active_index);
}

bool sortBySkill (const UnitInfo *d1, const UnitInfo *d2)
{
    if (sort_skill != job_skill::NONE)
    {
        df::unit_skill *s1 = linear_in_vector<df::unit_skill,df::job_skill>(d1->unit->status.skills, &df::unit_skill::id, sort_skill);
        df::unit_skill *s2 = linear_in_vector<df::unit_skill,df::job_skill>(d2->unit->status.skills, &df::unit_skill::id, sort_skill);
        int l1 = s1 ? s1->rating : 0;
        int l2 = s2 ? s2->rating : 0;
        int e1 = s1 ? s1->experience : 0;
        int e2 = s2 ? s2->experience : 0;
        if (descending)
        {
            if (l1 != l2)
                return l1 > l2;
            if (e1 != e2)
                return e1 > e2;
        }
        else
        {
            if (l1 != l2)
                return l1 < l2;
            if (e1 != e2)
                return e1 < e2;
        }
    }
    if (sort_labor != unit_labor::NONE)
    {
        if (descending)
            return d1->unit->status.labors[sort_labor] > d2->unit->status.labors[sort_labor];
        else
            return d1->unit->status.labors[sort_labor] < d2->unit->status.labors[sort_labor];
    }
    return sortByName(d1, d2);
}

enum display_columns {
    DISP_COLUMN_HAPPINESS,
    DISP_COLUMN_NAME,
    DISP_COLUMN_PROFESSION,
    DISP_COLUMN_LABORS,
    DISP_COLUMN_MAX,
};

class viewscreen_unitlaborsst : public dfhack_viewscreen {
public:
    void feed(std::set<df::interface_key> *keys);

    void logic() {
        dfhack_viewscreen::logic();
        if (do_refresh_names)
            refreshNames();
    }

    void render();

    void help() { }

    std::string getFocusString() { return "unitlabors"; }

    df::unit *getSelectedUnit();

    viewscreen_unitlaborsst(stl::vector<df::unit*> &src, int cursor_pos);
    ~viewscreen_unitlaborsst() { };

protected:
    vector<UnitInfo *> units;
    altsort_mode altsort;

    bool do_refresh_names;
    int first_row, sel_row, num_rows;
    int first_column, sel_column;

    int col_widths[DISP_COLUMN_MAX];
    int col_offsets[DISP_COLUMN_MAX];

    void refreshNames();
    void calcSize ();
};

viewscreen_unitlaborsst::viewscreen_unitlaborsst(stl::vector<df::unit*> &src, int cursor_pos)
{
    std::map<df::unit*,int> active_idx;
    auto &active = world->units.active;
    for (size_t i = 0; i < active.size(); i++)
        active_idx[active[i]] = i;

    for (size_t i = 0; i < src.size(); i++)
    {
        df::unit *unit = src[i];
        if (!unit)
        {
            if (cursor_pos > i)
                cursor_pos--;
            continue;
        }

        UnitInfo *cur = new UnitInfo;

        cur->unit = unit;
        cur->allowEdit = true;
        cur->active_index = active_idx[unit];

        if (unit->race != ui->race_id)
            cur->allowEdit = false;

        if (unit->civ_id != ui->civ_id)
            cur->allowEdit = false;

        if (unit->flags1.bits.dead)
            cur->allowEdit = false;

        if (!ENUM_ATTR(profession, can_assign_labor, unit->profession))
            cur->allowEdit = false;

        cur->color = Units::getProfessionColor(unit);

        units.push_back(cur);
    }
    altsort = ALTSORT_NAME;
    first_column = sel_column = 0;

    refreshNames();

    first_row = 0;
    sel_row = cursor_pos;
    calcSize();

    // recalculate first_row to roughly match the original layout
    first_row = 0;
    while (first_row < sel_row - num_rows + 1)
        first_row += num_rows + 2;
    // make sure the selection stays visible
    if (first_row > sel_row)
        first_row = sel_row - num_rows + 1;
    // don't scroll beyond the end
    if (first_row > units.size() - num_rows)
        first_row = units.size() - num_rows;
}

void viewscreen_unitlaborsst::refreshNames()
{
    do_refresh_names = false;

    for (size_t i = 0; i < units.size(); i++)
    {
        UnitInfo *cur = units[i];
        df::unit *unit = cur->unit;

        cur->name = Translation::TranslateName(Units::getVisibleName(unit), false);
        cur->transname = Translation::TranslateName(Units::getVisibleName(unit), true);
        cur->profession = Units::getProfessionName(unit);
    }
    calcSize();
}

void viewscreen_unitlaborsst::calcSize()
{
    auto dim = Screen::getWindowSize();

    num_rows = dim.y - 10;
    if (num_rows > units.size())
        num_rows = units.size();

    int num_columns = dim.x - DISP_COLUMN_MAX - 1;

    // min/max width of columns
    int col_minwidth[DISP_COLUMN_MAX];
    int col_maxwidth[DISP_COLUMN_MAX];
    col_minwidth[DISP_COLUMN_HAPPINESS] = 4;
    col_maxwidth[DISP_COLUMN_HAPPINESS] = 4;
    col_minwidth[DISP_COLUMN_NAME] = 16;
    col_maxwidth[DISP_COLUMN_NAME] = 16;        // adjusted in the loop below
    col_minwidth[DISP_COLUMN_PROFESSION] = 10;
    col_maxwidth[DISP_COLUMN_PROFESSION] = 10;  // adjusted in the loop below
    col_minwidth[DISP_COLUMN_LABORS] = num_columns*3/5;     // 60%
    col_maxwidth[DISP_COLUMN_LABORS] = NUM_COLUMNS;

    // get max_name/max_prof from strings length
    for (size_t i = 0; i < units.size(); i++)
    {
        if (col_maxwidth[DISP_COLUMN_NAME] < units[i]->name.size())
            col_maxwidth[DISP_COLUMN_NAME] = units[i]->name.size();
        if (col_maxwidth[DISP_COLUMN_PROFESSION] < units[i]->profession.size())
            col_maxwidth[DISP_COLUMN_PROFESSION] = units[i]->profession.size();
    }

    // check how much room we have
    int width_min = 0, width_max = 0;
    for (int i = 0; i < DISP_COLUMN_MAX; i++)
    {
        width_min += col_minwidth[i];
        width_max += col_maxwidth[i];
    }

    if (width_max <= num_columns)
    {
        // lots of space, distribute leftover (except last column)
        int col_margin   = (num_columns - width_max) / (DISP_COLUMN_MAX-1);
        int col_margin_r = (num_columns - width_max) % (DISP_COLUMN_MAX-1);
        for (int i = DISP_COLUMN_MAX-1; i>=0; i--)
        {
            col_widths[i] = col_maxwidth[i];

            if (i < DISP_COLUMN_MAX-1)
            {
                col_widths[i] += col_margin;

                if (col_margin_r)
                {
                    col_margin_r--;
                    col_widths[i]++;
                }
            }
        }
    }
    else if (width_min <= num_columns)
    {
        // constrained, give between min and max to every column
        int space = num_columns - width_min;
        // max size columns not yet seen may consume
        int next_consume_max = width_max - width_min;

        for (int i = 0; i < DISP_COLUMN_MAX; i++)
        {
            // divide evenly remaining space
            int col_margin = space / (DISP_COLUMN_MAX-i);

            // take more if the columns after us cannot
            next_consume_max -= (col_maxwidth[i]-col_minwidth[i]);
            if (col_margin < space-next_consume_max)
                col_margin = space - next_consume_max;

            // no more than maxwidth
            if (col_margin > col_maxwidth[i] - col_minwidth[i])
                col_margin = col_maxwidth[i] - col_minwidth[i];

            col_widths[i] = col_minwidth[i] + col_margin;

            space -= col_margin;
        }
    }
    else
    {
        // should not happen, min screen is 80x25
        int space = num_columns;
        for (int i = 0; i < DISP_COLUMN_MAX; i++)
        {
            col_widths[i] = space / (DISP_COLUMN_MAX-i);
            space -= col_widths[i];
        }
    }

    for (int i = 0; i < DISP_COLUMN_MAX; i++)
    {
        if (i == 0)
            col_offsets[i] = 1;
        else
            col_offsets[i] = col_offsets[i - 1] + col_widths[i - 1] + 1;
    }

    // don't adjust scroll position immediately after the window opened
    if (units.size() == 0)
        return;

    // if the window grows vertically, scroll upward to eliminate blank rows from the bottom
    if (first_row > units.size() - num_rows)
        first_row = units.size() - num_rows;

    // if it shrinks vertically, scroll downward to keep the cursor visible
    if (first_row < sel_row - num_rows + 1)
        first_row = sel_row - num_rows + 1;

    // if the window grows horizontally, scroll to the left to eliminate blank columns from the right
    if (first_column > NUM_COLUMNS - col_widths[DISP_COLUMN_LABORS])
        first_column = NUM_COLUMNS - col_widths[DISP_COLUMN_LABORS];

    // if it shrinks horizontally, scroll to the right to keep the cursor visible
    if (first_column < sel_column - col_widths[DISP_COLUMN_LABORS] + 1)
        first_column = sel_column - col_widths[DISP_COLUMN_LABORS] + 1;
}

void viewscreen_unitlaborsst::feed(std::set<df::interface_key> *events)
{
    bool leave_all = events->count(interface_key::LEAVESCREEN_ALL);
    if (leave_all || events->count(interface_key::LEAVESCREEN))
    {
        events->clear();
        Screen::dismiss(this);
        if (leave_all)
            parent->view();
        return;
    }

    if (!units.size())
        return;

    df::historical_entity *cur_entity = df::historical_entity::find(ui->civ_id);

    if (do_refresh_names)
        refreshNames();

    int old_sel_row = sel_row;

    if (events->count(interface_key::CURSOR_UP) || events->count(interface_key::CURSOR_UPLEFT) || events->count(interface_key::CURSOR_UPRIGHT))
        sel_row--;
    if (events->count(interface_key::CURSOR_UP_FAST) || events->count(interface_key::CURSOR_UPLEFT_FAST) || events->count(interface_key::CURSOR_UPRIGHT_FAST))
        sel_row -= 10;
    if (events->count(interface_key::CURSOR_DOWN) || events->count(interface_key::CURSOR_DOWNLEFT) || events->count(interface_key::CURSOR_DOWNRIGHT))
        sel_row++;
    if (events->count(interface_key::CURSOR_DOWN_FAST) || events->count(interface_key::CURSOR_DOWNLEFT_FAST) || events->count(interface_key::CURSOR_DOWNRIGHT_FAST))
        sel_row += 10;

    if ((sel_row > 0) && events->count(interface_key::CURSOR_UP_Z_AUX))
    {
        sel_row = 0;
    }
    if ((sel_row < units.size()-1) && events->count(interface_key::CURSOR_DOWN_Z_AUX))
    {
        sel_row = units.size()-1;
    }

    if (sel_row < 0)
    {
        if (old_sel_row == 0 && events->count(interface_key::CURSOR_UP))
            sel_row = units.size() - 1;
        else
            sel_row = 0;
    }

    if (sel_row > units.size() - 1)
    {
        if (old_sel_row == units.size()-1 && events->count(interface_key::CURSOR_DOWN))
            sel_row = 0;
        else
            sel_row = units.size() - 1;
    }

    if (events->count(interface_key::STRING_A000))
        sel_row = 0;

    if (sel_row < first_row)
        first_row = sel_row;
    if (first_row < sel_row - num_rows + 1)
        first_row = sel_row - num_rows + 1;

    if (events->count(interface_key::CURSOR_LEFT) || events->count(interface_key::CURSOR_UPLEFT) || events->count(interface_key::CURSOR_DOWNLEFT))
        sel_column--;
    if (events->count(interface_key::CURSOR_LEFT_FAST) || events->count(interface_key::CURSOR_UPLEFT_FAST) || events->count(interface_key::CURSOR_DOWNLEFT_FAST))
        sel_column -= 10;
    if (events->count(interface_key::CURSOR_RIGHT) || events->count(interface_key::CURSOR_UPRIGHT) || events->count(interface_key::CURSOR_DOWNRIGHT))
        sel_column++;
    if (events->count(interface_key::CURSOR_RIGHT_FAST) || events->count(interface_key::CURSOR_UPRIGHT_FAST) || events->count(interface_key::CURSOR_DOWNRIGHT_FAST))
        sel_column += 10;

    if ((sel_column != 0) && events->count(interface_key::CURSOR_UP_Z))
    {
        // go to beginning of current column group; if already at the beginning, go to the beginning of the previous one
        sel_column--;
        int cur = columns[sel_column].group;
        while ((sel_column > 0) && columns[sel_column - 1].group == cur)
            sel_column--;
    }
    if ((sel_column != NUM_COLUMNS - 1) && events->count(interface_key::CURSOR_DOWN_Z))
    {
        // go to beginning of next group
        int cur = columns[sel_column].group;
        int next = sel_column+1;
        while ((next < NUM_COLUMNS) && (columns[next].group == cur))
            next++;
        if ((next < NUM_COLUMNS) && (columns[next].group != cur))
            sel_column = next;
    }

    if (events->count(interface_key::STRING_A000))
        sel_column = 0;

    if (sel_column < 0)
        sel_column = 0;
    if (sel_column > NUM_COLUMNS - 1)
        sel_column = NUM_COLUMNS - 1;

    if (events->count(interface_key::CURSOR_DOWN_Z) || events->count(interface_key::CURSOR_UP_Z))
    {
        // when moving by group, ensure the whole group is shown onscreen
        int endgroup_column = sel_column;
        while ((endgroup_column < NUM_COLUMNS-1) && columns[endgroup_column+1].group == columns[sel_column].group)
            endgroup_column++;

        if (first_column < endgroup_column - col_widths[DISP_COLUMN_LABORS] + 1)
            first_column = endgroup_column - col_widths[DISP_COLUMN_LABORS] + 1;
    }

    if (sel_column < first_column)
        first_column = sel_column;
    if (first_column < sel_column - col_widths[DISP_COLUMN_LABORS] + 1)
        first_column = sel_column - col_widths[DISP_COLUMN_LABORS] + 1;

    int input_row = sel_row;
    int input_column = sel_column;
    int input_sort = altsort;

    df::coord2d mouse = Screen::getMousePos();

    // Translate mouse input to appropriate keyboard input
    if (mouse.x != -1 && mouse.y != -1)
    {
        int click_header = DISP_COLUMN_MAX; // group ID of the column header clicked
        int click_body = DISP_COLUMN_MAX; // group ID of the column body clicked

        int click_unit = -1; // Index into units[] (-1 if out of range)
        int click_labor = -1; // Index into columns[] (-1 if out of range)

        for (int i = 0; i < DISP_COLUMN_MAX; i++)
        {
            if ((mouse.x >= col_offsets[i]) &&
                (mouse.x < col_offsets[i] + col_widths[i]))
            {
                if ((mouse.y >= 1) && (mouse.y <= 2))
                    click_header = i;
                if ((mouse.y >= 4) && (mouse.y <= 4 + num_rows))
                    click_body = i;
            }
        }

        if ((mouse.x >= col_offsets[DISP_COLUMN_LABORS]) &&
            (mouse.x < col_offsets[DISP_COLUMN_LABORS] + col_widths[DISP_COLUMN_LABORS]))
            click_labor = mouse.x - col_offsets[DISP_COLUMN_LABORS] + first_column;
        if ((mouse.y >= 4) && (mouse.y <= 4 + num_rows))
            click_unit = mouse.y - 4 + first_row;

        switch (click_header)
        {
        case DISP_COLUMN_HAPPINESS:
            if (enabler->mouse_lbut || enabler->mouse_rbut)
            {
                input_sort = ALTSORT_HAPPINESS;
                if (enabler->mouse_lbut)
                    events->insert(interface_key::SECONDSCROLL_PAGEUP);
                if (enabler->mouse_rbut)
                    events->insert(interface_key::SECONDSCROLL_PAGEDOWN);
            }
            break;

        case DISP_COLUMN_NAME:
            if (enabler->mouse_lbut || enabler->mouse_rbut)
            {
                input_sort = ALTSORT_NAME;
                if (enabler->mouse_lbut)
                    events->insert(interface_key::SECONDSCROLL_PAGEDOWN);
                if (enabler->mouse_rbut)
                    events->insert(interface_key::SECONDSCROLL_PAGEUP);
            }
            break;

        case DISP_COLUMN_PROFESSION:
            if (enabler->mouse_lbut || enabler->mouse_rbut)
            {
                input_sort = ALTSORT_PROFESSION;
                if (enabler->mouse_lbut)
                    events->insert(interface_key::SECONDSCROLL_PAGEDOWN);
                if (enabler->mouse_rbut)
                    events->insert(interface_key::SECONDSCROLL_PAGEUP);
            }
            break;

        case DISP_COLUMN_LABORS:
            if (enabler->mouse_lbut || enabler->mouse_rbut)
            {
                input_column = click_labor;
                if (enabler->mouse_lbut)
                    events->insert(interface_key::SECONDSCROLL_UP);
                if (enabler->mouse_rbut)
                    events->insert(interface_key::SECONDSCROLL_DOWN);
            }
            break;
        }

        switch (click_body)
        {
        case DISP_COLUMN_HAPPINESS:
            // do nothing
            break;

        case DISP_COLUMN_NAME:
        case DISP_COLUMN_PROFESSION:
            // left-click to view, right-click to zoom
            if (enabler->mouse_lbut)
            {
                input_row = click_unit;
                events->insert(interface_key::UNITJOB_VIEW);
                Screen::setKeyPressed(interface_key::UNITJOB_VIEW);
            }
            if (enabler->mouse_rbut)
            {
                input_row = click_unit;
                events->insert(interface_key::UNITJOB_ZOOM_CRE);
                Screen::setKeyPressed(interface_key::UNITJOB_ZOOM_CRE);
            }
            break;

        case DISP_COLUMN_LABORS:
            // left-click to toggle, right-click to just highlight
            if (enabler->mouse_lbut || enabler->mouse_rbut)
            {
                if (enabler->mouse_lbut)
                {
                    input_row = click_unit;
                    input_column = click_labor;
                    events->insert(interface_key::SELECT);
                }
                if (enabler->mouse_rbut)
                {
                    sel_row = click_unit;
                    sel_column = click_labor;
                }
            }
            break;
        }
        enabler->mouse_lbut = enabler->mouse_rbut = 0;
    }

    UnitInfo *cur = units[input_row];
    if (events->count(interface_key::SELECT))
    {
        df::unit *unit = cur->unit;
        const SkillColumn &col = columns[input_column];
        int8_t newstatus = (unit->status.labors[col.labor] ? 0 : 1);
        switch (col.type)
        {
        case SKILL_NORMAL:
            if (!col.isValidLabor(cur_entity))
                break;
            if (!cur->allowEdit)
                break;
            unit->status.labors[col.labor] = newstatus;
            break;

        case SKILL_SPECIAL:
            if (!col.isValidLabor(cur_entity))
                break;
            if (!cur->allowEdit)
                break;
            if (unit->status.labors[col.labor] == 0)
            {
                for (int i = 0; i < NUM_COLUMNS; i++)
                {
                    if (columns[i].isValidLabor(cur_entity) && (columns[i].type == SKILL_SPECIAL))
                        unit->status.labors[columns[i].labor] = 0;
                }
            }
            unit->status.labors[col.labor] = newstatus;
            if (!newstatus)
                break;
            for (int i = 0; i < NUM_COLUMNS; i++)
            {
                if (columns[i].type != SKILL_COMBAT)
                    continue;
                if (columns[i].labor == unit_labor::NONE)
                    continue;
                unit->status.labors[columns[i].labor] = (columns[i].labor == col.extdata) ? 1 : 0;
            }
            break;

        case SKILL_COMBAT:
            if (!cur->allowEdit)
                break;
            for (int i = 0; i < NUM_COLUMNS; i++)
            {
                if (columns[i].labor == unit_labor::NONE)
                    continue;
                if ((columns[i].type == SKILL_SPECIAL) && columns[i].isValidLabor(cur_entity) && (columns[i].extdata != col.labor))
                    unit->status.labors[columns[i].labor] = 0;
                if (columns[i].type == SKILL_COMBAT)
                    unit->status.labors[columns[i].labor] = (columns[i].labor == col.labor) ? 1 : 0;
            }
            break;

        case SKILL_ARMOR:
            if (!cur->allowEdit)
                break;
            unit->status.labors[col.labor]++;
            unit->status.labors[col.labor] %= 4;
            break;

        case SKILL_SHIELD:
            if (!cur->allowEdit)
                break;
            unit->status.labors[col.labor]++;
            unit->status.labors[col.labor] %= 3;
            break;

        case SKILL_NUMWEAPON:
            if (!cur->allowEdit)
                break;
            if (unit->status.labors[col.labor] == 1)
                unit->status.labors[col.labor] = 2;
            else
                unit->status.labors[col.labor] = 1;
            break;
        }
    }
    if (events->count(interface_key::SELECT_ALL) && (cur->allowEdit) && columns[input_column].isValidLabor(cur_entity))
    {
        df::unit *unit = cur->unit;
        const SkillColumn &col = columns[input_column];
        int8_t newstatus = (unit->status.labors[col.labor] ? 0 : 1);
        for (int i = 0; i < NUM_COLUMNS; i++)
        {
            if (columns[i].group != col.group)
                continue;
            switch (columns[i].type)
            {
            case SKILL_NORMAL:
                if (!columns[i].isValidLabor(cur_entity))
                    break;
                unit->status.labors[columns[i].labor] = newstatus;
                break;

            case SKILL_SPECIAL:
                if (!columns[i].isValidLabor(cur_entity))
                    break;
                if (newstatus)
                {
                    for (int j = 0; j < NUM_COLUMNS; j++)
                    {
                        if (columns[j].isValidLabor(cur_entity) && (columns[j].type == SKILL_SPECIAL))
                            unit->status.labors[columns[j].labor] = 0;
                    }
                }
                unit->status.labors[columns[i].labor] = newstatus;
                if (!newstatus)
                    break;
                for (int j = 0; j < NUM_COLUMNS; j++)
                {
                    if (columns[j].type != SKILL_COMBAT)
                        continue;
                    if (columns[j].labor == unit_labor::NONE)
                        continue;
                    unit->status.labors[columns[j].labor] = (columns[j].labor == columns[i].extdata) ? 1 : 0;
                }
                break;
            }
        }
    }

    if (events->count(interface_key::SECONDSCROLL_UP) || events->count(interface_key::SECONDSCROLL_DOWN))
    {
        descending = events->count(interface_key::SECONDSCROLL_UP);
        sort_skill = columns[input_column].skill;
        sort_labor = columns[input_column].labor;
        std::sort(units.begin(), units.end(), sortBySkill);
    }

    if (events->count(interface_key::SECONDSCROLL_PAGEUP) || events->count(interface_key::SECONDSCROLL_PAGEDOWN))
    {
        descending = events->count(interface_key::SECONDSCROLL_PAGEUP);
        switch (input_sort)
        {
        case ALTSORT_NAME:
            std::sort(units.begin(), units.end(), sortByName);
            break;
        case ALTSORT_PROFESSION:
            std::sort(units.begin(), units.end(), sortByProfession);
            break;
        case ALTSORT_HAPPINESS:
            std::sort(units.begin(), units.end(), sortByHappiness);
            break;
        case ALTSORT_ARRIVAL:
            std::sort(units.begin(), units.end(), sortByArrival);
            break;
        }
    }
    if (events->count(interface_key::CHANGETAB))
    {
        switch (altsort)
        {
        case ALTSORT_NAME:
            altsort = ALTSORT_PROFESSION;
            break;
        case ALTSORT_PROFESSION:
            altsort = ALTSORT_HAPPINESS;
            break;
        case ALTSORT_HAPPINESS:
            altsort = ALTSORT_ARRIVAL;
            break;
        case ALTSORT_ARRIVAL:
            altsort = ALTSORT_NAME;
            break;
        }
    }

    if (VIRTUAL_CAST_VAR(unitjobs, df::viewscreen_unitjobsst, parent))
    {
        if (events->count(interface_key::UNITJOB_VIEW) || events->count(interface_key::UNITJOB_ZOOM_CRE))
        {
            for (int i = 0; i < unitjobs->units.size(); i++)
            {
                if (unitjobs->units[i] == units[input_row]->unit)
                {
                    df::viewscreen_unitjobsst::T_mode old_mode = unitjobs->mode;
                    unitjobs->mode = df::viewscreen_unitjobsst::T_mode::Units;
                    unitjobs->cursor_pos = i;
                    unitjobs->view();
                    unitjobs->mode = old_mode;
                    if (Screen::isDismissed(unitjobs))
                        Screen::dismiss(this);
                    else
                        do_refresh_names = true;
                    break;
                }
            }
        }
    }
}

void OutputString(int8_t color, int &x, int y, const std::string &text)
{
    Screen::paintString(Screen::Pen(' ', color, 0), x, y, text);
    x += text.length();
}
void viewscreen_unitlaborsst::render()
{
    if (Screen::isDismissed(this))
        return;

    df::historical_entity *cur_entity = df::historical_entity::find(ui->civ_id);

    auto dim = Screen::getWindowSize();

    Screen::clear();
    Screen::drawBorder("  Dwarf Manipulator - Manage Labors  ");

    Screen::paintString(Screen::Pen(' ', 7, 0), col_offsets[DISP_COLUMN_HAPPINESS], 2, "Hap.");
    Screen::paintString(Screen::Pen(' ', 7, 0), col_offsets[DISP_COLUMN_NAME], 2, "Name");
    Screen::paintString(Screen::Pen(' ', 7, 0), col_offsets[DISP_COLUMN_PROFESSION], 2, "Profession");

    for (int col = 0; col < col_widths[DISP_COLUMN_LABORS]; col++)
    {
        int col_offset = col + first_column;
        if (col_offset >= NUM_COLUMNS)
            break;

        int8_t fg = columns[col_offset].color;
        int8_t bg = 0;

        if (col_offset == sel_column)
        {
            fg = 0;
            bg = 7;
        }

        Screen::paintTile(Screen::Pen(columns[col_offset].label[0], fg, bg), col_offsets[DISP_COLUMN_LABORS] + col, 1);
        Screen::paintTile(Screen::Pen(columns[col_offset].label[1], fg, bg), col_offsets[DISP_COLUMN_LABORS] + col, 2);
        df::profession profession = columns[col_offset].profession;
        if ((profession != profession::NONE) && (ui->race_id != -1))
        {
            auto graphics = world->raws.creatures[ui->race_id]->graphics;
            Screen::paintTile(
                Screen::Pen(' ', fg, 0,
                    graphics.profession_add_color[creature_graphics_role::DEFAULT][profession],
                    graphics.profession_texpos[creature_graphics_role::DEFAULT][profession]),
                col_offsets[DISP_COLUMN_LABORS] + col, 3);
        }
    }

    for (int row = 0; row < num_rows; row++)
    {
        int row_offset = row + first_row;
        if (row_offset >= units.size())
            break;

        UnitInfo *cur = units[row_offset];
        df::unit *unit = cur->unit;
        int8_t fg = 15, bg = 0;

        int happy = cur->unit->status.happiness;
        string happiness = stl_sprintf("%4i", happy);
        if (happy == 0)         // miserable
            fg = 13;    // 5:1
        else if (happy <= 25)   // very unhappy
            fg = 12;    // 4:1
        else if (happy <= 50)   // unhappy
            fg = 4;     // 4:0
        else if (happy < 75)    // fine
            fg = 14;    // 6:1
        else if (happy < 125)   // quite content
            fg = 6;     // 6:0
        else if (happy < 150)   // happy
            fg = 2;     // 2:0
        else                    // ecstatic
            fg = 10;    // 2:1
        Screen::paintString(Screen::Pen(' ', fg, bg), col_offsets[DISP_COLUMN_HAPPINESS], 4 + row, happiness);

        fg = 15;
        if (row_offset == sel_row)
        {
            fg = 0;
            bg = 7;
        }

        string name = cur->name;
        name.resize(col_widths[DISP_COLUMN_NAME]);
        Screen::paintString(Screen::Pen(' ', fg, bg), col_offsets[DISP_COLUMN_NAME], 4 + row, name);

        string profession = cur->profession;
        profession.resize(col_widths[DISP_COLUMN_PROFESSION]);
        fg = cur->color;
        bg = 0;

        Screen::paintString(Screen::Pen(' ', fg, bg), col_offsets[DISP_COLUMN_PROFESSION], 4 + row, profession);

        // Print unit's skills and labor assignments
        for (int col = 0; col < col_widths[DISP_COLUMN_LABORS]; col++)
        {
            int col_offset = col + first_column;
            fg = 15;
            bg = 0;
            uint8_t c = 0xFA;
            if ((col_offset == sel_column) && (row_offset == sel_row))
                fg = 9;
            if (columns[col_offset].skill != job_skill::NONE)
            {
                df::unit_skill *skill = linear_in_vector<df::unit_skill,df::job_skill>(unit->status.skills, &df::unit_skill::id, columns[col_offset].skill);
                if ((skill != NULL) && (skill->rating || skill->experience))
                {
                    int level = skill->rating;
                    if (level > NUM_SKILL_LEVELS - 1)
                        level = NUM_SKILL_LEVELS - 1;
                    c = skill_levels[level].abbrev;
                }
                else
                    c = '-';
            }
            if (columns[col_offset].labor != unit_labor::NONE)
            {
                if (unit->status.labors[columns[col_offset].labor])
                {
                    bg = 7;
                    if (columns[col_offset].skill == job_skill::NONE)
                        c = 0xF9;
                }
            }
            else
                bg = 3;
            switch (columns[col_offset].type)
            {
            case SKILL_COMBAT:
                if (columns[col_offset].labor == unit_labor::NONE)
                {
                    bg = 7;
                    for (int i = 0; i < NUM_COLUMNS; i++)
                    {
                        if (columns[i].type != SKILL_COMBAT)
                            continue;
                        if (columns[i].labor == unit_labor::NONE)
                            continue;
                        if (unit->status.labors[columns[i].labor])
                        {
                            bg = 0;
                            break;
                        }
                    }
                }
                break;
            case SKILL_ARMOR:
                switch (unit->status.labors[columns[col_offset].labor])
                {
                case 0:
                    bg = 0;
                    break;
                case 1:
                    bg = 1;
                    break;
                case 2:
                    bg = 2;
                    break;
                case 3:
                    bg = 4;
                    break;
                }
                break;
            case SKILL_SHIELD:
                switch (unit->status.labors[columns[col_offset].labor])
                {
                case 0:
                    bg = 0;
                    break;
                case 1:
                    bg = 2;
                    break;
                case 2:
                    bg = 4;
                    break;
                }
                break;
            case SKILL_NUMWEAPON:
                bg = 0;
                c = '0' + unit->status.labors[columns[col_offset].labor];
                break;
            }
            Screen::paintTile(Screen::Pen(c, fg, bg), col_offsets[DISP_COLUMN_LABORS] + col, 4 + row);
        }
    }

    UnitInfo *cur = units[sel_row];
    bool canToggle = false, canToggleGroup = false;
    if (cur != NULL)
    {
        df::unit *unit = cur->unit;
        int x = 1, y = 3 + num_rows + 2;
        Screen::Pen white_pen(' ', 15, 0);

        Screen::paintString(white_pen, x, y, (cur->unit && cur->unit->sex) ? "\x0b" : "\x0c");
        x += 2;
        Screen::paintString(white_pen, x, y, cur->transname);
        x += cur->transname.length();

        if (cur->transname.length())
        {
            Screen::paintString(white_pen, x, y, ", ");
            x += 2;
        }
        Screen::paintString(white_pen, x, y, cur->profession);
        x += cur->profession.length();
        Screen::paintString(white_pen, x, y, ": ");
        x += 2;

        string str;
        if (columns[sel_column].type == SKILL_NUMWEAPON)
        {
            if (unit->status.labors[columns[sel_column].labor] == 2)
                str = "Wielding 2 Weapons";
            else
                str = "Wielding 1 Weapon";
        }
        else if (columns[sel_column].skill == job_skill::NONE)
        {
            str = ENUM_ATTR_STR(unit_labor, caption, columns[sel_column].labor);
            if (unit->status.labors[columns[sel_column].labor])
                str += " Enabled";
            else
                str += " Not Enabled";
        }
        else
        {
            df::unit_skill *skill = linear_in_vector<df::unit_skill,df::job_skill>(unit->status.skills, &df::unit_skill::id, columns[sel_column].skill);
            if (skill)
            {
                int level = skill->rating;
                if (level > NUM_SKILL_LEVELS - 1)
                    level = NUM_SKILL_LEVELS - 1;
                str = stl_sprintf("%s %s", skill_levels[level].name, ENUM_ATTR_STR(job_skill, caption_noun, columns[sel_column].skill));
                if (level != NUM_SKILL_LEVELS - 1)
                    str += stl_sprintf(" (%d/%d)", skill->experience, skill_levels[level].points);
            }
            else
                str = stl_sprintf("Not %s (0/500)", ENUM_ATTR_STR(job_skill, caption_noun, columns[sel_column].skill));
        }
        if (columns[sel_column].type == SKILL_ARMOR)
        {
            str += ", wearing ";
            str += armor_names[unit->status.labors[columns[sel_column].labor]];
        }
        if (columns[sel_column].type == SKILL_SHIELD)
        {
            str += ", holding ";
            str += shield_names[unit->status.labors[columns[sel_column].labor]];
        }
        Screen::paintString(Screen::Pen(' ', 9, 0), x, y, str);

        switch (columns[sel_column].type)
        {
        case SKILL_NORMAL:
        case SKILL_SPECIAL:
            canToggleGroup = canToggle = (cur->allowEdit) && columns[sel_column].isValidLabor(cur_entity);
            break;
        default:
            canToggle = (cur->allowEdit);
            break;
        }
    }

    int x = 2, y = dim.y - 3;
    OutputString(10, x, y, Screen::getKeyDisplay(interface_key::SELECT));
    OutputString(canToggle ? 15 : 8, x, y, ": Toggle labor, ");

    OutputString(10, x, y, Screen::getKeyDisplay(interface_key::SELECT_ALL));
    OutputString(canToggleGroup ? 15 : 8, x, y, ": Toggle Group, ");

    OutputString(10, x, y, Screen::getKeyDisplay(interface_key::UNITJOB_VIEW));
    OutputString(15, x, y, ": ViewCre, ");

    OutputString(10, x, y, Screen::getKeyDisplay(interface_key::UNITJOB_ZOOM_CRE));
    OutputString(15, x, y, ": Zoom-Cre");

    x = 2; y = dim.y - 2;
    OutputString(10, x, y, Screen::getKeyDisplay(interface_key::LEAVESCREEN));
    OutputString(15, x, y, ": Done, ");

    OutputString(10, x, y, Screen::getKeyDisplay(interface_key::SECONDSCROLL_DOWN));
    OutputString(10, x, y, Screen::getKeyDisplay(interface_key::SECONDSCROLL_UP));
    OutputString(15, x, y, ": Sort by Skill, ");

    OutputString(10, x, y, Screen::getKeyDisplay(interface_key::SECONDSCROLL_PAGEDOWN));
    OutputString(10, x, y, Screen::getKeyDisplay(interface_key::SECONDSCROLL_PAGEUP));
    OutputString(15, x, y, ": Sort by (");
    OutputString(10, x, y, Screen::getKeyDisplay(interface_key::CHANGETAB));
    OutputString(15, x, y, ") ");
    switch (altsort)
    {
    case ALTSORT_NAME:
        OutputString(15, x, y, "Name");
        break;
    case ALTSORT_PROFESSION:
        OutputString(15, x, y, "Profession");
        break;
    case ALTSORT_HAPPINESS:
        OutputString(15, x, y, "Happiness");
        break;
    case ALTSORT_ARRIVAL:
        OutputString(15, x, y, "Arrival");
        break;
    default:
        OutputString(15, x, y, "Unknown");
        break;
    }

    dfhack_viewscreen::render();
}

df::unit *viewscreen_unitlaborsst::getSelectedUnit()
{
    // This query might be from the rename plugin
    do_refresh_names = true;

    return units[sel_row]->unit;
}

bool inRenderHook;

struct unitjobs_hook : df::viewscreen_unitjobsst
{
    typedef df::viewscreen_unitjobsst interpose_base;

    DEFINE_VMETHOD_INTERPOSE(void, view, ())
    {
        if (Screen::isKeyPressed(interface_key::UNITVIEW_PRF_PROF))
        {
            if (units.size())
            {
                Screen::show(new viewscreen_unitlaborsst(units, cursor_pos));
                return;
            }
        }
        if (units.size())
            inRenderHook = true;
        INTERPOSE_NEXT(view)();
    }
};

DFhackCExport command_result plugin_onrender ( color_ostream &out)
{
    if (inRenderHook)
    {
        auto dim = Screen::getWindowSize();
        int x = 2, y = dim.y - 2;
        OutputString(12, x, y, Screen::getKeyDisplay(interface_key::UNITVIEW_PRF_PROF));
        OutputString(15, x, y, ": Manage labors (DFHack)");
        inRenderHook = false;
    }
    return CR_OK;
}

IMPLEMENT_VMETHOD_INTERPOSE(unitjobs_hook, view);

DFHACK_PLUGIN("manipulator");

DFHACK_PLUGIN_IS_ENABLED(is_enabled);

DFhackCExport command_result plugin_enable(color_ostream &out, bool enable)
{
    if (!gps)
        return CR_FAILURE;

    if (enable != is_enabled)
    {
        if (!INTERPOSE_HOOK(unitjobs_hook, view).apply(enable))
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
    INTERPOSE_HOOK(unitjobs_hook, view).remove();
    return CR_OK;
}
