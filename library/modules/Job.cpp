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

#include <string>
#include <vector>
#include <map>
#include <cassert>
using namespace std;

#include "Core.h"
#include "Error.h"
#include "PluginManager.h"
#include "MiscUtils.h"
#include "Types.h"

#include "modules/Job.h"
#include "modules/Materials.h"
#include "modules/Items.h"

#include "DataDefs.h"
#include "df/world.h"
#include "df/ui.h"
#include "df/job.h"
#include "df/job_list_link.h"
#include "df/specific_ref.h"
#include "df/general_ref.h"
#include "df/general_ref_unit_workerst.h"
#include "df/general_ref_building_holderst.h"

using namespace DFHack;
using namespace df::enums;

df::job *DFHack::Job::cloneJobStruct(df::job *job, bool keepEverything)
{
    CHECK_NULL_POINTER(job);

    df::job *pnew = new df::job(*job);
    
    if ( !keepEverything ) {
        // Clean out transient fields
        pnew->flags.whole = 0;
        pnew->flags.bits.repeat = job->flags.bits.repeat;
        pnew->flags.bits.suspend = job->flags.bits.suspend;
        
        pnew->completion_timer = -1;
    }
    pnew->list_link = NULL;

    //pnew->items.clear();
    //pnew->specific_refs.clear();
    pnew->general_refs.clear();
    //pnew->job_items.clear();
    
    if ( keepEverything ) {
        for ( int a = 0; a < pnew->items.size(); a++ )
            pnew->items[a] = new df::job_item_ref(*pnew->items[a]);
        for ( int a = 0; a < pnew->specific_refs.size(); a++ )
            pnew->specific_refs[a] = new df::specific_ref(*pnew->specific_refs[a]);
    } else {
        pnew->items.clear();
        pnew->specific_refs.clear();
    }
    
    for ( int a = 0; a < job->general_refs.size(); a++ )
        if ( keepEverything || job->general_refs[a]->getType() != df::enums::general_ref_type::UNIT_WORKER )
            pnew->general_refs.push_back(job->general_refs[a]->clone());
    return pnew;
}

void DFHack::Job::deleteJobStruct(df::job *job, bool keptEverything)
{
    if (!job)
        return;

    // Only allow free-floating job structs
    if ( !keptEverything )
        assert(!job->list_link && job->items.empty() && job->specific_refs.empty());
    else
        assert(!job->list_link);
    
    if ( keptEverything ) {
        for ( int a = 0; a < job->items.size(); a++ )
            delete job->items[a];
        for ( int a = 0; a < job->specific_refs.size(); a++ )
            delete job->specific_refs[a];
    }
    for ( int a = 0; a < job->general_refs.size(); a++ )
        delete job->general_refs[a];

    delete job;
}

#define CMP(field) (a.field == b.field)

bool DFHack::operator== (const df::job &a, const df::job &b)
{
    CHECK_NULL_POINTER(&a);
    CHECK_NULL_POINTER(&b);

    if (!(CMP(job_type) &&
          CMP(material) && CMP(matgloss) &&
          CMP(item_subtype) && CMP(item_category.whole)))
        return false;

    return true;
}

void DFHack::Job::printJobDetails(color_ostream &out, df::job *job)
{
    CHECK_NULL_POINTER(job);

    out.color(job->flags.bits.suspend ? COLOR_DARKGREY : COLOR_GREY);
    out << "Job " << job->id << ": " << ENUM_KEY_STR(job_type,job->job_type);
    if (job->flags.whole)
           out << " (" << bitfield_to_string(job->flags) << ")";
    out << endl;
    out.reset_color();

    df::item_type itype = ENUM_ATTR(job_type, item, job->job_type);

    MaterialInfo mat(job);
    if (itype == item_type::FOOD)
        mat.decode(-1);

    if (mat.isValid())
    {
        out << "    material: " << mat.toString();
        out << endl;
    }

    if (job->item_subtype >= 0 || job->item_category.whole)
    {
        ItemTypeInfo iinfo(itype, job->item_subtype);

        out << "    item: " << iinfo.toString()
               << " (" << bitfield_to_string(job->item_category) << ")" << endl;
    }
}

df::general_ref *Job::getGeneralRef(df::job *job, df::general_ref_type type)
{
    CHECK_NULL_POINTER(job);

    return findRef(job->general_refs, type);
}

df::specific_ref *Job::getSpecificRef(df::job *job, df::specific_ref_type type)
{
    CHECK_NULL_POINTER(job);

    return findRef(job->specific_refs, type);
}

df::building *DFHack::Job::getHolder(df::job *job)
{
    CHECK_NULL_POINTER(job);

    for (size_t i = 0; i < job->general_refs.size(); i++)
    {
        VIRTUAL_CAST_VAR(ref, df::general_ref_building_holderst, job->general_refs[i]);
        if (ref)
            return ref->getBuilding();
    }

    return NULL;
}

df::unit *DFHack::Job::getWorker(df::job *job)
{
    CHECK_NULL_POINTER(job);

    for (size_t i = 0; i < job->general_refs.size(); i++)
    {
        VIRTUAL_CAST_VAR(ref, df::general_ref_unit_workerst, job->general_refs[i]);
        if (ref)
            return ref->getUnit();
    }

    return NULL;
}

void DFHack::Job::checkBuildingsNow()
{
    if (df::global::process_jobs)
        *df::global::process_jobs = true;
}

void DFHack::Job::checkDesignationsNow()
{
    if (df::global::process_dig)
        *df::global::process_dig = true;
}

bool DFHack::Job::linkIntoWorld(df::job *job, bool new_id)
{
    using df::global::world;
    using df::global::job_next_id;

    assert(!job->list_link);

    if (new_id) {
        job->id = (*job_next_id)++;

        job->list_link = new df::job_list_link();
        job->list_link->item = job;
        linked_list_append(&world->job_list, job->list_link);
        return true;
    } else {
        df::job_list_link *ins_pos = &world->job_list;
        while (ins_pos->next && ins_pos->next->item->id < job->id)
            ins_pos = ins_pos->next;

        if (ins_pos->next && ins_pos->next->item->id == job->id)
            return false;

        job->list_link = new df::job_list_link();
        job->list_link->item = job;
        linked_list_insert_after(ins_pos, job->list_link);
        return true;
    }
}

bool DFHack::Job::listNewlyCreated(std::vector<df::job*> *pvec, int *id_var)
{
    using df::global::world;
    using df::global::job_next_id;

    pvec->clear();

    if (!job_next_id || *job_next_id <= *id_var)
        return false;

    int old_id = *id_var;
    int cur_id = *job_next_id;

    *id_var = cur_id;

    pvec->reserve(std::min(20,cur_id - old_id));

    df::job_list_link *link = world->job_list.next;
    for (; link; link = link->next)
    {
        int id = link->item->id;
        if (id >= old_id)
            pvec->push_back(link->item);
    }

    return true;
}

bool DFHack::Job::attachJobItem(df::job *job, df::item *item,
                                df::job_item_ref::T_role role,
                                int filter_idx, int insert_idx)
{
    CHECK_NULL_POINTER(job);
    CHECK_NULL_POINTER(item);

    /*
     * TODO: Rewrite for 0.28.181.40d
     */

    if (role != df::job_item_ref::TargetContainer)
    {
        if (item->flags.bits.in_job)
            return false;

        item->flags.bits.in_job = true;
    }

    auto item_link = new df::specific_ref();
    item_link->type = specific_ref_type::JOB;
    item_link->job = job;
    item->specific_refs.push_back(item_link);

    auto job_link = new df::job_item_ref();
    job_link->item = item;
    job_link->role = role;

    if (size_t(insert_idx) < job->items.size())
        vector_insert_at(job->items, insert_idx, job_link);
    else
        job->items.push_back(job_link);

    return true;
}
