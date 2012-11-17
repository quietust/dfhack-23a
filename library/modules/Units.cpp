/*
https://github.com/peterix/dfhack
Copyright (c) 2009-2012 Petr Mrázek (peterix@gmail.com)

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/


#include "Internal.h"

#include <stddef.h>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <algorithm>
using namespace std;

#include "VersionInfo.h"
#include "MemAccess.h"
#include "Error.h"
#include "Types.h"

// we connect to those
#include "modules/Units.h"
#include "modules/Items.h"
#include "modules/Materials.h"
#include "modules/Translation.h"
#include "ModuleFactory.h"
#include "Core.h"
#include "MiscUtils.h"

#include "df/world.h"
#include "df/ui.h"
#include "df/job.h"
#include "df/unit_inventory_item.h"
#include "df/nemesis_record.h"
#include "df/historical_entity.h"
#include "df/historical_figure.h"
#include "df/histfig_entity_link.h"
#include "df/creature_raw.h"
#include "df/game_mode.h"
#include "df/unit_misc_trait.h"
#include "df/unit_skill.h"

using namespace DFHack;
using namespace df::enums;
using df::global::world;
using df::global::ui;
using df::global::gamemode;

bool Units::isValid()
{
    return (world->units.all.size() > 0);
}

int32_t Units::getNumCreatures()
{
    return world->units.all.size();
}

df::unit * Units::GetCreature (const int32_t index)
{
    if (!isValid()) return NULL;

    // read pointer from vector at position
    if(size_t(index) > world->units.all.size())
        return 0;
    return world->units.all[index];
}

// returns index of creature actually read or -1 if no creature can be found
int32_t Units::GetCreatureInBox (int32_t index, df::unit ** furball,
                                const uint16_t x1, const uint16_t y1, const uint16_t z1,
                                const uint16_t x2, const uint16_t y2, const uint16_t z2)
{
    if (!isValid())
        return -1;

    size_t size = world->units.all.size();
    while (size_t(index) < size)
    {
        // read pointer from vector at position
        df::unit * temp = world->units.all[index];
        if (temp->pos.x >= x1 && temp->pos.x < x2)
        {
            if (temp->pos.y >= y1 && temp->pos.y < y2)
            {
                if (temp->pos.z >= z1 && temp->pos.z < z2)
                {
                    *furball = temp;
                    return index;
                }
            }
        }
        index++;
    }
    *furball = NULL;
    return -1;
}

void Units::CopyCreature(df::unit * source, t_unit & furball)
{
    if(!isValid()) return;
    // read pointer from vector at position
    furball.origin = source;

    //read creature from memory
    // name
    Translation::readName(furball.name, &source->name);

    // basic stuff
    furball.id = source->id;
    furball.x = source->pos.x;
    furball.y = source->pos.y;
    furball.z = source->pos.z;
    furball.race = source->race;
    furball.civ = source->civ_id;
    furball.sex = source->sex;
    furball.flags1.whole = source->flags1.whole;
    furball.flags2.whole = source->flags2.whole;
    // custom profession
    furball.custom_profession = source->custom_profession;
    // profession
    furball.profession = source->profession;
    // happiness
    furball.happiness = source->status.happiness;
    // physical attributes
    furball.strength = source->status.strength;
    furball.agility = source->status.agility;
    furball.toughness = source->status.toughness;

    // mood stuff
    furball.mood = source->mood;
    furball.mood_skill = source->job.mood_skill; // FIXME: really? More like currently used skill anyway.
    Translation::readName(furball.artifact_name, &source->status.artifact_name);

    // labors
    memcpy(&furball.labors, &source->status.labors, sizeof(furball.labors));

    furball.pregnancy_timer = source->relations.pregnancy_timer;

    //likes.
    /*
    DfVector <uint32_t> likes(d->p, temp + offs.creature_likes_offset);
    furball.numLikes = likes.getSize();
    for(uint32_t i = 0;i<furball.numLikes;i++)
    {
        uint32_t temp2 = *(uint32_t *) likes[i];
        p->read(temp2,sizeof(t_like),(uint8_t *) &furball.likes[i]);
    }
    */
    /*
    if(d->Ft_soul)
    {
        uint32_t soul = p->readDWord(addr_cr + offs.default_soul_offset);
        furball.has_default_soul = false;

        if(soul)
        {
            furball.has_default_soul = true;
            // get first soul's skills
            DfVector <uint32_t> skills(soul + offs.soul_skills_vector_offset);
            furball.defaultSoul.numSkills = skills.size();

            for (uint32_t i = 0; i < furball.defaultSoul.numSkills;i++)
            {
                uint32_t temp2 = skills[i];
                // a byte: this gives us 256 skills maximum.
                furball.defaultSoul.skills[i].id = p->readByte (temp2);
                furball.defaultSoul.skills[i].rating =
                    p->readByte (temp2 + offsetof(t_skill, rating));
                furball.defaultSoul.skills[i].experience =
                    p->readWord (temp2 + offsetof(t_skill, experience));
            }

            // mental attributes are part of the soul
            p->read(soul + offs.soul_mental_offset,
                sizeof(t_attrib) * NUM_CREATURE_MENTAL_ATTRIBUTES,
                (uint8_t *)&furball.defaultSoul.analytical_ability);

            // traits as well
            p->read(soul + offs.soul_traits_offset,
                sizeof (uint16_t) * NUM_CREATURE_TRAITS,
                (uint8_t *) &furball.defaultSoul.traits);
        }
    }
    */
    if(source->job.current_job == NULL)
    {
        furball.current_job.active = false;
    }
    else
    {
        furball.current_job.active = true;
        furball.current_job.jobType = source->job.current_job->job_type;
        furball.current_job.jobId = source->job.current_job->id;
    }
}

int32_t Units::FindIndexById(int32_t creature_id)
{
    return df::unit::binsearch_index(world->units.all, creature_id);
}
/*
bool Creatures::WriteLabors(const uint32_t index, uint8_t labors[NUM_CREATURE_LABORS])
{
    if(!d->Started || !d->Ft_advanced) return false;

    uint32_t temp = d->p_cre->at (index);
    Process * p = d->owner;

    p->write(temp + d->creatures.labors_offset, NUM_CREATURE_LABORS, labors);
    uint32_t pickup_equip;
    p->readDWord(temp + d->creatures.pickup_equipment_bit, pickup_equip);
    pickup_equip |= 1u;
    p->writeDWord(temp + d->creatures.pickup_equipment_bit, pickup_equip);
    return true;
}

bool Creatures::WriteHappiness(const uint32_t index, const uint32_t happinessValue)
{
    if(!d->Started || !d->Ft_advanced) return false;

    uint32_t temp = d->p_cre->at (index);
    Process * p = d->owner;
    p->writeDWord (temp + d->creatures.happiness_offset, happinessValue);
    return true;
}

bool Creatures::WriteFlags(const uint32_t index,
                           const uint32_t flags1,
                           const uint32_t flags2)
{
    if(!d->Started || !d->Ft_basic) return false;

    uint32_t temp = d->p_cre->at (index);
    Process * p = d->owner;
    p->writeDWord (temp + d->creatures.flags1_offset, flags1);
    p->writeDWord (temp + d->creatures.flags2_offset, flags2);
    return true;
}

bool Creatures::WriteFlags(const uint32_t index,
                           const uint32_t flags1,
                           const uint32_t flags2,
                           const uint32_t flags3)
{
    if(!d->Started || !d->Ft_basic) return false;

    uint32_t temp = d->p_cre->at (index);
    Process * p = d->owner;
    p->writeDWord (temp + d->creatures.flags1_offset, flags1);
    p->writeDWord (temp + d->creatures.flags2_offset, flags2);
    p->writeDWord (temp + d->creatures.flags3_offset, flags3);
    return true;
}

bool Creatures::WriteSkills(const uint32_t index, const t_soul &soul)
{
    if(!d->Started || !d->Ft_soul) return false;

    uint32_t temp = d->p_cre->at (index);
    Process * p = d->owner;
    uint32_t souloff = p->readDWord(temp + d->creatures.default_soul_offset);

    if(!souloff)
    {
        return false;
    }

    DfVector<uint32_t> skills(souloff + d->creatures.soul_skills_vector_offset);

    for (uint32_t i=0; i<soul.numSkills; i++)
    {
        uint32_t temp2 = skills[i];
        p->writeByte(temp2 + offsetof(t_skill, rating), soul.skills[i].rating);
        p->writeWord(temp2 + offsetof(t_skill, experience), soul.skills[i].experience);
    }

    return true;
}

bool Creatures::WriteAttributes(const uint32_t index, const t_creature &creature)
{
    if(!d->Started || !d->Ft_advanced || !d->Ft_soul) return false;

    uint32_t temp = d->p_cre->at (index);
    Process * p = d->owner;
    uint32_t souloff = p->readDWord(temp + d->creatures.default_soul_offset);

    if(!souloff)
    {
        return false;
    }

    // physical attributes
    p->write(temp + d->creatures.physical_offset,
        sizeof(t_attrib) * NUM_CREATURE_PHYSICAL_ATTRIBUTES,
        (uint8_t *)&creature.strength);

    // mental attributes are part of the soul
    p->write(souloff + d->creatures.soul_mental_offset,
        sizeof(t_attrib) * NUM_CREATURE_MENTAL_ATTRIBUTES,
        (uint8_t *)&creature.defaultSoul.analytical_ability);

    return true;
}

bool Creatures::WriteSex(const uint32_t index, const uint8_t sex)
{
    if(!d->Started || !d->Ft_basic ) return false;

    uint32_t temp = d->p_cre->at (index);
    Process * p = d->owner;
    p->writeByte (temp + d->creatures.sex_offset, sex);

    return true;
}

bool Creatures::WriteTraits(const uint32_t index, const t_soul &soul)
{
    if(!d->Started || !d->Ft_soul) return false;

    uint32_t temp = d->p_cre->at (index);
    Process * p = d->owner;
    uint32_t souloff = p->readDWord(temp + d->creatures.default_soul_offset);

    if(!souloff)
    {
        return false;
    }

    p->write(souloff + d->creatures.soul_traits_offset,
            sizeof (uint16_t) * NUM_CREATURE_TRAITS,
            (uint8_t *) &soul.traits);

    return true;
}

bool Creatures::WriteMood(const uint32_t index, const uint16_t mood)
{
    if(!d->Started || !d->Ft_advanced) return false;

    uint32_t temp = d->p_cre->at (index);
    Process * p = d->owner;
    p->writeWord(temp + d->creatures.mood_offset, mood);
    return true;
}

bool Creatures::WriteMoodSkill(const uint32_t index, const uint16_t moodSkill)
{
    if(!d->Started || !d->Ft_advanced) return false;

    uint32_t temp = d->p_cre->at (index);
    Process * p = d->owner;
    p->writeWord(temp + d->creatures.mood_skill_offset, moodSkill);
    return true;
}

bool Creatures::WriteJob(const t_creature * furball, std::vector<t_material> const& mat)
{
    if(!d->Inited || !d->Ft_job_materials) return false;
    if(!furball->current_job.active) return false;

    unsigned int i;
    Process * p = d->owner;
    Private::t_offsets & off = d->creatures;
    DfVector <uint32_t> cmats(furball->current_job.occupationPtr + off.job_materials_vector);

    for(i=0;i<cmats.size();i++)
    {
        p->writeWord(cmats[i] + off.job_material_itemtype_o, mat[i].itemType);
        p->writeWord(cmats[i] + off.job_material_subtype_o, mat[i].itemSubtype);
        p->writeWord(cmats[i] + off.job_material_subindex_o, mat[i].subIndex);
        p->writeDWord(cmats[i] + off.job_material_index_o, mat[i].index);
        p->writeDWord(cmats[i] + off.job_material_flags_o, mat[i].flags);
    }
    return true;
}

bool Creatures::WritePos(const uint32_t index, const t_creature &creature)
{
    if(!d->Started) return false;

    uint32_t temp = d->p_cre->at (index);
    Process * p = d->owner;
    p->write (temp + d->creatures.pos_offset, 3 * sizeof (uint16_t), (uint8_t *) & (creature.x));
    return true;
}

bool Creatures::WriteCiv(const uint32_t index, const int32_t civ)
{
    if(!d->Started) return false;

    uint32_t temp = d->p_cre->at (index);
    Process * p = d->owner;
    p->writeDWord(temp + d->creatures.civ_offset, civ);
    return true;
}

bool Creatures::WritePregnancy(const uint32_t index, const uint32_t pregTimer)
{
    if(!d->Started) return false;

    uint32_t temp = d->p_cre->at (index);
    Process * p = d->owner;
    p->writeDWord(temp + d->creatures.pregnancy_offset, pregTimer);
    return true;
}
*/
uint32_t Units::GetDwarfRaceIndex()
{
    return ui->race_id;
}

int32_t Units::GetDwarfCivId()
{
    return ui->civ_id;
}
/*
bool Creatures::getCurrentCursorCreature(uint32_t & creature_index)
{
    if(!d->cursorWindowInited) return false;
    Process * p = d->owner;
    creature_index = p->readDWord(d->current_cursor_creature_offset);
    return true;
}
*/
/*
bool Creatures::ReadJob(const t_creature * furball, vector<t_material> & mat)
{
    unsigned int i;
    if(!d->Inited || !d->Ft_job_materials) return false;
    if(!furball->current_job.active) return false;

    Process * p = d->owner;
    Private::t_offsets & off = d->creatures;
    DfVector <uint32_t> cmats(furball->current_job.occupationPtr + off.job_materials_vector);
    mat.resize(cmats.size());
    for(i=0;i<cmats.size();i++)
    {
        mat[i].itemType = p->readWord(cmats[i] + off.job_material_itemtype_o);
        mat[i].itemSubtype = p->readWord(cmats[i] + off.job_material_subtype_o);
        mat[i].subIndex = p->readWord(cmats[i] + off.job_material_subindex_o);
        mat[i].index = p->readDWord(cmats[i] + off.job_material_index_o);
        mat[i].flags = p->readDWord(cmats[i] + off.job_material_flags_o);
    }
    return true;
}
*/
bool Units::ReadInventoryByIdx(const uint32_t index, std::vector<df::item *> & item)
{
    if(index >= world->units.all.size()) return false;
    df::unit * temp = world->units.all[index];
    return ReadInventoryByPtr(temp, item);
}

bool Units::ReadInventoryByPtr(const df::unit * unit, std::vector<df::item *> & items)
{
    if(!isValid()) return false;
    if(!unit) return false;
    items.clear();
    for (size_t i = 0; i < unit->inventory.size(); i++)
        items.push_back(unit->inventory[i]->item);
    return true;
}

void Units::CopyNameTo(df::unit * creature, df::language_name * target)
{
    Translation::copyName(&creature->name, target);
}

df::coord Units::getPosition(df::unit *unit)
{
    CHECK_NULL_POINTER(unit);

    if (unit->flags1.bits.caged)
    {
        auto cage = getContainer(unit);
        if (cage)
            return Items::getPosition(cage);
    }

    return unit->pos;
}

df::general_ref *Units::getGeneralRef(df::unit *unit, df::general_ref_type type)
{
    CHECK_NULL_POINTER(unit);

    return findRef(unit->general_refs, type);
}

df::specific_ref *Units::getSpecificRef(df::unit *unit, df::specific_ref_type type)
{
    CHECK_NULL_POINTER(unit);

    return findRef(unit->specific_refs, type);
}

df::item *Units::getContainer(df::unit *unit)
{
    CHECK_NULL_POINTER(unit);

    return findItemRef(unit->general_refs, general_ref_type::CONTAINED_IN_ITEM);
}

void Units::setNickname(df::unit *unit, std::string nick)
{
    CHECK_NULL_POINTER(unit);

    // There are multiple copies of the name, and the one
    // in the unit is not the authoritative one.
    // This is the reason why military units often
    // lose nicknames set from Dwarf Therapist.
    Translation::setNickname(&unit->name, nick);

    df::historical_figure *figure = df::historical_figure::find(unit->hist_figure_id);
    if (figure)
        Translation::setNickname(&figure->name, nick);
}

df::language_name *Units::getVisibleName(df::unit *unit)
{
    CHECK_NULL_POINTER(unit);

    return &unit->name;
}

df::nemesis_record *Units::getNemesis(df::unit *unit)
{
    if (!unit)
        return NULL;

    for (unsigned i = 0; i < unit->general_refs.size(); i++)
    {
        df::nemesis_record *rv = unit->general_refs[i]->getNemesis();
        if (rv && rv->unit == unit)
            return rv;
    }

    return NULL;
}

static bool creatureFlagSet(int race, df::creature_raw_flags flag)
{
    auto creature = df::creature_raw::find(race);
    if (!creature)
        return false;

    return creature->flags.is_set(flag);
}

bool Units::hasExtravision(df::unit *unit)
{
    CHECK_NULL_POINTER(unit);
    return creatureFlagSet(unit->race, creature_raw_flags::EXTRAVISION);
}

bool Units::isMischievous(df::unit *unit)
{
    CHECK_NULL_POINTER(unit);
    return creatureFlagSet(unit->race, creature_raw_flags::MISCHIEVIOUS);
}

df::unit_misc_trait *Units::getMiscTrait(df::unit *unit, df::misc_trait_type type, bool create)
{
    CHECK_NULL_POINTER(unit);

    auto &vec = unit->status.misc_traits;
    for (size_t i = 0; i < vec.size(); i++)
        if (vec[i]->id == type)
            return vec[i];

    if (create)
    {
        auto obj = new df::unit_misc_trait();
        obj->id = type;
        vec.push_back(obj);
        return obj;
    }

    return NULL;
}

bool DFHack::Units::isDead(df::unit *unit)
{
    CHECK_NULL_POINTER(unit);

    return unit->flags1.bits.dead;
}

bool DFHack::Units::isAlive(df::unit *unit)
{
    CHECK_NULL_POINTER(unit);

    return !unit->flags1.bits.dead;
}

bool DFHack::Units::isSane(df::unit *unit)
{
    CHECK_NULL_POINTER(unit);

    if (unit->flags1.bits.dead)
        return false;

    switch (unit->mood)
    {
    case mood_type::Melancholy:
    case mood_type::Raving:
    case mood_type::Berserk:
        return false;
    default:
        break;
    }

    return true;
}

bool DFHack::Units::isCitizen(df::unit *unit)
{
    CHECK_NULL_POINTER(unit);

    /*
     * This needs to be completely rechecked - it's based on how speed worked in 0.34.11
     */

    // Copied from the conditions used to decide game over,
    // except that the game appears to let melancholy/raving
    // dwarves count as citizens.

    if (!isDwarf(unit) || !isSane(unit))
        return false;

    if (unit->flags1.bits.marauder ||
        unit->flags1.bits.invader_origin ||
        unit->flags1.bits.active_invader ||
        unit->flags1.bits.forest ||
        unit->flags1.bits.merchant ||
        unit->flags1.bits.diplomat)
        return false;

    if (unit->flags1.bits.tame)
        return true;

    return unit->civ_id == ui->civ_id &&
           unit->civ_id != -1 &&
           !unit->flags2.bits.underworld &&
           !unit->flags2.bits.resident &&
           !unit->flags2.bits.visitor_uninvited &&
           !unit->flags2.bits.visitor;
}

bool DFHack::Units::isDwarf(df::unit *unit)
{
    CHECK_NULL_POINTER(unit);

    return unit->race == ui->race_id;
}

double DFHack::Units::getAge(df::unit *unit, bool true_age)
{
    CHECK_NULL_POINTER(unit);
    return unit->relations.age_years * 403200.0 + unit->relations.age_seconds;
}

inline void adjust_skill_rating(int &rating, bool is_adventure, int value, int dwarf3_4, int dwarf1_2, int adv9_10, int adv3_4, int adv1_2)
{
    /*
     * This needs to be completely rechecked - it's based on how speed worked in 0.34.11
     */
    if  (is_adventure)
    {
        if (value >= adv1_2) rating >>= 1;
        else if (value >= adv3_4) rating = rating*3/4;
        else if (value >= adv9_10) rating = rating*9/10;
    }
    else
    {
        if (value >= dwarf1_2) rating >>= 1;
        else if (value >= dwarf3_4) rating = rating*3/4;
    }
}

int Units::getNominalSkill(df::unit *unit, df::job_skill skill_id, bool use_rust)
{
    CHECK_NULL_POINTER(unit);

    /*
     * This needs to be completely rechecked - it's based on how speed worked in 0.34.11
     */

    // Retrieve skill from unit soul:

    auto skill = binsearch_in_vector(unit->status.skills, &df::unit_skill::id, skill_id);

    if (skill)
    {
        int rating = int(skill->rating);
        return std::max(0, rating);
    }

    return 0;
}

int Units::getExperience(df::unit *unit, df::job_skill skill_id, bool total)
{
    CHECK_NULL_POINTER(unit);

    auto skill = binsearch_in_vector(unit->status.skills, &df::unit_skill::id, skill_id);
    if (!skill)
        return 0;

    int xp = skill->experience;
    // exact formula used by the game:
    if (total && skill->rating > 0)
        xp += 500*skill->rating + 100*skill->rating*(skill->rating - 1)/2;
    return xp;
}

int Units::getEffectiveSkill(df::unit *unit, df::job_skill skill_id)
{
    /*
     * This needs to be completely rechecked - it's based on how speed worked in 0.34.11
     */

    int rating = getNominalSkill(unit, skill_id, true);

    // Apply special states

    if (unit->counters.soldier_mood == df::unit::T_counters::None)
    {
        if (unit->counters.nausea > 0) rating >>= 1;
        if (unit->counters.winded > 0) rating >>= 1;
        if (unit->counters.stunned > 0) rating >>= 1;
    }

    if (unit->counters.soldier_mood != df::unit::T_counters::MartialTrance)
    {
        if (!unit->flags2.bits.vision_good && !unit->flags2.bits.vision_damaged &&
            !hasExtravision(unit))
        {
            rating >>= 2;
        }
        if (unit->counters.pain >= 100 && unit->mood == -1)
        {
            rating >>= 1;
        }
        if (unit->counters.exhaustion >= 2000)
        {
            rating = rating*3/4;
            if (unit->counters.exhaustion >= 4000)
            {
                rating = rating*3/4;
                if (unit->counters.exhaustion >= 6000)
                    rating = rating*3/4;
            }
        }
    }

    // Hunger etc timers

    bool is_adventure = (gamemode && *gamemode == game_mode::ADVENTURE);

    adjust_skill_rating(
        rating, is_adventure, unit->counters.thirst_timer,
        50000, 50000, 115200, 172800, 345600
    );
    adjust_skill_rating(
        rating, is_adventure, unit->counters.hunger_timer,
        75000, 75000, 172800, 1209600, 2592000
    );
    if (is_adventure && unit->counters.sleepiness_timer >= 846000)
        rating >>= 2;
    else
        adjust_skill_rating(
            rating, is_adventure, unit->counters.sleepiness_timer,
            150000, 150000, 172800, 259200, 345600
        );

    return rating;
}

inline void adjust_speed_rating(int &rating, bool is_adventure, int value, int dwarf100, int dwarf200, int adv50, int adv75, int adv100, int adv200)
{
    /*
     * This needs to be completely rechecked - it's based on how speed worked in 0.34.11
     */

    if  (is_adventure)
    {
        if (value >= adv200) rating += 200;
        else if (value >= adv100) rating += 100;
        else if (value >= adv75) rating += 75;
        else if (value >= adv50) rating += 50;
    }
    else
    {
        if (value >= dwarf200) rating += 200;
        else if (value >= dwarf100) rating += 100;
    }
}

static int calcInventoryWeight(df::unit *unit)
{
    /*
     * This needs to be completely rechecked - it's based on how speed worked in 0.34.11
     */

    int armor_skill = Units::getEffectiveSkill(unit, job_skill::ARMOR);
    int armor_mul = 15 - std::min(15, armor_skill);

    int inv_weight = 0, inv_weight_fraction = 0;

    for (size_t i = 0; i < unit->inventory.size(); i++)
    {
        auto item = unit->inventory[i]->item;

        int wval = (item->getBaseWeight() >> item->getWeightShiftBits()) * item->getDensity() / 1000;
        auto mode = unit->inventory[i]->mode;

        if ((mode == df::unit_inventory_item::Worn) &&
             item->isArmorNotClothing() && armor_skill > 1)
        {
            wval = wval * armor_mul / 16;
        }

        inv_weight += wval;
    }

    return inv_weight;
}

int Units::computeMovementSpeed(df::unit *unit)
{
    /*
     * This needs to be completely rechecked - it's based on how speed worked in 0.34.11
     */

    // Base speed

    auto craw = df::creature_raw::find(unit->race);
    if (!craw)
        return 0;

    int speed = craw->speed;

    // Swimming

    auto cur_liquid = unit->status2.liquid_type.bits.liquid_type;
    bool in_magma = (cur_liquid == tile_liquid::Magma);

    if (unit->flags2.bits.swimming)
    {
        speed = craw->swim_speed;
        if (in_magma)
            speed *= 2;

        if (craw->flags.is_set(creature_raw_flags::SWIMS_LEARNED))
        {
            int skill = Units::getEffectiveSkill(unit, job_skill::SWIMMING);

            // Originally a switch:
            if (skill > 1)
                speed = speed * std::max(6, 21-skill) / 20;
        }
    }
    else
    {
        int delta = 150*unit->status2.liquid_depth;
        if (in_magma)
            delta *= 2;
        speed += delta;
    }

    // General counters and flags

    if (unit->profession == profession::BABY)
        speed += 3000;

    if (unit->counters.exhaustion >= 2000)
    {
        speed += 200;
        if (unit->counters.exhaustion >= 4000)
        {
            speed += 200;
            if (unit->counters.exhaustion >= 6000)
                speed += 200;
        }
    }

    if (unit->flags2.bits.gutted) speed += 2000;

    if (unit->counters.soldier_mood == df::unit::T_counters::None)
    {
        if (unit->counters.nausea > 0) speed += 1000;
        if (unit->counters.winded > 0) speed += 1000;
        if (unit->counters.stunned > 0) speed += 1000;
    }

    if (unit->counters.soldier_mood != df::unit::T_counters::MartialTrance)
    {
        if (unit->counters.pain >= 100 && unit->mood == -1)
            speed += 1000;
    }

    // Hunger etc timers

    bool is_adventure = (gamemode && *gamemode == game_mode::ADVENTURE);

    adjust_speed_rating(
        speed, is_adventure, unit->counters.thirst_timer,
        50000, 0x7fffffff, 172800, 172800, 172800, 345600
    );
    adjust_speed_rating(
        speed, is_adventure, unit->counters.hunger_timer,
        75000, 0x7fffffff, 1209600, 1209600, 1209600, 2592000
    );
    adjust_speed_rating(
        speed, is_adventure, unit->counters.sleepiness_timer,
        57600, 150000, 172800, 259200, 345600, 864000
    );

    // Activity state

    if (unit->relations.draggee_id != -1) speed += 1000;

    if (unit->flags1.bits.on_ground)
        speed += 2000;

    if (unit->flags1.bits.hidden_in_ambush && !Units::isMischievous(unit))
    {
        int skill = Units::getEffectiveSkill(unit, job_skill::SNEAK);
        speed += 2000 - 100*std::min(20, skill);
    }

    if (unsigned(unit->counters.paralysis-1) <= 98)
        speed += unit->counters.paralysis*10;
    if (unsigned(unit->counters.webbed-1) <= 8)
        speed += unit->counters.webbed*100;

    // Attributes

    int strength_attr = unit->status.strength;
    int agility_attr = unit->status.agility;

    int total_attr = std::max(200, std::min(3800, strength_attr + agility_attr));
    speed = ((total_attr-200)*(speed/2) + (3800-total_attr)*(speed*3/2))/3600;

    // Stance

    if (!unit->flags1.bits.on_ground && unit->status2.able_stand > 2)
    {
        // WTF
        int as = unit->status2.able_stand;
        int x = (as-1) - (as>>1);
        int y = as - unit->status2.able_stand_impair;
        y = y * 500 / x;
        if (y > 0) speed += y;
    }

    // Mood

    if (unit->mood == mood_type::Melancholy) speed += 8000;

    // Inventory encumberance

    int total_weight = calcInventoryWeight(unit);
    int free_weight = std::max(1, strength_attr*3);

    if (free_weight < total_weight)
    {
        int delta = (total_weight - free_weight)/10 + 1;
        if (!is_adventure)
            delta = std::min(5000, delta);
        speed += delta;
    }

    // skipped: unknown loop on inventory items that amounts to 0 change

    if (is_adventure)
    {
        auto player = vector_get(world->units.active, 0);
        if (player && player->id == unit->relations.group_leader_id)
            speed = std::min(speed, computeMovementSpeed(player));
    }

    return std::min(10000, std::max(0, speed));
}

static bool noble_pos_compare(const Units::NoblePosition &a, const Units::NoblePosition &b)
{
    return (a.precedence > b.precedence);
}

bool DFHack::Units::getNoblePositions(std::vector<NoblePosition> *pvec, df::unit *unit)
{
    CHECK_NULL_POINTER(unit);

    pvec->clear();

    auto histfig = df::historical_figure::find(unit->hist_figure_id);
    if (!histfig)
        return false;

    for (size_t i = 0; i < histfig->entity_links.size(); i++)
    {
        auto link = histfig->entity_links[i];
        NoblePosition pos;
        pos.position = link->type;
        switch (link->type)
        {
        case histfig_entity_link_type::MAYOR:
            pos.precedence = 2;
            pos.color[0] = 5;
            pos.color[1] = 0;
            pos.color[2] = 0;
            pos.name = "Mayor";
            break;
        case histfig_entity_link_type::GUARD:
            pos.precedence = 0;
            pos.color[0] = 1;
            pos.color[1] = 0;
            pos.color[2] = 0;
            pos.name = "Guard";
            break;
        case histfig_entity_link_type::ROYAL_GUARD:
            pos.precedence = 0;
            pos.color[0] = 5;
            pos.color[1] = 0;
            pos.color[2] = 0;
            pos.name = "Royal Guard";
            break;
/*
        case histfig_entity_link_type::MANAGER:
            pos.precedence = -1;
            break;
        case histfig_entity_link_type::BOOKKEEPER:
            pos.precedence = -1;
            break;
        case histfig_entity_link_type::BROKER:
            pos.precedence = -1;
            break;
*/
        case histfig_entity_link_type::SHERIFF:
            pos.precedence = 1;
            pos.color[0] = 1;
            pos.color[1] = 0;
            pos.color[2] = 1;
            pos.name = "Sheriff";
            break;
        case histfig_entity_link_type::GUARD_CAPTAIN:
            pos.precedence = 1;
            pos.color[0] = 1;
            pos.color[1] = 0;
            pos.color[2] = 1;
            pos.name = "Captain of the Guard";
            break;
        default:
            pos.precedence = -1;
        }
        if (pos.precedence == -1)
            continue;

        pvec->push_back(pos);
    }

    if (pvec->empty())
        return false;

    std::sort(pvec->begin(), pvec->end(), noble_pos_compare);
    return true;
}

std::string DFHack::Units::getProfessionName(df::unit *unit, bool ignore_noble, bool plural)
{
    std::string prof = unit->custom_profession;
    if (!prof.empty())
        return prof;

    std::vector<NoblePosition> np;

    if (!ignore_noble && getNoblePositions(&np, unit))
    {
        prof = np[0].name;
        if (!prof.empty())
            return prof;
    }

    return getCreatureProfessionName(unit->race, unit->profession, plural);
}

std::string DFHack::Units::getCreatureProfessionName(int race, df::profession pid, bool plural)
{
    std::string prof, race_prefix;

    if (pid < (df::profession)0 || !is_valid_enum_item(pid))
        return "";

    bool use_race_prefix = (race >= 0 && race != df::global::ui->race_id);

    if (auto creature = df::creature_raw::find(race))
    {
        race_prefix = creature->name[0];

        switch (pid)
        {
        case profession::CRAFTSMAN:
            prof = creature->craftsman_name[plural ? 1 : 0];
            break;
        case profession::FISHERMAN:
            prof = creature->fisherman_name[plural ? 1 : 0];
            break;
        case profession::HAMMERMAN:
            prof = creature->hammerman_name[plural ? 1 : 0];
            break;
        case profession::SPEARMAN:
            prof = creature->spearman_name[plural ? 1 : 0];
            break;
        case profession::CROSSBOWMAN:
            prof = creature->crossbowman_name[plural ? 1 : 0];
            break;
        case profession::AXEMAN:
            prof = creature->axeman_name[plural ? 1 : 0];
            break;
        case profession::SWORDSMAN:
            prof = creature->swordsman_name[plural ? 1 : 0];
            break;
        case profession::MACEMAN:
            prof = creature->maceman_name[plural ? 1 : 0];
            break;
        case profession::PIKEMAN:
            prof = creature->pikeman_name[plural ? 1 : 0];
            break;
        case profession::BOWMAN:
            prof = creature->bowman_name[plural ? 1 : 0];
            break;
        case profession::CHILD:
            prof = creature->childname[plural ? 1 : 0];
            break;
        case profession::BABY:
            prof = creature->babyname[plural ? 1 : 0];
            break;
        }
        if (!prof.empty())
            use_race_prefix = false;

        if (race_prefix.empty())
            race_prefix = creature->name[0];
    }

    if (race_prefix.empty())
        race_prefix = "Animal";

    if (prof.empty())
    {
        switch (pid)
        {
        case profession::TRAINED_WAR:
            prof = "War " + (use_race_prefix ? race_prefix : "Peasant");
            use_race_prefix = false;
            break;

        case profession::TRAINED_HUNTER:
            prof = "Hunting " + (use_race_prefix ? race_prefix : "Peasant");
            use_race_prefix = false;
            break;

        case profession::STANDARD:
            if (!use_race_prefix)
                prof = "Peasant";
            break;

        default:
            if (auto caption = ENUM_ATTR(profession, caption, pid))
                prof = caption;
            else
                prof = ENUM_KEY_STR(profession, pid);
        }
    }

    if (use_race_prefix)
    {
        if (!prof.empty())
            race_prefix += " ";
        prof = race_prefix + prof;
    }

    return Translation::capitalize(prof, true);
}

int8_t DFHack::Units::getProfessionColor(df::unit *unit, bool ignore_noble)
{
    std::vector<NoblePosition> np;

    if (!ignore_noble && getNoblePositions(&np, unit))
        return np[0].color[0] + np[0].color[2] * 8;

    return getCreatureProfessionColor(unit->race, unit->profession);
}

int8_t DFHack::Units::getCreatureProfessionColor(int race, df::profession pid)
{
    // make sure it's an actual profession
    if (pid < 0 || !is_valid_enum_item(pid))
        return 3;

    // If it's not a Peasant, it's hardcoded
    if (pid != profession::STANDARD)
        return ENUM_ATTR(profession, color, pid);

    if (auto creature = df::creature_raw::find(race))
        return creature->color[0] + creature->color[2] * 8;

    // default to dwarven peasant color
    return 3;
}
