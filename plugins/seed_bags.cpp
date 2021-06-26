// Tries to ensure that Dwarves use as few seed bags as possible
// It works about as well as it does in 0.28.181.40d

// If a bunch of seeds all appear at once and there are no bags,
// a few single-seed bags will be made but one of them will quickly
// become the "primary" bag, and the rest can be emptied out manually

#include "Core.h"
#include <Console.h>
#include <Export.h>
#include <PluginManager.h>
#include <MiscUtils.h>
#include <modules/Items.h>
#include <modules/Job.h>
#include <vector>
#include <string>
#include <set>
#include <algorithm>

#include "df/game_mode.h"
#include "df/item.h"
#include "df/items_other_id.h"
#include "df/job.h"
#include "df/unit.h"
#include "df/world.h"

using std::set;
using std::vector;
using std::string;

using namespace DFHack;
using namespace df::enums;

using df::global::gamemode;
using df::global::world;
using df::global::job_next_id;

// State
int job_last_id;

DFHACK_PLUGIN("seed_bags");
DFHACK_PLUGIN_IS_ENABLED(is_enabled);

struct seed_info
{
    int num_seeds;
    int num_jobs;
    int num_bags;

    seed_info() : num_seeds(0), num_jobs(0), num_bags(0) { }
    int fill_level() { return num_seeds + num_jobs; }
    bool has_room() { return fill_level() < num_bags * 100; }
};

DFhackCExport command_result plugin_onupdate (color_ostream &out)
{
    if (*gamemode != game_mode::DWARF)
        return CR_OK;
    if (!is_enabled)
        return CR_OK;
    if (job_last_id == *job_next_id)
        return CR_OK;

    // First, do a quick pass through the job list to see if we have any work to do
    bool have_work = false;
    for (df::job_list_link *link = world->job_list.next; link != NULL; link = link->next)
    {
        df::job *job = link->item;
        if (!job)
            continue;
        if (job->id < job_last_id)
            continue;
        if (job->job_type != job_type::StoreItemInBag)
            continue;
        for (auto item = job->items.begin(); item != job->items.end(); item++)
        {
            if (!(*item)->item)
                continue;
            if ((*item)->item->getType() == item_type::SEEDS)
            {
                have_work = true;
                break;
            }
        }
        if (have_work)
            break;
    }
    // If there's nothing to do, bail out now
    if (!have_work)
        return CR_OK;

    // Next, make a list of all existing seed bags
    std::map<int16_t,std::map<df::coord32,seed_info> > bags;

    // Look in the BOX list and filter from there
    auto &boxes = world->items.other[items_other_id::BOX];
    for (auto box = boxes.begin(); box != boxes.end(); box++)
    {
        // Ignore non-bags
        if (!(*box)->isBag())
            continue;
        std::vector<df::item *> items;
        Items::getContainedItems(*box, &items);

        if (items.size())
        {
            // If the first item inside isn't a seed, it's not a seed bag
            if (items[0]->getType() != item_type::SEEDS)
                continue;

            // If the first item inside IS a seed, then they're ALL seeds
            int16_t plant_id = items[0]->getMaterial();
            auto &cur = bags[plant_id][Items::getPosition(*box)];
            cur.num_seeds += items.size();
            cur.num_bags++;
        }
        else if (!Items::getSpecificRef(*box, specific_ref_type::JOB) &&
                 !Items::getGeneralRef(*box, general_ref_type::BUILDING_HOLDER))
        {
            // Also count empty bags, but only if they're not being used by a job
            // and they're not parts of buildings
            bags[-1][Items::getPosition(*box)].num_bags++;
        }
    }

    // Do another pass through the Job list, this time in detail
    for (df::job_list_link *link = world->job_list.next; link != NULL; link = link->next)
    {
        df::job *job = link->item;
        if (!job)
            continue;

        // We still only want Store Item in Bag jobs
        if (job->job_type != job_type::StoreItemInBag)
            continue;
        int16_t plant_id = -1;
        for (auto item = job->items.begin(); item != job->items.end(); item++)
        {
            if (!(*item)->item)
                continue;
            if ((*item)->item->getType() == item_type::SEEDS)
            {
                plant_id = (*item)->item->getMaterial();
                break;
            }
        }
        if (plant_id == -1)
            continue;

        // Is this an old job?
        if (job->id < job_last_id)
        {
            // Yes - increment that location's job count,
            // but only if there's actually a bag there
            if (bags[plant_id].count(job->pos))
                bags[plant_id][job->pos].num_jobs++;
            // If the bag got picked up, it'll fail and a new job will be created
            continue;
        }

        // If it's a new job, find the best seed bag to put it in
        int best_cap = 0;
        df::coord32 best_bag = job->pos;
        auto cur_bag = bags[plant_id];
        for (auto bag = cur_bag.begin(); bag != cur_bag.end(); bag++)
        {
            // Ignore full bags
            if (!bag->second.has_room())
                continue;

            // Find the fullest bag
            if (bag->second.fill_level() > best_cap)
            {
                best_cap = bag->second.fill_level();
                best_bag = bag->first;
            }
        }

        // If we didn't find any better bags, then use this one and register it
        if (best_bag == job->pos)
        {
            // If it's an empty bag (which it should be),
            // mark it as being for this plant
            if (bags[-1].count(job->pos))
            {
                bags[-1][job->pos].num_bags--;
                if (!bags[-1][job->pos].num_bags)
                    bags[-1].erase(job->pos);
                bags[plant_id][job->pos].num_bags++;
            }
            bags[plant_id][job->pos].num_jobs++;
            continue;
        }

        // We found a better bag, so put it there
        df::coord32 old_pos = job->pos;
        job->pos = best_bag;

        // Is there a worker? If so, update its path destination
        // Hopefully, the unit will recalculate its path in time
        df::unit *worker = Job::getWorker(job);
        if (worker && worker->path.dest == old_pos)
            worker->path.dest = best_bag;
    }

    job_last_id = *job_next_id;

    return CR_OK;
}

DFhackCExport command_result plugin_onstatechange (color_ostream &out, state_change_event event)
{
    switch (event) {
    case SC_MAP_LOADED:
        if (gamemode && *gamemode == game_mode::DWARF)
            job_last_id = -1;
        break;
    }

    return CR_OK;
}

DFhackCExport command_result plugin_enable (color_ostream &out, bool enable)
{
    if (!gamemode || !job_next_id || !world)
        return CR_FAILURE;

    is_enabled = enable;

    return CR_OK;
}

DFhackCExport command_result plugin_init (color_ostream &out, vector <PluginCommand> &commands)
{
    if (!gamemode || !job_next_id || !world)
        return CR_FAILURE;
    return CR_OK;
}

DFhackCExport command_result plugin_shutdown (color_ostream &out)
{
    return CR_OK;
}
