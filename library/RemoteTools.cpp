/*
https://github.com/peterix/dfhack
Copyright (c) 2011 Petr Mrázek <peterix@gmail.com>

A thread-safe logging console with a line editor for windows.

Based on linenoise win32 port,
copyright 2010, Jon Griffiths <jon_p_griffiths at yahoo dot com>.
All rights reserved.
Based on linenoise, copyright 2010, Salvatore Sanfilippo <antirez at gmail dot com>.
The original linenoise can be found at: http://github.com/antirez/linenoise

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
  * Neither the name of Redis nor the names of its contributors may be used
    to endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/


#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <istream>
#include <string>
#include <stdint.h>

#include "RemoteTools.h"
#include "PluginManager.h"
#include "MiscUtils.h"
#include "VersionInfo.h"

#include "modules/Materials.h"
#include "modules/Translation.h"
#include "modules/Units.h"
#include "modules/World.h"

#include "DataDefs.h"
#include "df/ui.h"
#include "df/ui_advmode.h"
#include "df/world.h"
#include "df/world_data.h"
#include "df/unit.h"
#include "df/unit_misc_trait.h"
#include "df/unit_skill.h"
#include "df/creature_raw.h"
#include "df/matgloss_stone.h"
#include "df/matgloss_gem.h"
#include "df/matgloss_plant.h"
#include "df/matgloss_wood.h"
#include "df/nemesis_record.h"
#include "df/historical_figure.h"
#include "df/historical_entity.h"

#include "BasicApi.pb.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

#include <memory>

using namespace DFHack;
using namespace df::enums;
using namespace dfproto;

using google::protobuf::MessageLite;

void DFHack::strVectorToRepeatedField(RepeatedPtrField<std::string> *pf,
                                      const std::vector<std::string> &vec)
{
    for (size_t i = 0; i < vec.size(); ++i)
        *pf->Add() = vec[i];
}

void DFHack::describeEnum(RepeatedPtrField<EnumItemName> *pf, int base,
                          int size, const char* const *names)
{
    for (int i = 0; i < size; i++)
    {
        const char *key = names[i];
        if (!key)
            continue;

        auto item = pf->Add();
        item->set_value(base+i);
        item->set_name(key);
    }
}

void DFHack::describeBitfield(RepeatedPtrField<EnumItemName> *pf,
                              int size, const bitfield_item_info *items)
{
    for (int i = 0; i < size; i++)
    {
        const char *key = items[i].name;
        if (!key && items[i].size <= 1)
            continue;

        auto item = pf->Add();

        item->set_value(i);
        if (key)
            item->set_name(key);

        if (items[i].size > 1)
        {
            item->set_bit_size(items[i].size);
            i += items[i].size-1;
        }
    }
}

void DFHack::describeName(NameInfo *info, df::language_name *name)
{
    if (!name->first_name.empty())
        info->set_first_name(DF2UTF(name->first_name));
    if (!name->nickname.empty())
        info->set_nickname(DF2UTF(name->nickname));

    if (name->language >= 0)
        info->set_language_id(name->language);

    std::string lname = Translation::TranslateName(name, false, true);
    if (!lname.empty())
        info->set_last_name(DF2UTF(lname));

    lname = Translation::TranslateName(name, true, true);
    if (!lname.empty())
        info->set_english_name(DF2UTF(lname));
}

void DFHack::describeNameTriple(NameTriple *info, const std::string &name,
                                const std::string &plural, const std::string &adj)
{
    info->set_normal(DF2UTF(name));
    if (!plural.empty() && plural != name)
        info->set_plural(DF2UTF(plural));
    if (!adj.empty() && adj != name)
        info->set_adjective(DF2UTF(adj));
}

void DFHack::describeUnit(BasicUnitInfo *info, df::unit *unit,
                          const BasicUnitInfoMask *mask)
{
    info->set_unit_id(unit->id);

    info->set_pos_x(unit->pos.x);
    info->set_pos_y(unit->pos.y);
    info->set_pos_z(unit->pos.z);

    auto name = Units::getVisibleName(unit);
    if (name->has_name)
        describeName(info->mutable_name(), name);

    info->set_flags1(unit->flags1.whole);
    info->set_flags2(unit->flags2.whole);

    info->set_race(unit->race);

    if (unit->sex >= 0)
        info->set_gender(unit->sex);
    if (unit->civ_id >= 0)
        info->set_civ_id(unit->civ_id);
    if (unit->hist_figure_id >= 0)
        info->set_histfig_id(unit->hist_figure_id);

    if (mask && mask->profession())
    {
        if (unit->profession >= (df::profession)0)
            info->set_profession(unit->profession);
        if (!unit->custom_profession.empty())
            info->set_custom_profession(unit->custom_profession);
    }

    if (mask && mask->labors())
    {
        for (size_t i = 0; i < sizeof(unit->status.labors)/sizeof(bool); i++)
            if (unit->status.labors[i])
                info->add_labors(i);
    }

    if (mask && mask->skills())
    {
        auto &vec = unit->status.skills;

        for (size_t i = 0; i < vec.size(); i++)
        {
            auto skill = vec[i];
            auto item = info->add_skills();
            item->set_id(skill->id);
            item->set_level(skill->rating);
            item->set_experience(skill->experience);
        }
    }

    if (mask && mask->misc_traits())
    {
        auto &vec = unit->status.misc_traits;

        for (size_t i = 0; i < vec.size(); i++)
        {
            auto trait = vec[i];
            auto item = info->add_misc_traits();
            item->set_id(trait->id);
            item->set_value(trait->value);
        }
    }
}

static command_result GetVersion(color_ostream &stream,
                                 const EmptyMessage *, StringMessage *out)
{
    out->set_value(DFHACK_VERSION);
    return CR_OK;
}

static command_result GetDFVersion(color_ostream &stream,
                                   const EmptyMessage *, StringMessage *out)
{
    out->set_value(Core::getInstance().vinfo->getVersion());
    return CR_OK;
}

static command_result GetWorldInfo(color_ostream &stream,
                                   const EmptyMessage *, GetWorldInfoOut *out)
{
    using df::global::ui;
    using df::global::ui_advmode;
    using df::global::world;

    if (!ui || !world || !Core::getInstance().isWorldLoaded())
        return CR_NOT_FOUND;

    df::game_type gt = game_type::DWARF_MAIN;
    if (df::global::gametype)
        gt = *df::global::gametype;

    //out->set_save_dir(world->cur_savegame.save_dir);

    if (world->world_data.name.has_name)
        describeName(out->mutable_world_name(), &world->world_data.name);

    switch (gt)
    {
    case game_type::DWARF_MAIN:
    case game_type::DWARF_RECLAIM:
        out->set_mode(GetWorldInfoOut::MODE_DWARF);
        out->set_civ_id(ui->civ_id);
        out->set_group_id(ui->group_id);
        out->set_race_id(ui->race_id);
        break;

    case game_type::ADVENTURE_MAIN:
        out->set_mode(GetWorldInfoOut::MODE_ADVENTURE);

        if (auto unit = vector_get(world->units.active, 0))
            out->set_player_unit_id(unit->id);

        if (!ui_advmode)
            break;
/*
        if (auto nemesis = vector_get(world->nemesis.all, ui_advmode->player_id))
        {
            if (nemesis->figure)
                out->set_player_histfig_id(nemesis->figure->id);

            for (size_t i = 0; i < nemesis->companions.size(); i++)
            {
                auto unm = df::nemesis_record::find(nemesis->companions[i]);
                if (!unm || !unm->figure)
                    continue;
                out->add_companion_histfig_ids(unm->figure->id);
            }
        }*/
        break;

    case game_type::VIEW_LEGENDS:
        out->set_mode(GetWorldInfoOut::MODE_LEGENDS);
        break;

    default:
        return CR_NOT_FOUND;
    }

    return CR_OK;
}

static command_result ListEnums(color_ostream &stream,
                                const EmptyMessage *, ListEnumsOut *out)
{
#define ENUM(name) describe_enum<df::name>(out->mutable_##name());
#define BITFIELD(name) describe_bitfield<df::name>(out->mutable_##name());

    BITFIELD(unit_flags1);
    BITFIELD(unit_flags2);

    ENUM(unit_labor);
    ENUM(job_skill);

    ENUM(profession);

    return CR_OK;
#undef ENUM
#undef BITFIELD
}

static command_result ListJobSkills(color_ostream &stream, const EmptyMessage *, ListJobSkillsOut *out)
{
    auto pf_skill = out->mutable_skill();
    FOR_ENUM_ITEMS(job_skill, skill)
    {
        auto item = pf_skill->Add();

        item->set_id(skill);
        item->set_key(ENUM_KEY_STR(job_skill, skill));
        item->set_caption(ENUM_ATTR_STR(job_skill, caption, skill));
        item->set_caption_noun(ENUM_ATTR_STR(job_skill, caption_noun, skill));
        item->set_profession(ENUM_ATTR(job_skill, profession, skill));
        item->set_labor(ENUM_ATTR(job_skill, labor, skill));
        item->set_type(ENUM_KEY_STR(job_skill_class, ENUM_ATTR(job_skill, type, skill)));
    }

    auto pf_profession = out->mutable_profession();
    FOR_ENUM_ITEMS(profession, p)
    {
        auto item = pf_profession->Add();

        item->set_id(p);
        item->set_key(ENUM_KEY_STR(profession, p));
        item->set_caption(ENUM_ATTR_STR(profession, caption, p));
        item->set_military(ENUM_ATTR(profession, military, p));
        item->set_can_assign_labor(ENUM_ATTR(profession, can_assign_labor, p));
        item->set_parent(ENUM_ATTR(profession, parent, p));
    }

    auto pf_labor = out->mutable_labor();
    FOR_ENUM_ITEMS(unit_labor, labor)
    {
        auto item = pf_labor->Add();

        item->set_id(labor);
        item->set_key(ENUM_KEY_STR(unit_labor, labor));
        item->set_caption(ENUM_ATTR_STR(unit_labor, caption, labor));
    }

    return CR_OK;
}

static command_result ListUnits(color_ostream &stream,
                                const ListUnitsIn *in, ListUnitsOut *out)
{
    auto mask = in->has_mask() ? &in->mask() : NULL;

    if (in->id_list_size() > 0)
    {
        for (int i = 0; i < in->id_list_size(); i++)
        {
            auto unit = df::unit::find(in->id_list(i));
            if (unit)
                describeUnit(out->add_value(), unit, mask);
        }
    }

    if (in->scan_all())
    {
        auto &vec = df::unit::get_vector();

        for (size_t i = 0; i < vec.size(); i++)
        {
            auto unit = vec[i];

            if (in->has_race() && unit->race != in->race())
                continue;
            if (in->has_civ_id() && unit->civ_id != in->civ_id())
                continue;
            if (in->has_dead() && Units::isDead(unit) != in->dead())
                continue;
            if (in->has_alive() && Units::isAlive(unit) != in->alive())
                continue;
            if (in->has_sane() && Units::isSane(unit) != in->sane())
                continue;

            describeUnit(out->add_value(), unit, mask);
        }
    }

    return out->value_size() ? CR_OK : CR_NOT_FOUND;
}

static command_result SetUnitLabors(color_ostream &stream, const SetUnitLaborsIn *in)
{
    for (int i = 0; i < in->change_size(); i++)
    {
        auto change = in->change(i);
        auto unit = df::unit::find(change.unit_id());

        if (unit)
            unit->status.labors[change.labor()] = change.value();
    }

    return CR_OK;
}

CoreService::CoreService() {
    suspend_depth = 0;

    // These 2 methods must be first, so that they get id 0 and 1
    addMethod("BindMethod", &CoreService::BindMethod, SF_DONT_SUSPEND);
    addMethod("RunCommand", &CoreService::RunCommand, SF_DONT_SUSPEND);

    // Add others here:
    addMethod("CoreSuspend", &CoreService::CoreSuspend, SF_DONT_SUSPEND);
    addMethod("CoreResume", &CoreService::CoreResume, SF_DONT_SUSPEND);

    // Functions:
    addFunction("GetVersion", GetVersion, SF_DONT_SUSPEND);
    addFunction("GetDFVersion", GetDFVersion, SF_DONT_SUSPEND);

    addFunction("GetWorldInfo", GetWorldInfo);

    addFunction("ListEnums", ListEnums, SF_CALLED_ONCE | SF_DONT_SUSPEND);
    addFunction("ListJobSkills", ListJobSkills, SF_CALLED_ONCE | SF_DONT_SUSPEND);

    addFunction("ListUnits", ListUnits);

    addFunction("SetUnitLabors", SetUnitLabors);
}

CoreService::~CoreService()
{
    while (suspend_depth-- > 0)
        Core::getInstance().Resume();
}

command_result CoreService::BindMethod(color_ostream &stream,
                                       const dfproto::CoreBindRequest *in,
                                       dfproto::CoreBindReply *out)
{
    ServerFunctionBase *fn = connection()->findFunction(stream, in->plugin(), in->method());

    if (!fn)
    {
        stream.printerr("RPC method not found: %s::%s\n",
                        in->plugin().c_str(), in->method().c_str());
        return CR_FAILURE;
    }

    if (fn->p_in_template->GetTypeName() != in->input_msg() ||
        fn->p_out_template->GetTypeName() != in->output_msg())
    {
        stream.printerr("Requested wrong signature for RPC method: %s::%s\n",
                        in->plugin().c_str(), in->method().c_str());
        return CR_FAILURE;
    }

    out->set_assigned_id(fn->getId());
    return CR_OK;
}

command_result CoreService::RunCommand(color_ostream &stream,
                                       const dfproto::CoreRunCommandRequest *in)
{
    std::string cmd = in->command();
    std::vector<std::string> args;
    for (int i = 0; i < in->arguments_size(); i++)
        args.push_back(in->arguments(i));

    return Core::getInstance().runCommand(stream, cmd, args);
}

command_result CoreService::CoreSuspend(color_ostream &stream, const EmptyMessage*, IntMessage *cnt)
{
    Core::getInstance().Suspend();
    cnt->set_value(++suspend_depth);
    return CR_OK;
}

command_result CoreService::CoreResume(color_ostream &stream, const EmptyMessage*, IntMessage *cnt)
{
    if (suspend_depth <= 0)
        return CR_WRONG_USAGE;

    Core::getInstance().Resume();
    cnt->set_value(--suspend_depth);
    return CR_OK;
}
