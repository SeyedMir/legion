/* Copyright 2022 Stanford University, NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "legion/legion_ops.h"
#include "legion/legion_trace.h"
#include "legion/legion_views.h"
#include "legion/legion_context.h"
#include "legion/legion_replication.h"

namespace Legion {
  namespace Internal {

    LEGION_EXTERN_LOGGER_DECLARATIONS

#ifdef DEBUG_LEGION_COLLECTIVES
    /////////////////////////////////////////////////////////////
    // Collective Check Reduction
    /////////////////////////////////////////////////////////////
    
    /*static*/ const long CollectiveCheckReduction::IDENTITY = -1;
    /*static*/ const long CollectiveCheckReduction::identity = IDENTITY;
    /*static*/ const long CollectiveCheckReduction::BAD = -2;
    /*static*/ const ReductionOpID CollectiveCheckReduction::REDOP = 
                                                MAX_APPLICATION_REDUCTION_ID;

    //--------------------------------------------------------------------------
    template<>
    /*static*/ void CollectiveCheckReduction::apply<true>(LHS &lhs, RHS rhs)
    //--------------------------------------------------------------------------
    {
      assert(rhs > IDENTITY);
      if (lhs != IDENTITY)
      {
        if (lhs != rhs)
          lhs = BAD;
      }
      else
        lhs = rhs;
    }

    //--------------------------------------------------------------------------
    template<>
    /*static*/ void CollectiveCheckReduction::apply<false>(LHS &lhs, RHS rhs)
    //--------------------------------------------------------------------------
    {
      LHS *ptr = &lhs;
      LHS temp = *ptr;
      while ((temp != BAD) && (temp != rhs))
      {
        if (temp != IDENTITY)
          temp = __sync_val_compare_and_swap(ptr, temp, BAD);
        else
          temp = __sync_val_compare_and_swap(ptr, temp, rhs); 
      }
    }

    //--------------------------------------------------------------------------
    template<>
    /*static*/ void CollectiveCheckReduction::fold<true>(RHS &rhs1, RHS rhs2)
    //--------------------------------------------------------------------------
    {
      assert(rhs2 > IDENTITY);
      if (rhs1 != IDENTITY)
      {
        if (rhs1 != rhs2)
          rhs1 = BAD;
      }
      else
        rhs1 = rhs2;
    }

    //--------------------------------------------------------------------------
    template<>
    /*static*/ void CollectiveCheckReduction::fold<false>(RHS &rhs1, RHS rhs2)
    //--------------------------------------------------------------------------
    {
      RHS *ptr = &rhs1;
      RHS temp = *ptr;
      while ((temp != BAD) && (temp != rhs2))
      {
        if (temp != IDENTITY)
          temp = __sync_val_compare_and_swap(ptr, temp, BAD);
        else
          temp = __sync_val_compare_and_swap(ptr, temp, rhs2);
      }
    }

    /////////////////////////////////////////////////////////////
    // Check Reduction
    /////////////////////////////////////////////////////////////
    
    /*static*/ const CloseCheckReduction::CloseCheckValue 
      CloseCheckReduction::IDENTITY = CloseCheckReduction::CloseCheckValue();
    /*static*/ const CloseCheckReduction::CloseCheckValue
      CloseCheckReduction::identity = IDENTITY;
    /*static*/ const ReductionOpID CloseCheckReduction::REDOP = 
                                              MAX_APPLICATION_REDUCTION_ID + 1;

    //--------------------------------------------------------------------------
    CloseCheckReduction::CloseCheckValue::CloseCheckValue(void)
      : operation_index(0), region_requirement_index(0),
        barrier(RtBarrier::NO_RT_BARRIER), region(LogicalRegion::NO_REGION), 
        partition(LogicalPartition::NO_PART), is_region(true), read_only(false)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    CloseCheckReduction::CloseCheckValue::CloseCheckValue(
        const LogicalUser &user, RtBarrier bar, RegionTreeNode *node, bool read)
      : operation_index(user.op->get_ctx_index()), 
        region_requirement_index(user.idx), barrier(bar),
        is_region(node->is_region()), read_only(read)
    //--------------------------------------------------------------------------
    {
      if (is_region)
        region = node->as_region_node()->handle;
      else
        partition = node->as_partition_node()->handle;
    }

    //--------------------------------------------------------------------------
    bool CloseCheckReduction::CloseCheckValue::operator==(const
                                                     CloseCheckValue &rhs) const
    //--------------------------------------------------------------------------
    {
      if (operation_index != rhs.operation_index)
        return false;
      if (region_requirement_index != rhs.region_requirement_index)
        return false;
      if (barrier != rhs.barrier)
        return false;
      if (read_only != rhs.read_only)
        return false;
      if (is_region != rhs.is_region)
        return false;
      if (is_region)
      {
        if (region != rhs.region)
          return false;
      }
      else
      {
        if (partition != rhs.partition)
          return false;
      }
      return true;
    }

    //--------------------------------------------------------------------------
    template<>
    /*static*/ void CloseCheckReduction::apply<true>(LHS &lhs, RHS rhs)
    //--------------------------------------------------------------------------
    {
      // Only copy over if LHS is the identity
      // This will effectively do a broadcast of one value
      if (lhs == IDENTITY)
        lhs = rhs;
    }

    //--------------------------------------------------------------------------
    template<>
    /*static*/ void CloseCheckReduction::apply<false>(LHS &lhs, RHS rhs)
    //--------------------------------------------------------------------------
    {
      // Not supported at the moment
      assert(false);
    }

    //--------------------------------------------------------------------------
    template<>
    /*static*/ void CloseCheckReduction::fold<true>(RHS &rhs1, RHS rhs2)
    //--------------------------------------------------------------------------
    {
      // Only copy over if RHS1 is the identity
      // This will effectively do a broadcast of one value
      if (rhs1 == IDENTITY)
        rhs1 = rhs2;
    }

    //--------------------------------------------------------------------------
    template<>
    /*static*/ void CloseCheckReduction::fold<false>(RHS &rhs1, RHS rhs2)
    //--------------------------------------------------------------------------
    {
      // Not supported at the moment
      assert(false);
    }
#endif // DEBUG_LEGION_COLLECTIVES

    /////////////////////////////////////////////////////////////
    // Sharded Mapping 
    /////////////////////////////////////////////////////////////
    
    //--------------------------------------------------------------------------
    ShardedMapping::ShardedMapping(const std::set<ShardID> &shards, size_t r)
      : radix(r)
    //--------------------------------------------------------------------------
    {
      unique_sorted_shards.insert(unique_sorted_shards.end(),
                                  shards.begin(), shards.end());
    }

    //--------------------------------------------------------------------------
    ShardedMapping::ShardedMapping(const std::vector<ShardID> &shards, size_t r)
      : radix(r)
    //--------------------------------------------------------------------------
    {
      std::set<ShardID> unique_shards(shards.begin(), shards.end());
      unique_sorted_shards.insert(unique_sorted_shards.end(),
                                  unique_shards.begin(), unique_shards.end());
    }

    //--------------------------------------------------------------------------
    bool ShardedMapping::operator==(const ShardedMapping &rhs) const
    //--------------------------------------------------------------------------
    {
      if (radix != rhs.radix)
        return false;
      if (size() != rhs.size())
        return false;
      for (unsigned idx = 0; idx < unique_sorted_shards.size(); idx++)
        if (unique_sorted_shards[idx] != rhs[idx])
          return false;
      return true;
    }

    //--------------------------------------------------------------------------
    bool ShardedMapping::operator!=(const ShardedMapping &rhs) const
    //--------------------------------------------------------------------------
    {
      return !((*this) == rhs);
    }

    //--------------------------------------------------------------------------
    ShardID ShardedMapping::get_parent(const ShardID origin, 
                                       const ShardID local) const
    //--------------------------------------------------------------------------
    {
      const unsigned local_index = find_index(local);
      const unsigned origin_index = find_index(origin);
#ifdef DEBUG_LEGION
      assert(local_index < unique_sorted_shards.size());
      assert(origin_index < unique_sorted_shards.size());
#endif
      const unsigned offset = convert_to_offset(local_index, origin_index);
      const unsigned index = convert_to_index((offset-1) / radix, origin_index);
      return unique_sorted_shards[index];
    }

    //--------------------------------------------------------------------------
    void ShardedMapping::get_children(const ShardID origin,
                      const ShardID local, std::vector<ShardID> &children) const
    //--------------------------------------------------------------------------
    {
      const unsigned local_index = find_index(local);
      const unsigned origin_index = find_index(origin);
#ifdef DEBUG_LEGION
      assert(local_index < unique_sorted_shards.size());
      assert(origin_index < unique_sorted_shards.size());
#endif
      const unsigned offset = radix * 
        convert_to_offset(local_index, origin_index);
      for (unsigned idx = 1; idx <= radix; idx++)
      {
        const unsigned child_offset = offset + idx;
        if (child_offset < unique_sorted_shards.size())
        {
          const unsigned index = convert_to_index(child_offset, origin_index);
          children.push_back(unique_sorted_shards[index]); 
        }
      }
    }

    //--------------------------------------------------------------------------
    size_t ShardedMapping::count_children(const ShardID origin,
                                          const ShardID local) const
    //--------------------------------------------------------------------------
    {
      const unsigned local_index = find_index(local);
      const unsigned origin_index = find_index(origin);
#ifdef DEBUG_LEGION
      assert(local_index < unique_sorted_shards.size());
      assert(origin_index < unique_sorted_shards.size());
#endif
      const unsigned offset = radix * 
        convert_to_offset(local_index, origin_index);
      size_t child_count = 0;
      for (unsigned idx = 1; idx <= radix; idx++)
      {
        const unsigned child_offset = offset + idx;
        if (child_offset < unique_sorted_shards.size())
          child_count++;
      }
      return child_count;
    }

    //--------------------------------------------------------------------------
    bool ShardedMapping::contains(const ShardID shard) const
    //--------------------------------------------------------------------------
    {
      return (find_index(shard) < unique_sorted_shards.size()); 
    }

    //--------------------------------------------------------------------------
    unsigned ShardedMapping::find_index(const ShardID shard) const
    //--------------------------------------------------------------------------
    {
      // Binary search, will be fast
      unsigned first = 0;
      unsigned last = unique_sorted_shards.size() - 1;
      unsigned mid = 0;
      while (first <= last)
      {
        mid = (first + last) / 2;
        const AddressSpaceID midval = unique_sorted_shards[mid];
        if (shard == midval)
          return mid;
        else if (shard < midval)
          last = mid - 1;
        else if (midval < shard)
          first = mid + 1;
        else
          break;
      }
      return unique_sorted_shards.size();
    }

    //--------------------------------------------------------------------------
    unsigned ShardedMapping::convert_to_offset(unsigned index, 
                                               unsigned origin_index) const
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(index < unique_sorted_shards.size());
      assert(origin_index < unique_sorted_shards.size());
#endif
      if (index < origin_index)
      {
        // Modulus arithmetic here
        return ((index + unique_sorted_shards.size()) - origin_index);
      }
      else
        return (index - origin_index);
    }

    //--------------------------------------------------------------------------
    unsigned ShardedMapping::convert_to_index(unsigned offset,
                                              unsigned origin_index) const
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(offset < unique_sorted_shards.size());
      assert(origin_index < unique_sorted_shards.size());
#endif
      unsigned result = origin_index + offset;
      if (result >= unique_sorted_shards.size())
        result -= unique_sorted_shards.size();
      return result;
    }

    /////////////////////////////////////////////////////////////
    // Repl Collective View Creator
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    template<typename OP>
    ReplCollectiveViewCreator<OP>::ReplCollectiveViewCreator(Runtime *r)
      : OP(r)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    ReplCollectiveViewCreator<OP>::ReplCollectiveViewCreator(
                                       const ReplCollectiveViewCreator<OP> &rhs)
      : OP(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveViewCreator<OP>::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      OP::deactivate(freeop);
      for (typename std::map<RendezvousKey,
                             CollectiveViewRendezvous*>::const_iterator
            it = collective_view_rendezvous.begin(); 
            it != collective_view_rendezvous.end(); it++)
        delete it->second;
      collective_view_rendezvous.clear();
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveViewCreator<OP>::construct_collective_mapping(
                       const RendezvousKey &key,
                       std::map<LogicalRegion,CollectiveRendezvous> &rendezvous)
    //--------------------------------------------------------------------------
    {
      typename std::map<RendezvousKey,
                        CollectiveViewRendezvous*>::const_iterator 
        finder = collective_view_rendezvous.find(key);
#ifdef DEBUG_LEGION
      assert(finder != collective_view_rendezvous.end());
#endif
      finder->second->perform_rendezvous(rendezvous);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveViewCreator<OP>::finalize_collective_mapping(
                       const RendezvousKey &key,
                       std::map<LogicalRegion,CollectiveRendezvous> &rendezvous)
    //--------------------------------------------------------------------------
    {
      // Do the base task call here since we've done the collective rendezvous
      OP::construct_collective_mapping(key, rendezvous);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveViewCreator<OP>::create_collective_view_rendezvous(
          RegionTreeID tid, unsigned requirement_index, unsigned analysis_index)
    //--------------------------------------------------------------------------
    {
      const RendezvousKey key(requirement_index, analysis_index);
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = 
        dynamic_cast<ReplicateContext*>(this->get_context());
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = 
        static_cast<ReplicateContext*>(this->get_context());
#endif
      // This should always be in the dependence analysis stage of the pipeline
      // so we need to make sure we make the right kind of collective ID
      const CollectiveID id =
        repl_ctx->get_next_collective_index(COLLECTIVE_LOC_19, true/*logical*/);
      collective_view_rendezvous[key] = 
        new CollectiveViewRendezvous(id, repl_ctx, this, this, key, tid);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveViewCreator<OP>::shard_off_collective_view_rendezvous(
                                               std::set<RtEvent> &preconditions)
    //--------------------------------------------------------------------------
    {
      std::map<LogicalRegion,CollectiveRendezvous> empty_rendezvous;
      for (typename std::map<RendezvousKey,
                             CollectiveViewRendezvous*>::const_iterator
            it = collective_view_rendezvous.begin(); 
            it != collective_view_rendezvous.end(); it++)
      {
        it->second->perform_rendezvous(empty_rendezvous);
        preconditions.insert(it->second->get_done_event());
      }
    }

#ifdef NO_EXPLICIT_COLLECTIVES
    /////////////////////////////////////////////////////////////
    // Repl Collective Instance Creator
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    template<typename OP>
    ReplCollectiveInstanceCreator<OP>::ReplCollectiveInstanceCreator(Runtime *r)
      : OP(r)
    //--------------------------------------------------------------------------
    {
      shard_mapping.store(NULL);
      first_entry.store(true);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    ReplCollectiveInstanceCreator<OP>::ReplCollectiveInstanceCreator(
                                   const ReplCollectiveInstanceCreator<OP> &rhs)
      : OP(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::
                                 activate_repl_collective_instance_creator(void)
    //--------------------------------------------------------------------------
    {
      shard_mapping.store(NULL);
      first_entry.store(true);
      collectives_done = RtUserEvent::NO_RT_USER_EVENT;
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::
                               deactivate_repl_collective_instance_creator(void)
    //--------------------------------------------------------------------------
    {
      if (!this->first_entry.load())
      {
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(this->get_context());
        assert(repl_ctx != NULL);
#else
        ReplicateContext *repl_ctx =
          static_cast<ReplicateContext*>(this->get_context());
#endif
        repl_ctx->unregister_collective_instance_handler(
                              this->get_context_index());
      }
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    ShardID ReplCollectiveInstanceCreator<OP>::
                                      get_collective_instance_origin_shard(void)
    //--------------------------------------------------------------------------
    {
      ShardedMapping *mapping = shard_mapping.load();
      if (mapping == NULL)
      {
        mapping = get_collective_instance_sharded_mapping();
        // Try to write it and see if we lost the race
        ShardedMapping *previous = NULL;
        if (!shard_mapping.compare_exchange_strong(previous, mapping))
        {
          // We lost the race
          if (mapping->remove_reference())
            delete mapping;
          mapping = previous;
        }
      }
      unsigned offset = this->get_context_index() % mapping->size();
      return (*mapping)[offset];
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    size_t ReplCollectiveInstanceCreator<OP>::
                                      get_total_collective_instance_points(void)
    //--------------------------------------------------------------------------
    {
      ShardID origin_shard = get_collective_instance_origin_shard();
      ShardedMapping *mapping = shard_mapping.load();
#ifdef DEBUG_LEGION
      assert(mapping != NULL);
      ReplicateContext *repl_ctx = 
        dynamic_cast<ReplicateContext*>(this->get_context());
      assert(repl_ctx != NULL);
      assert(mapping->contains(repl_ctx->owner_shard->shard_id));
#else
      ReplicateContext *repl_ctx =
        static_cast<ReplicateContext*>(this->get_context());
#endif
      // Figure out how many local points we have plus how ever
      // many messages we are expecting from "children" shards
      return OP::get_total_collective_instance_points() +
                mapping->count_children(origin_shard,
                          repl_ctx->owner_shard->shard_id);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::register_handler(void)
    //--------------------------------------------------------------------------
    {
      if (first_entry.exchange(false))
      {
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(this->get_context());
        assert(repl_ctx != NULL);
#else
        ReplicateContext *repl_ctx =
          static_cast<ReplicateContext*>(this->get_context());
#endif
        repl_ctx->register_collective_instance_handler(
                        this->get_context_index(), this);
      }
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::finalize_collective(void)
    //--------------------------------------------------------------------------
    {
      RtUserEvent to_trigger;
      {
        AutoLock o_lock(this->op_lock);
        if (collectives_done.exists() && this->pending_privileges.empty() &&
            this->pending_collectives.empty() && 
            this->pending_matches.empty() && this->pending_finalizes.empty() && 
            this->pending_verifications.empty() && this->pending_counts.empty())
          to_trigger = collectives_done;
      }
      if (to_trigger.exists())
        Runtime::trigger_event(to_trigger);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::trigger_commit(void)
    //--------------------------------------------------------------------------
    {
      // In safe mapper mode make sure the we've finished our collectives
      RtEvent wait_on;
      {
        AutoLock o_lock(this->op_lock);
        if (!this->pending_privileges.empty() || 
            !this->pending_collectives.empty() ||
            !this->pending_matches.empty() ||
            !this->pending_finalizes.empty() ||
            !this->pending_verifications.empty() || 
            !this->pending_counts.empty())
        {
#ifdef DEBUG_LEGION
          assert(!collectives_done.exists());
#endif
          collectives_done = Runtime::create_rt_user_event();
          wait_on = collectives_done;
        }
      }
      if (wait_on.exists() && !wait_on.has_triggered())
        wait_on.wait();
      OP::trigger_commit();
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    RtEvent 
    ReplCollectiveInstanceCreator<OP>::acquire_collective_allocation_privileges(
                     MappingCallKind mapper_call, unsigned index, Memory target)
    //--------------------------------------------------------------------------
    {
      AutoCheck check(this);
      return OP::acquire_collective_allocation_privileges(mapper_call,
                                                          index, target);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    RtEvent 
    ReplCollectiveInstanceCreator<OP>::acquire_collective_allocation_privileges(
                                 MappingCallKind mapper_call, unsigned index,
                                 const std::set<Memory> &targets, size_t points)
    //--------------------------------------------------------------------------
    {
      AutoCheck check(this);
      return OP::acquire_collective_allocation_privileges(mapper_call, index,
                                                          targets, points);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void 
    ReplCollectiveInstanceCreator<OP>::release_collective_allocation_privileges(
                     MappingCallKind mapper_call, unsigned index, size_t points)
    //--------------------------------------------------------------------------
    {
      AutoCheck check(this);
      OP::release_collective_allocation_privileges(mapper_call, index, points);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    PendingCollectiveManager* 
        ReplCollectiveInstanceCreator<OP>::create_pending_collective_manager(
                 MappingCallKind mapper_call, unsigned index,
                 size_t collective_tag, const LayoutConstraintSet &constraints,
                 const std::vector<LogicalRegion> &regions,
                 AddressSpaceID memory_space,
                 LayoutConstraintKind &bad_constraint,
                 size_t &bad_index, bool &bad_regions)
    //--------------------------------------------------------------------------
    {
      AutoCheck check(this);
      return OP::create_pending_collective_manager(mapper_call, index,
          collective_tag, constraints, regions, memory_space, 
          bad_constraint, bad_index, bad_regions);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::create_pending_collective_managers(
         MappingCallKind mapper_call, unsigned index,
         const std::map<size_t,typename OP::PendingCollective> &instances,
         std::map<size_t,PendingCollectiveManager*> &collectives, size_t points,
         LayoutConstraintKind &bad_kind, size_t &bad_index, bool &bad_regions)
    //--------------------------------------------------------------------------
    {
      AutoCheck check(this);
      OP::create_pending_collective_managers(mapper_call, index, instances,
          collectives, points, bad_kind, bad_index, bad_regions);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::match_collective_instances(
             MappingCallKind mapper_call, unsigned index, size_t collective_tag,
             std::vector<MappingInstance> &instances)
    //--------------------------------------------------------------------------
    {
      AutoCheck check(this);
      OP::match_collective_instances(mapper_call, index,
                                     collective_tag, instances);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::match_collective_instances(
       MappingCallKind mapper_call, unsigned index,
       std::map<size_t,std::vector<DistributedID> > &instances, size_t points)
    //--------------------------------------------------------------------------
    {
      AutoCheck check(this);
      OP::match_collective_instances(mapper_call, index, instances, points);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    bool 
      ReplCollectiveInstanceCreator<OP>::finalize_pending_collective_instance(
                                    MappingCallKind mapper_call, unsigned index,
                                    bool success, size_t points)
    //--------------------------------------------------------------------------
    {
      AutoCheck check(this);
      return OP::finalize_pending_collective_instance(mapper_call, index,
                                                      success, points);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    unsigned 
     ReplCollectiveInstanceCreator<OP>::verify_total_collective_instance_calls(
               MappingCallKind mapper_call, unsigned total_calls, size_t points) 
    //--------------------------------------------------------------------------
    {
      AutoCheck check(this);
      return OP::verify_total_collective_instance_calls(mapper_call,
                                                        total_calls, points);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    size_t
      ReplCollectiveInstanceCreator<OP>::count_collective_region_occurrences(
                   unsigned index, LogicalRegion region, DistributedID inst_did)
    //--------------------------------------------------------------------------
    {
      AutoCheck check(this);
      return OP::count_collective_region_occurrences(index, region, inst_did);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::count_collective_region_occurrences(
                    unsigned index, RegionInstanceCounts &counts, size_t points)
    //--------------------------------------------------------------------------
    {
      AutoCheck check(this);
      OP::count_collective_region_occurrences(index, counts, points);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::
                perform_acquire_collective_allocation_privileges(
                                    MappingCallKind mapper_call, unsigned index,
                                    const std::set<Memory> &targets,
                                    RtUserEvent to_trigger)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = 
        dynamic_cast<ReplicateContext*>(this->get_context());
      assert(repl_ctx != NULL);
      assert(shard_mapping.load() != NULL);
#else
      ReplicateContext *repl_ctx =
        static_cast<ReplicateContext*>(this->get_context());
#endif
      // Check to see if we are the owner shard or not
      ShardID origin_shard = get_collective_instance_origin_shard();
      if (origin_shard != repl_ctx->owner_shard->shard_id)
      {
        const ShardID parent_shard = shard_mapping.load()->
          get_parent(origin_shard, repl_ctx->owner_shard->shard_id);
        // Package this up and send it off to the parent shard
        Serializer rez;
        rez.serialize(repl_ctx->shard_manager->repl_id);
        rez.serialize(parent_shard);
        rez.serialize<size_t>(this->get_context_index());
        rez.serialize(REPL_COLLECTIVE_ACQUIRE_ALLOCATION_PRIVILEGE);
        rez.serialize(mapper_call);
        rez.serialize(index);
        rez.serialize<size_t>(targets.size());
        for (std::set<Memory>::const_iterator it =
              targets.begin(); it != targets.end(); it++)
          rez.serialize(*it);
        rez.serialize(to_trigger);
        repl_ctx->shard_manager->send_collective_instance_message(
                                                parent_shard, rez); 
      }
      else // we're the origin shard so we do the base call
        OP::perform_acquire_collective_allocation_privileges(mapper_call,
                                              index, targets, to_trigger);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::
                perform_release_collective_allocation_privileges(
                                    MappingCallKind mapper_call, unsigned index,
                                    const std::set<Memory> &targets)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = 
        dynamic_cast<ReplicateContext*>(this->get_context());
      assert(repl_ctx != NULL);
      assert(shard_mapping.load() != NULL);
#else
      ReplicateContext *repl_ctx =
        static_cast<ReplicateContext*>(this->get_context());
#endif
      // Check to see if we are the owner shard or not
      ShardID origin_shard = get_collective_instance_origin_shard();
      if (origin_shard != repl_ctx->owner_shard->shard_id)
      {
        const ShardID parent_shard = shard_mapping.load()->
          get_parent(origin_shard, repl_ctx->owner_shard->shard_id);
        Serializer rez;
        rez.serialize(repl_ctx->shard_manager->repl_id);
        rez.serialize(parent_shard);
        rez.serialize<size_t>(this->get_context_index());
        rez.serialize(REPL_COLLECTIVE_RELEASE_ALLOCATION_PRIVILEGE);
        rez.serialize(mapper_call);
        rez.serialize(index);
        repl_ctx->shard_manager->send_collective_instance_message(
                                                parent_shard, rez);
      }
      else
        OP::perform_release_collective_allocation_privileges(mapper_call,
                                                             index, targets);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::
                perform_create_pending_collective_managers(
                                  MappingCallKind mapper_call, unsigned index, 
                                  const std::map<size_t,
                                    typename OP::PendingCollective> &instances,
                                  LayoutConstraintKind bad_constraint,
                                  size_t bad_index, bool bad_regions)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = 
        dynamic_cast<ReplicateContext*>(this->get_context());
      assert(repl_ctx != NULL);
      assert(shard_mapping.load() != NULL);
#else
      ReplicateContext *repl_ctx =
        static_cast<ReplicateContext*>(this->get_context());
#endif
      // Check to see if we were the owner shard or not
      ShardID origin_shard = get_collective_instance_origin_shard();
      if (origin_shard != repl_ctx->owner_shard->shard_id)
      {
        const ShardID parent_shard = shard_mapping.load()->
          get_parent(origin_shard, repl_ctx->owner_shard->shard_id);
        Serializer rez;
        rez.serialize(repl_ctx->shard_manager->repl_id);
        rez.serialize(parent_shard);
        rez.serialize<size_t>(this->get_context_index());
        rez.serialize(REPL_COLLECTIVE_CREATE_PENDING_MANAGERS);
        rez.serialize(mapper_call);
        rez.serialize(index);
        rez.serialize<bool>(true/*request*/);
        rez.serialize<size_t>(instances.size());
        for (typename std::map<size_t,typename OP::PendingCollective>::
              const_iterator pit = instances.begin();
              pit != instances.end(); pit++)
        {
          rez.serialize(pit->first);
          pit->second.constraints->serialize(rez);
          rez.serialize<size_t>(pit->second.regions.size());
          for (std::vector<LogicalRegion>::const_iterator it = 
                pit->second.regions.begin(); it !=
                pit->second.regions.end(); it++)
            rez.serialize(*it);
          rez.serialize<size_t>(pit->second.memory_spaces.size());
          for (std::map<AddressSpaceID,unsigned>::const_iterator it =
                pit->second.memory_spaces.begin(); it !=
                pit->second.memory_spaces.end(); it++)
          {
            rez.serialize(it->first);
            rez.serialize(it->second);
          }
          rez.serialize(pit->second.total_points);
        }
        rez.serialize(bad_constraint);
        rez.serialize(bad_index);
        rez.serialize<bool>(bad_regions);
        rez.serialize(repl_ctx->owner_shard->shard_id);

        repl_ctx->shard_manager->send_collective_instance_message(
                                                parent_shard, rez);
      }
      else
        OP::perform_create_pending_collective_managers(mapper_call, index,
                        instances, bad_constraint, bad_index, bad_regions);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::
                        perform_match_collective_instances(
                                    MappingCallKind mapper_call, unsigned index,
                                    std::map<size_t,
                                      std::vector<DistributedID> > &instances)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = 
        dynamic_cast<ReplicateContext*>(this->get_context());
      assert(repl_ctx != NULL);
      assert(shard_mapping.load() != NULL);
#else
      ReplicateContext *repl_ctx =
        static_cast<ReplicateContext*>(this->get_context());
#endif
      // Check to see if we are the owner shard or not
      ShardID origin_shard = get_collective_instance_origin_shard();
      if (origin_shard != repl_ctx->owner_shard->shard_id)
      {
        const ShardID parent_shard = shard_mapping.load()->
          get_parent(origin_shard, repl_ctx->owner_shard->shard_id);
        Serializer rez;
        rez.serialize(repl_ctx->shard_manager->repl_id);
        rez.serialize(parent_shard);
        rez.serialize<size_t>(this->get_context_index());
        rez.serialize(REPL_COLLECTIVE_MATCH_INSTANCES);
        rez.serialize(mapper_call);
        rez.serialize(index);
        rez.serialize<bool>(true/*request*/);
        rez.serialize<size_t>(instances.size());
        for (std::map<size_t,std::vector<DistributedID> >::iterator it =
              instances.begin(); it != instances.end(); it++)
        {
          rez.serialize(it->first);
          rez.serialize<size_t>(it->second.size());
          for (std::vector<DistributedID>::const_iterator dit =
                it->second.begin(); dit != it->second.end(); dit++)
            rez.serialize(*dit);
        }
        rez.serialize(repl_ctx->owner_shard->shard_id);

        repl_ctx->shard_manager->send_collective_instance_message(
                                                parent_shard, rez);
      }
      else
        OP::perform_match_collective_instances(mapper_call, index, instances);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::
                perform_finalize_pending_collective_instance(
                      MappingCallKind mapper_call, unsigned index, bool success)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = 
        dynamic_cast<ReplicateContext*>(this->get_context());
      assert(repl_ctx != NULL);
      assert(shard_mapping.load() != NULL);
#else
      ReplicateContext *repl_ctx =
        static_cast<ReplicateContext*>(this->get_context());
#endif
      // Check to see if we are the owner shard or not
      ShardID origin_shard = get_collective_instance_origin_shard();
      if (origin_shard != repl_ctx->owner_shard->shard_id)
      {
        const ShardID parent_shard = shard_mapping.load()->
          get_parent(origin_shard, repl_ctx->owner_shard->shard_id);
        Serializer rez;
        rez.serialize(repl_ctx->shard_manager->repl_id);
        rez.serialize(parent_shard);
        rez.serialize<size_t>(this->get_context_index());
        rez.serialize(REPL_COLLECTIVE_FINALIZE_COLLECTIVE_INSTANCE);
        rez.serialize(mapper_call);
        rez.serialize(index);
        rez.serialize<bool>(true/*request*/);
        rez.serialize<bool>(success);
        rez.serialize(repl_ctx->owner_shard->shard_id);

        repl_ctx->shard_manager->send_collective_instance_message(
                                                parent_shard, rez);
      }
      else
        OP::perform_finalize_pending_collective_instance(mapper_call,
                                                         index, success);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::
                     perform_verify_total_collective_instance_calls( 
                              MappingCallKind mapper_call, unsigned total_calls)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = 
        dynamic_cast<ReplicateContext*>(this->get_context());
      assert(repl_ctx != NULL);
      assert(shard_mapping.load() != NULL);
#else
      ReplicateContext *repl_ctx =
        static_cast<ReplicateContext*>(this->get_context());
#endif
      // Check to see if we are the owner shard or not
      ShardID origin_shard = get_collective_instance_origin_shard();
      if (origin_shard != repl_ctx->owner_shard->shard_id)
      {
        const ShardID parent_shard = shard_mapping.load()->
          get_parent(origin_shard, repl_ctx->owner_shard->shard_id);
        Serializer rez;
        rez.serialize(repl_ctx->shard_manager->repl_id);
        rez.serialize(parent_shard);
        rez.serialize<size_t>(this->get_context_index());
        rez.serialize(REPL_COLLECTIVE_VERIFY_TOTAL_CALLS);
        rez.serialize(mapper_call);
        rez.serialize<bool>(true/*request*/);
        rez.serialize(total_calls);
        rez.serialize(repl_ctx->owner_shard->shard_id);

        repl_ctx->shard_manager->send_collective_instance_message(
                                                parent_shard, rez);
      }
      else
        OP::perform_verify_total_collective_instance_calls(mapper_call,
                                                           total_calls);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::
              perform_count_collective_region_occurrences(unsigned index,
                                                   RegionInstanceCounts &counts)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = 
        dynamic_cast<ReplicateContext*>(this->get_context());
      assert(repl_ctx != NULL);
      assert(shard_mapping.load() != NULL);
#else
      ReplicateContext *repl_ctx =
        static_cast<ReplicateContext*>(this->get_context());
#endif
      // Check to see if we are the owner shard or not
      ShardID origin_shard = get_collective_instance_origin_shard();
      if (origin_shard != repl_ctx->owner_shard->shard_id)
      {
        const ShardID parent_shard = shard_mapping.load()->
          get_parent(origin_shard, repl_ctx->owner_shard->shard_id);
        Serializer rez;
        rez.serialize(repl_ctx->shard_manager->repl_id);
        rez.serialize(parent_shard);
        rez.serialize<size_t>(this->get_context_index());
        rez.serialize(REPL_COLLECTIVE_COUNT_REGION_OCCURRENCES);
        rez.serialize(index);
        rez.serialize<bool>(true); // request
        rez.serialize<size_t>(counts.size());
        for (RegionInstanceCounts::const_iterator it =
              counts.begin(); it != counts.end(); it++)
        {
          rez.serialize(it->first.first);
          rez.serialize(it->first.second);
          rez.serialize(it->second);
        }
        rez.serialize(repl_ctx->owner_shard->shard_id);

        repl_ctx->shard_manager->send_collective_instance_message(
                                                parent_shard, rez);
      }
      else
        OP::perform_count_collective_region_occurrences(index, counts);
    }

    //--------------------------------------------------------------------------
    template<typename OP>
    void ReplCollectiveInstanceCreator<OP>::
                         handle_collective_instance_message(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = 
        dynamic_cast<ReplicateContext*>(this->get_context());
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx =
        static_cast<ReplicateContext*>(this->get_context());
#endif
      ReplCollectiveInstanceMessageKind message;
      derez.deserialize(message);
      switch (message)
      {
        case REPL_COLLECTIVE_ACQUIRE_ALLOCATION_PRIVILEGE:
          {
            MappingCallKind mapper_call;
            derez.deserialize(mapper_call);
            unsigned index;
            derez.deserialize(index);
            size_t num_targets;
            derez.deserialize(num_targets);
            std::set<Memory> targets;
            for (unsigned idx = 0; idx < num_targets; idx++)
            {
              Memory target;
              derez.deserialize(target);
              targets.insert(target);
            }
            RtUserEvent to_trigger;
            derez.deserialize(to_trigger);
            const RtEvent ready = 
              this->acquire_collective_allocation_privileges(mapper_call, index,
                  targets, 1/*only one "point" for all upstream shards*/);
            Runtime::trigger_event(to_trigger, ready);
            break;
          }
        case REPL_COLLECTIVE_RELEASE_ALLOCATION_PRIVILEGE:
          {
            MappingCallKind mapper_call;
            derez.deserialize(mapper_call);
            unsigned index;
            derez.deserialize(index);
            this->release_collective_allocation_privileges(mapper_call, index);
            break;
          }
        case REPL_COLLECTIVE_CREATE_PENDING_MANAGERS:
          {
            MappingCallKind mapper_call;
            derez.deserialize(mapper_call);
            unsigned index;
            derez.deserialize(index);
            bool request;
            derez.deserialize<bool>(request);
            if (request)
            {
              size_t num_pending;
              derez.deserialize(num_pending);
              std::map<size_t,typename OP::PendingCollective> instances;
              std::vector<LayoutConstraintSet> constraints(num_pending);
              for (unsigned idx = 0; idx < num_pending; idx++)
              {
                size_t collective_tag;
                derez.deserialize(collective_tag);
                constraints[idx].deserialize(derez);
                size_t num_regions;
                derez.deserialize(num_regions);
                std::vector<LogicalRegion> regions(num_regions);
                for (unsigned idx2 = 0; idx2 < num_regions; idx2++)
                  derez.deserialize(regions[idx2]);
                size_t num_spaces;
                derez.deserialize(num_spaces);
                std::map<AddressSpaceID,unsigned> memory_spaces;
                for (unsigned idx2 = 0; idx2 < num_spaces; idx2++)
                {
                  AddressSpaceID space;
                  derez.deserialize(space);
                  derez.deserialize(memory_spaces[space]);
                }
                size_t total_points;
                derez.deserialize(total_points);

                typename std::map<size_t,typename OP::PendingCollective>::
                  iterator it = instances.insert(
                      std::pair<size_t,typename OP::PendingCollective>(
                        collective_tag, typename OP::PendingCollective(
                          constraints[idx], regions, total_points))).first;
                memory_spaces.swap(it->second.memory_spaces);
              }
              LayoutConstraintKind bad_constraint;
              derez.deserialize(bad_constraint);
              size_t bad_index;
              derez.deserialize(bad_index);
              bool bad_regions;
              derez.deserialize<bool>(bad_regions);
              ShardID source_shard;
              derez.deserialize(source_shard);

              std::map<size_t,PendingCollectiveManager*> collectives;
              this->create_pending_collective_managers(mapper_call, index,
                  instances, collectives, 1/*only one point*/,
                  bad_constraint, bad_index, bad_regions);

              // Send the response back
              Serializer rez;
              rez.serialize(repl_ctx->shard_manager->repl_id);
              rez.serialize(source_shard);
              rez.serialize<size_t>(this->get_context_index());
              rez.serialize(message);
              rez.serialize(mapper_call);
              rez.serialize(index);
              rez.serialize<bool>(false); // not a request but a return 
              rez.serialize<size_t>(collectives.size());
              for (std::map<size_t,PendingCollectiveManager*>::const_iterator
                    it = collectives.begin(); it != collectives.end(); it++)
              {
                rez.serialize(it->first);
                if (it->second != NULL)
                {
                  it->second->pack(rez);
                  if (it->second->remove_reference())
                    delete it->second;
                }
                else
                  rez.serialize<DistributedID>(0);
              }
              rez.serialize(bad_constraint);
              rez.serialize(bad_index);
              rez.serialize<bool>(bad_regions);
              repl_ctx->shard_manager->send_collective_instance_message(
                                                      source_shard, rez); 
            }
            else
            {
              size_t num_collectives;
              derez.deserialize(num_collectives);
              std::map<size_t,PendingCollectiveManager*> collectives;
              for (unsigned idx = 0; idx < num_collectives; idx++)
              {
                size_t collective_tag;
                derez.deserialize(collective_tag);
                PendingCollectiveManager *manager =
                  PendingCollectiveManager::unpack(derez);
                if (manager != NULL)
                  manager->add_reference();
                collectives[collective_tag] = manager;
              }
              LayoutConstraintKind bad_constraint;
              derez.deserialize(bad_constraint);
              size_t bad_index;
              derez.deserialize(bad_index);
              bool bad_regions;
              derez.deserialize<bool>(bad_regions);
              this->return_create_pending_collective_managers(mapper_call,
                  index, collectives, bad_constraint, bad_index, bad_regions);
            }
            break;
          }
        case REPL_COLLECTIVE_MATCH_INSTANCES:
          {
            MappingCallKind mapper_call;
            derez.deserialize(mapper_call);
            unsigned index;
            derez.deserialize(index);
            bool request;
            derez.deserialize<bool>(request);
            if (request)
            {
              size_t num_tags;
              derez.deserialize(num_tags);
              std::map<size_t,std::vector<DistributedID> > instances;
              for (unsigned idx1 = 0; idx1 < num_tags; idx1++)
              {
                size_t collective_tag;
                derez.deserialize(collective_tag);
                size_t num_instances;
                derez.deserialize(num_instances);
                std::vector<DistributedID> &insts = instances[collective_tag];
                insts.resize(num_instances);
                for (unsigned idx2 = 0; idx2 < num_instances; idx2++)
                  derez.deserialize(insts[idx2]);
              }
              ShardID source_shard;
              derez.deserialize(source_shard);

              this->match_collective_instances(mapper_call, index,
                                               instances, 1/*points*/);
              Serializer rez;
              rez.serialize(repl_ctx->shard_manager->repl_id);
              rez.serialize(source_shard);
              rez.serialize<size_t>(this->get_context_index());
              rez.serialize(message);
              rez.serialize(mapper_call);
              rez.serialize(index);
              rez.serialize<bool>(false); // not a request but a return
              rez.serialize(instances.size());
              for (std::map<size_t,std::vector<DistributedID> >::iterator it =
                    instances.begin(); it != instances.end(); it++)
              {
                rez.serialize(it->first);
                rez.serialize<size_t>(it->second.size());
                for (std::vector<DistributedID>::const_iterator dit =
                      it->second.begin(); dit != it->second.end(); dit++)
                  rez.serialize(*dit);
              }
              repl_ctx->shard_manager->send_collective_instance_message(
                                                      source_shard, rez);
            }
            else
            {
              size_t num_tags;
              derez.deserialize(num_tags);
              std::map<size_t,std::vector<DistributedID> > instances;
              for (unsigned idx1 = 0; idx1 < num_tags; idx1++)
              {
                size_t collective_tag;
                derez.deserialize(collective_tag);
                size_t num_instances;
                derez.deserialize(num_instances);
                std::vector<DistributedID> &insts = instances[collective_tag];
                insts.resize(num_instances);
                for (unsigned idx2 = 0; idx2 < num_instances; idx2++)
                  derez.deserialize(insts[idx2]);
              }
              this->return_match_collective_instances(mapper_call, index,
                                                      instances);
            }
            break;
          }
        case REPL_COLLECTIVE_FINALIZE_COLLECTIVE_INSTANCE:
          {
            MappingCallKind mapper_call;
            derez.deserialize(mapper_call);
            unsigned index;
            derez.deserialize(index);
            bool request;
            derez.deserialize<bool>(request);
            if (request)
            {
              bool success;
              derez.deserialize<bool>(success);
              ShardID source_shard;
              derez.deserialize(source_shard);

              const bool result =
                this->finalize_pending_collective_instance(mapper_call, index,
                    success, 1/*only one point is being passed in here*/);

              Serializer rez;
              rez.serialize(repl_ctx->shard_manager->repl_id);
              rez.serialize(source_shard);
              rez.serialize<size_t>(this->get_context_index());
              rez.serialize(message);
              rez.serialize(mapper_call);
              rez.serialize(index);
              rez.serialize<bool>(false); // not a request but a return
              rez.serialize<bool>(result);

              repl_ctx->shard_manager->send_collective_instance_message(
                                                      source_shard, rez);
            }
            else
            {
              bool success;
              derez.deserialize<bool>(success);
              this->return_finalize_pending_collective_instance(mapper_call,
                                                                index, success);
            }
            break;
          }
        case REPL_COLLECTIVE_VERIFY_TOTAL_CALLS:
          {
            MappingCallKind mapper_call;
            derez.deserialize(mapper_call);
            bool request;
            derez.deserialize<bool>(request);
            if (request)
            {
              unsigned total_calls;
              derez.deserialize(total_calls);
              ShardID source_shard;
              derez.deserialize(source_shard);

              const unsigned result =
                this->verify_total_collective_instance_calls(mapper_call, total_calls,
                    1/*only one point is returned in this case*/);

              Serializer rez;
              rez.serialize(repl_ctx->shard_manager->repl_id);
              rez.serialize(source_shard);
              rez.serialize<size_t>(this->get_context_index());
              rez.serialize(message);
              rez.serialize(mapper_call);
              rez.serialize<bool>(false); // not a request but a return
              rez.serialize(result);

              repl_ctx->shard_manager->send_collective_instance_message(
                                                      source_shard, rez);
            }
            else
            {
              unsigned total_calls;
              derez.deserialize(total_calls);
              this->return_verify_total_collective_instance_calls(mapper_call,
                                                                  total_calls);
            }
            break;
          }
        case REPL_COLLECTIVE_COUNT_REGION_OCCURRENCES:
          {
            unsigned index;
            derez.deserialize(index);
            bool request;
            derez.deserialize<bool>(request);
            if (request)
            {
              size_t num_counts;
              derez.deserialize(num_counts);
              RegionInstanceCounts counts;
              for (unsigned idx = 0; idx < num_counts; idx++)
              {
                std::pair<LogicalRegion,DistributedID> key;
                derez.deserialize(key.first);
                derez.deserialize(key.second);
                derez.deserialize(counts[key]);
              }
              ShardID source_shard;
              derez.deserialize(source_shard);

              this->count_collective_region_occurrences(index, counts,
                  1/*only one point returns in this case*/);

              Serializer rez;
              rez.serialize(repl_ctx->shard_manager->repl_id);
              rez.serialize(source_shard);
              rez.serialize<size_t>(this->get_context_index());
              rez.serialize(message);
              rez.serialize(index);
              rez.serialize<bool>(false); // this is not a request
              rez.serialize<size_t>(counts.size());
              for (RegionInstanceCounts::const_iterator it =
                    counts.begin(); it != counts.end(); it++)
              {
                rez.serialize(it->first.first);
                rez.serialize(it->first.second);
                rez.serialize(it->second);
              }

              repl_ctx->shard_manager->send_collective_instance_message(
                                                      source_shard, rez);
            }
            else
            {
              size_t num_counts;
              derez.deserialize(num_counts);
              RegionInstanceCounts counts;
              for (unsigned idx = 0; idx < num_counts; idx++)
              {
                std::pair<LogicalRegion,DistributedID> key;
                derez.deserialize(key.first);
                derez.deserialize(key.second);
                derez.deserialize(counts[key]);
              }
              this->return_count_collective_region_occurrences(index, counts);
            }
            break;
          }
        default:
          assert(false);
      }
    } 
#endif // NO_EXPLICIT_COLLECTIVES

    /////////////////////////////////////////////////////////////
    // Repl Individual Task 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplIndividualTask::ReplIndividualTask(Runtime *rt)
      : IndividualTask(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplIndividualTask::ReplIndividualTask(const ReplIndividualTask &rhs)
      : IndividualTask(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplIndividualTask::~ReplIndividualTask(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplIndividualTask& ReplIndividualTask::operator=(
                                                  const ReplIndividualTask &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplIndividualTask::activate(void)
    //--------------------------------------------------------------------------
    {
      IndividualTask::activate();
      owner_shard = 0;
      launch_space = NULL;
      sharding_functor = UINT_MAX;
      sharding_function = NULL;
      future_collective_id = UINT_MAX;
      future_collective = NULL;
#ifdef DEBUG_LEGION
      sharding_collective = NULL; 
#endif
    }

    //--------------------------------------------------------------------------
    void ReplIndividualTask::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      IndividualTask::deactivate(false/*free*/);
#ifdef DEBUG_LEGION
      if (sharding_collective != NULL)
        delete sharding_collective;
#endif
      if (future_collective != NULL)
        delete future_collective;
      if (freeop)
        runtime->free_repl_individual_task(this);
    }

    //--------------------------------------------------------------------------
    void ReplIndividualTask::trigger_prepipeline_stage(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // We might be able to skip this if the sharding function was already
      // picked for us which occurs when we're part of a must-epoch launch
      if (sharding_function == NULL)
      {
        // Do the mapper call to get the sharding function to use
        if (mapper == NULL)
          mapper = runtime->find_mapper(current_proc, map_id); 
        Mapper::SelectShardingFunctorInput* input = repl_ctx->shard_manager;
        SelectShardingFunctorOutput output;
        mapper->invoke_task_select_sharding_functor(this, input, &output);
        if (output.chosen_functor == UINT_MAX)
          REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                        "Mapper %s failed to pick a valid sharding functor for "
                        "task %s (UID %lld)", mapper->get_mapper_name(),
                        get_task_name(), get_unique_id())
        this->sharding_functor = output.chosen_functor;
        sharding_function = 
          repl_ctx->shard_manager->find_sharding_function(sharding_functor);
      }
#ifdef DEBUG_LEGION
      assert(sharding_function != NULL);
      // In debug mode we check to make sure that all the mappers
      // picked the same sharding function
      assert(sharding_collective != NULL);
      // Contribute the result
      sharding_collective->contribute(this->sharding_functor);
      if (sharding_collective->is_target() && 
          !sharding_collective->validate(this->sharding_functor))
        REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                      "Mapper %s chose different sharding functions "
                      "for individual task %s (UID %lld) in %s "
                      "(UID %lld)", mapper->get_mapper_name(), get_task_name(), 
                      get_unique_id(), parent_ctx->get_task_name(), 
                      parent_ctx->get_unique_id())
#endif 
      // Now we can do the normal prepipeline stage
      IndividualTask::trigger_prepipeline_stage();
    }

    //--------------------------------------------------------------------------
    void ReplIndividualTask::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
      perform_base_dependence_analysis();
      RefinementTracker refinement_tracker(this, map_applied_conditions);
      ShardingFunction *analysis_sharding_function = sharding_function;
      if (must_epoch_task)
      {
        // Note we use a special 
        // projection function for must epoch launches that maps all the 
        // tasks to the special shard UINT_MAX so that they appear to be
        // on a different shard than any other tasks, but on the same shard
        // for all the tasks in the must epoch launch.
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        analysis_sharding_function = 
          repl_ctx->get_universal_sharding_function();
      }
      for (unsigned idx = 0; idx < logical_regions.size(); idx++)
      {
        RegionRequirement &req = logical_regions[idx];
        // Treat these as a special kind of projection requirement since we
        // need the logical analysis to look at sharding to determine if any
        // kind of close operations are required. 
        ProjectionInfo projection_info(runtime, req, launch_space,
            analysis_sharding_function, sharding_space);
        runtime->forest->perform_dependence_analysis(this, idx, req,
                                                     projection_info,
                                                     privilege_paths[idx],
                                                     refinement_tracker,
                                                     map_applied_conditions);
      }
    }

    //--------------------------------------------------------------------------
    void ReplIndividualTask::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(sharding_function != NULL);
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Figure out whether this shard owns this point
      if (sharding_space.exists())
      {
        Domain shard_domain;
        runtime->forest->find_launch_space_domain(sharding_space, shard_domain);
        owner_shard = sharding_function->find_owner(index_point, shard_domain);
      }
      else
        owner_shard = sharding_function->find_owner(index_point, index_domain);
      // If we're recording then record the owner shard
      if (is_recording())
      {
#ifdef DEBUG_LEGION
        assert(!is_remote());
        assert((tpl != NULL) && tpl->is_recording());
#endif
        tpl->record_owner_shard(trace_local_id, owner_shard);
      }
      if (runtime->legion_spy_enabled)
        LegionSpy::log_owner_shard(get_unique_id(), owner_shard);
      // If we own it we go on the queue, otherwise we complete early
      if (owner_shard != repl_ctx->owner_shard->shard_id)
      {
#ifdef LEGION_SPY
        // Still have to do this for legion spy
        LegionSpy::log_operation_events(unique_op_id, 
            ApEvent::NO_AP_EVENT, ApEvent::NO_AP_EVENT);
#endif
        shard_off(RtEvent::NO_RT_EVENT);
      }
      else // We own it, so it goes on the ready queue
      {
        // Don't signal the tree yet, we need to wait to see how big
        // the result future size is first
        // Then we can do the normal analysis
        IndividualTask::trigger_ready();
      }
    }

    //--------------------------------------------------------------------------
    void ReplIndividualTask::trigger_replay(void)
    //--------------------------------------------------------------------------
    {
      // Figure out if we're the one to do the replay
#ifdef DEBUG_LEGION
      assert(!is_remote());
      assert(tpl != NULL);
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      owner_shard = tpl->find_owner_shard(trace_local_id);
      if (runtime->legion_spy_enabled)
        LegionSpy::log_owner_shard(get_unique_id(), owner_shard);
      if (owner_shard != repl_ctx->owner_shard->shard_id)
      {
        if (runtime->legion_spy_enabled)
        {
          for (unsigned idx = 0; idx < regions.size(); idx++)
            TaskOp::log_requirement(unique_op_id, idx, regions[idx]);
        }
#ifdef LEGION_SPY
        LegionSpy::log_replay_operation(unique_op_id);
#endif
        shard_off(RtEvent::NO_RT_EVENT);
      }
      else
        IndividualTask::trigger_replay();
    }

    //--------------------------------------------------------------------------
    void ReplIndividualTask::shard_off(RtEvent mapped_precondition)
    //--------------------------------------------------------------------------
    {
#ifdef LEGION_SPY
      // Still need this to record that this operation is done for LegionSpy
      LegionSpy::log_operation_events(unique_op_id, 
          ApEvent::NO_AP_EVENT, ApEvent::NO_AP_EVENT);
#endif
      complete_mapping(mapped_precondition);
      if ((must_epoch == NULL) && !elide_future_return &&
          ((predication_state != RESOLVE_FALSE_STATE)))
      {
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
        assert(future_collective == NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        future_collective = new FutureBroadcast(repl_ctx, 
                future_collective_id, owner_shard, result.impl);
        const RtEvent future_ready = 
          future_collective->perform_collective_wait(false/*block*/);
        // Do the stuff to record that this is mapped and executed
        complete_execution(future_ready);
      }
      else
        complete_execution();
      trigger_children_complete(ApEvent::NO_AP_EVENT);
      trigger_children_committed();
    }

    //--------------------------------------------------------------------------
    void ReplIndividualTask::prepare_map_must_epoch(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
      assert(must_epoch != NULL);
      assert(sharding_function != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      set_origin_mapped(true);
      // See if we're going to be a local point or not
      Domain shard_domain = index_domain;
      if (sharding_space.exists())
        runtime->forest->find_launch_space_domain(sharding_space, shard_domain);
      ShardID owner = sharding_function->find_owner(index_point, shard_domain);
      if (owner == repl_ctx->owner_shard->shard_id)
      {
        FutureMap map = must_epoch->get_future_map();
        result = map.impl->get_future(index_point, true/*internal only*/);
      }
    }

    //--------------------------------------------------------------------------
    void ReplIndividualTask::trigger_task_complete(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Before doing the normal thing we have to exchange broadcast/receive
      // the future result, can skip this though if we're part of a must epoch
      // We should also skip this if we were predicated false
      if ((must_epoch == NULL) && !elide_future_return &&
          (predication_state != RESOLVE_FALSE_STATE)
          && (owner_shard == repl_ctx->owner_shard->shard_id))
      {
#ifdef DEBUG_LEGION
        assert(future_collective == NULL);
#endif
        future_collective = new FutureBroadcast(repl_ctx, 
                future_collective_id, owner_shard, result.impl);
        future_collective->broadcast_future();
      }
      IndividualTask::trigger_task_complete();
    }

    //--------------------------------------------------------------------------
    void ReplIndividualTask::initialize_replication(ReplicateContext *ctx)
    //--------------------------------------------------------------------------
    {
      IndexSpace handle;
      if (index_domain.get_dim() == 0)
      {
        DomainPoint point(0);
        Domain launch_domain(point, point);
        handle = ctx->find_index_launch_space(launch_domain, get_provenance());
      }
      else
        handle = ctx->find_index_launch_space(index_domain, get_provenance());
      launch_space = runtime->forest->get_node(handle);
      if (!elide_future_return)
        future_collective_id = 
          ctx->get_next_collective_index(COLLECTIVE_LOC_1);
    }

    //--------------------------------------------------------------------------
    void ReplIndividualTask::set_sharding_function(ShardingID functor,
                                                   ShardingFunction *function)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(must_epoch != NULL);
      assert(sharding_function == NULL);
#endif
      sharding_functor = functor;
      sharding_function = function;
    }

    /////////////////////////////////////////////////////////////
    // Repl Index Task 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplIndexTask::ReplIndexTask(Runtime *rt)
      : ReplCollectiveViewCreator<IndexTask>(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplIndexTask::ReplIndexTask(const ReplIndexTask &rhs)
      : ReplCollectiveViewCreator<IndexTask>(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplIndexTask::~ReplIndexTask(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplIndexTask& ReplIndexTask::operator=(const ReplIndexTask &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::activate(void)
    //--------------------------------------------------------------------------
    {
      ReplCollectiveViewCreator<IndexTask>::activate(); 
      sharding_functor = UINT_MAX;
      sharding_function = NULL;
      serdez_redop_collective = NULL;
      all_reduce_collective = NULL;
      output_size_collective = NULL;
      collective_check_id = 0;
      slice_sharding_output = false;
      concurrent_prebar = RtBarrier::NO_RT_BARRIER;
      concurrent_postbar = RtBarrier::NO_RT_BARRIER;
      concurrent_validator = NULL;
#ifdef DEBUG_LEGION
      sharding_collective = NULL;
#endif
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      ReplCollectiveViewCreator<IndexTask>::deactivate(false/*free*/);
      if (serdez_redop_collective != NULL)
        delete serdez_redop_collective;
      if (all_reduce_collective != NULL)
        delete all_reduce_collective;
      if (output_size_collective != NULL)
        delete output_size_collective;
      if (concurrent_validator != NULL)
        delete concurrent_validator;
#ifdef DEBUG_LEGION
      if (sharding_collective != NULL)
        delete sharding_collective;
#endif
      unique_intra_space_deps.clear();
      if (freeop)
        runtime->free_repl_index_task(this);
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::prepare_map_must_epoch(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
      assert(must_epoch != NULL);
      assert(sharding_function != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      set_origin_mapped(true);
      future_map = must_epoch->get_future_map();
      const IndexSpace local_space = sharding_space.exists() ?
          sharding_function->find_shard_space(repl_ctx->owner_shard->shard_id,
              launch_space, sharding_space, get_provenance()) :
          sharding_function->find_shard_space(repl_ctx->owner_shard->shard_id,
              launch_space, launch_space->handle, get_provenance());
      // Figure out which points to enumerate
      if (local_space.exists())
      {
        Domain local_domain;
        runtime->forest->find_launch_space_domain(local_space, local_domain);
        enumerate_futures(local_domain);
      }
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::trigger_prepipeline_stage(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // We might be able to skip this if the sharding function was already
      // picked for us which occurs when we're part of a must-epoch launch
      if (sharding_function == NULL)
        select_sharding_function(repl_ctx);
#ifdef DEBUG_LEGION
      assert(sharding_function != NULL);
      assert(sharding_collective != NULL);
      sharding_collective->contribute(this->sharding_functor);
      if (sharding_collective->is_target() &&
          !sharding_collective->validate(this->sharding_functor))
        REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                      "Mapper %s chose different sharding functions "
                      "for index task %s (UID %lld) in %s (UID %lld)", 
                      mapper->get_mapper_name(), get_task_name(), 
                      get_unique_id(), parent_ctx->get_task_name(), 
                      parent_ctx->get_unique_id())
#endif 
      // Now we can do the normal prepipeline stage
      IndexTask::trigger_prepipeline_stage();
      if (!runtime->unsafe_mapper)
      {
        // Check that all the mappers agreed on the set of 
        // collective view region requirements
        if (repl_ctx->owner_shard->shard_id == 0)
        {
          Serializer rez;
          rez.serialize<size_t>(check_collective_regions.size());
          for (std::vector<unsigned>::const_iterator it = 
                check_collective_regions.begin(); it !=
                check_collective_regions.end(); it++)
            rez.serialize(*it);
          BufferBroadcast collective(collective_check_id, repl_ctx);
          collective.broadcast(const_cast<void*>(rez.get_buffer()), 
                               rez.get_used_bytes(), false/*copy*/);
        }
        else
        {
          BufferBroadcast collective(collective_check_id, 0/*owner*/, repl_ctx);
          size_t size;
          const void *buffer = collective.get_buffer(size);
          Deserializer derez(buffer, size);
          size_t num_regions;
          derez.deserialize(num_regions);
          if (num_regions != check_collective_regions.size())
            REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                "Mapper %s provided different number of logical regions to "
                "check for collective views on shards 0 and %d of task %s "
                "(UID %lld). Shard 0 provided %zd regions while Shard %d "
                "provided %zd regions. All shards must provide the same "
                "logical regions to check for the collective view creation.",
                mapper->get_mapper_name(), repl_ctx->owner_shard->shard_id,
                get_task_name(), get_unique_id(), num_regions, 
                repl_ctx->owner_shard->shard_id,
                check_collective_regions.size())
          for (unsigned idx = 0; idx < num_regions; idx++)
          {
            unsigned index;
            derez.deserialize(index);
            if (!std::binary_search(check_collective_regions.begin(),
                  check_collective_regions.end(), index))
              REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                "Mapper %s provided different logical regions to check for "
                "collective views on shards 0 and %d of task %s (UID %lld). "
                "Shard 0 provided region %d while Shard %d did not. All "
                "shards must provide the same logical regions to check for "
                "the collective view creation.",
                mapper->get_mapper_name(), repl_ctx->owner_shard->shard_id,
                get_task_name(), get_unique_id(), index,
                repl_ctx->owner_shard->shard_id)
          }
        }
      }
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::select_sharding_function(ReplicateContext *repl_ctx)
    //--------------------------------------------------------------------------
    {
      // Do the mapper call to get the sharding function to use
      if (mapper == NULL)
        mapper = runtime->find_mapper(current_proc, map_id); 
      Mapper::SelectShardingFunctorInput* input = repl_ctx->shard_manager;
      SelectShardingFunctorOutput output;
      mapper->invoke_task_select_sharding_functor(this, input, &output);
      if (output.chosen_functor == UINT_MAX)
        REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                      "Mapper %s failed to pick a valid sharding functor for "
                      "task %s (UID %lld)", mapper->get_mapper_name(),
                      get_task_name(), get_unique_id())
      this->sharding_functor = output.chosen_functor;
      sharding_function = 
        repl_ctx->shard_manager->find_sharding_function(sharding_functor);
      slice_sharding_output = output.slice_recurse;
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // If we have a future map then set the sharding function
      if ((redop == 0) && !elide_future_return && (must_epoch == NULL))
      {
#ifdef DEBUG_LEGION
        assert(future_map.impl != NULL);
        ReplFutureMapImpl *impl = 
          dynamic_cast<ReplFutureMapImpl*>(future_map.impl);
        assert(impl != NULL);
#else
        ReplFutureMapImpl *impl = 
          static_cast<ReplFutureMapImpl*>(future_map.impl);
#endif
        impl->set_sharding_function(sharding_function);
      }
      // Compute the local index space of points for this shard
      if (sharding_space.exists())
        internal_space = 
          sharding_function->find_shard_space(repl_ctx->owner_shard->shard_id,
              launch_space, sharding_space, get_provenance());
      else
        internal_space =
          sharding_function->find_shard_space(repl_ctx->owner_shard->shard_id,
              launch_space, launch_space->handle, get_provenance());
      // If we're recording then record the local_space
      if (is_recording())
      {
#ifdef DEBUG_LEGION
        assert(!is_remote());
        assert((tpl != NULL) && tpl->is_recording());
#endif
        tpl->record_local_space(trace_local_id, internal_space);
        // Record the sharding function if needed for the future map
        if (redop == 0)
          tpl->record_sharding_function(trace_local_id, sharding_function);
      }
      // Prepare any setup for performing the concurrent analysis
      if (concurrent_task)
        initialize_concurrent_analysis();
      // If it's empty we're done, otherwise we go back on the queue
      if (!internal_space.exists())
      {
        // Check to see if we still need to participate in the premap_task call
        if (must_epoch == NULL)
          premap_task();
        // Still need to participate in any collective view rendezvous
        if (!collective_view_rendezvous.empty())
          shard_off_collective_view_rendezvous(complete_preconditions);
#ifdef LEGION_SPY
        // Still have to do this for legion spy
        LegionSpy::log_operation_events(unique_op_id, 
            ApEvent::NO_AP_EVENT, ApEvent::NO_AP_EVENT);
#endif
        // We have no local points, so we can just trigger
        if (serdez_redop_fns == NULL)
        {
          if (!map_applied_conditions.empty())
            complete_mapping(Runtime::merge_events(map_applied_conditions));
          else
            complete_mapping();
        }
        if (redop > 0)
          finish_index_task_reduction();
        complete_execution(finish_index_task_complete());
        trigger_children_complete();
        trigger_children_committed();
      }
      else // We have valid points, so it goes on the ready queue
      {
        // Update the total number of points we're actually repsonsible
        // for now with this shard
        IndexSpaceNode *node = runtime->forest->get_node(internal_space);
        total_points = node->get_volume();
#ifdef DEBUG_LEGION
        assert(total_points > 0);
#endif
        if ((redop == 0) && !elide_future_return)
        {
          Domain shard_domain;
          node->get_launch_space_domain(shard_domain);
          enumerate_futures(shard_domain);
        }
        // If we still need to slice the task then we can run it 
        // through the normal path, otherwise we can simply make 
        // the slice task for these points and put it in the queue
        if (!slice_sharding_output)
        {
          if (must_epoch == NULL)
            premap_task();
          SliceTask *new_slice = this->clone_as_slice_task(internal_space,
              target_proc, false/*recurse*/, !runtime->stealing_disabled); 
          slices.push_back(new_slice);
          trigger_slices();
        }
        else
          enqueue_ready_operation();
      }
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::trigger_replay(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(tpl != NULL);
#endif
      internal_space = tpl->find_local_space(trace_local_id);
      if ((redop == 0) && !elide_future_return)
      {
        sharding_function = tpl->find_sharding_function(trace_local_id);
#ifdef DEBUG_LEGION
        assert(future_map.impl != NULL);
        ReplFutureMapImpl *impl = 
          dynamic_cast<ReplFutureMapImpl*>(future_map.impl);
        assert(impl != NULL);
#else
        ReplFutureMapImpl *impl = 
          static_cast<ReplFutureMapImpl*>(future_map.impl);
#endif
        impl->set_sharding_function(sharding_function);
      }
      // If it's empty we're done, otherwise we do the replay
      if (!internal_space.exists())
      {
        // Still have to do this for legion spy
        if (runtime->legion_spy_enabled)
        {
          for (unsigned idx = 0; idx < regions.size(); idx++)
            TaskOp::log_requirement(unique_op_id, idx, regions[idx]);
          runtime->forest->log_launch_space(launch_space->handle, unique_op_id);
        }
#ifdef LEGION_SPY
        LegionSpy::log_replay_operation(unique_op_id);
        LegionSpy::log_operation_events(unique_op_id, 
            ApEvent::NO_AP_EVENT, ApEvent::NO_AP_EVENT);
#endif
        // Still need to do any rendezvous for concurrent analysis
        if (concurrent_task)
          initialize_concurrent_analysis();
        // We have no local points, so we can just trigger
        if (serdez_redop_fns == NULL)
        {
          if (!map_applied_conditions.empty())
            complete_mapping(Runtime::merge_events(map_applied_conditions));
          else
            complete_mapping();
        }
        if (redop > 0)
        {
          std::vector<Memory> reduction_futures;
          tpl->get_premap_output(this, reduction_futures);
          create_future_instances(reduction_futures);
          finish_index_task_reduction();
        }
        complete_execution(finish_index_task_complete());
        trigger_children_complete();
        trigger_children_committed();
      }
      else
        IndexTask::trigger_replay();
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
      perform_base_dependence_analysis();
      RefinementTracker refinement_tracker(this, map_applied_conditions);
      ShardingFunction *analysis_sharding_function = sharding_function;
      if (must_epoch_task)
      {
        // Note we use a special 
        // projection function for must epoch launches that maps all the 
        // tasks to the special shard UINT_MAX so that they appear to be
        // on a different shard than any other tasks, but on the same shard
        // for all the tasks in the must epoch launch.
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        analysis_sharding_function = 
          repl_ctx->get_universal_sharding_function();
      }
      for (unsigned idx = 0; idx < logical_regions.size(); idx++)
      {
        RegionRequirement &req = logical_regions[idx];
        ProjectionInfo projection_info(runtime, req, launch_space,
            analysis_sharding_function, sharding_space);
        runtime->forest->perform_dependence_analysis(this, idx, req, 
                                                     projection_info,
                                                     privilege_paths[idx],
                                                     refinement_tracker,
                                                     map_applied_conditions);
      }
      // Generate any collective view rendezvous that we will need
      for (std::vector<unsigned>::const_iterator it =
            check_collective_regions.begin(); it != 
            check_collective_regions.end(); it++)
        create_collective_view_rendezvous(
            logical_regions[*it].parent.get_tree_id(), *it);
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::create_future_instances(
                                           std::vector<Memory> &target_memories)
    //--------------------------------------------------------------------------
    {
      // Do the base call first
      IndexTask::create_future_instances(target_memories);
      // Now check to see if we need to make a shadow instance for our
      // future all reduce collective
      if (all_reduce_collective != NULL)
      {
#ifdef DEBUG_LEGION
        assert(!reduction_instances.empty());
        assert(reduction_instance != NULL);
#endif
        // If the instance is in a memory we cannot see or is "too big"
        // then we need to make the shadow instance for the future
        // all-reduce collective to use now while still in the mapping stage
        if ((!reduction_instance->is_meta_visible) ||
            (reduction_instance->size > LEGION_MAX_RETURN_SIZE))
        {
          MemoryManager *manager = 
            runtime->find_memory_manager(reduction_instance->memory);
          FutureInstance *shadow_instance = 
            manager->create_future_instance(this, unique_op_id,
                ApEvent::NO_AP_EVENT, reduction_op->sizeof_rhs, false/*eager*/);
          all_reduce_collective->set_shadow_instance(shadow_instance);
        }
      }
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::finish_index_task_reduction(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(redop != 0);
#endif
      // Set the future if we actually ran the task or we speculated
      if (predication_state == RESOLVE_FALSE_STATE)
        return;
      if (serdez_redop_fns != NULL)
      {
#ifdef DEBUG_LEGION
        assert(serdez_redop_collective != NULL);
#endif
        const std::map<ShardID,std::pair<void*,size_t> > &remote_buffers =
          serdez_redop_collective->exchange_buffers(serdez_redop_state, 
                          serdez_redop_state_size, deterministic_redop);
        if (deterministic_redop)
        {
          // Reset this back to empty so we can reduce in order across shards
          // Note the serdez_redop_collective took ownership of deleting
          // the buffer in this case so we know that it is not leaking
          serdez_redop_state = NULL;
          for (std::map<ShardID,std::pair<void*,size_t> >::const_iterator it =
                remote_buffers.begin(); it != remote_buffers.end(); it++)
          {
            if (serdez_redop_state == NULL)
            {
              serdez_redop_state_size = it->second.second;
              serdez_redop_state = malloc(serdez_redop_state_size);
              memcpy(serdez_redop_state, it->second.first, 
                     serdez_redop_state_size);
            }
            else
              (*(serdez_redop_fns->fold_fn))(reduction_op, serdez_redop_state,
                                    serdez_redop_state_size, it->second.first);
          }
        }
        else
        {
          for (std::map<ShardID,std::pair<void*,size_t> >::const_iterator it =
                remote_buffers.begin(); it != remote_buffers.end(); it++)
          {
#ifdef DEBUG_LEGION
            assert(it->first != serdez_redop_collective->local_shard);
#endif
            (*(serdez_redop_fns->fold_fn))(reduction_op, serdez_redop_state,
                                  serdez_redop_state_size, it->second.first);
          }
        }
      }
      else
      {
#ifdef DEBUG_LEGION
        assert(all_reduce_collective != NULL);
        assert(!reduction_instances.empty());
        assert(reduction_instance == reduction_instances.front());
#endif
        ApEvent local_precondition;
        if (!reduction_effects.empty())
        {
          local_precondition = Runtime::merge_events(NULL, reduction_effects);
          reduction_effects.clear();
        }
        const RtEvent collective_done = all_reduce_collective->async_reduce(
                                    reduction_instance, local_precondition);
        if (local_precondition.exists())
          reduction_effects.push_back(local_precondition);
        // No need to do anything with the output local precondition
        // We already added it to the complete_effects when we made
        // the collective at the beginning
        if (collective_done.exists())
          complete_preconditions.insert(collective_done);
      }
      // Now call the base version of this to finish making
      // the instances for the future results
      IndexTask::finish_index_task_reduction();
    }

    //--------------------------------------------------------------------------
    RtEvent ReplIndexTask::finish_index_task_complete(void)
    //--------------------------------------------------------------------------
    {
      if ((output_size_collective != NULL) &&
          (predication_state != RESOLVE_FALSE_STATE))
      {
        // Make a copy of the output sizes before we perform all-gather
        local_output_sizes = all_output_sizes;
        // We need to gather output region sizes from all the other shards
        // to determine the sizes of globally indexed output regions
        return output_size_collective->exchange_output_sizes();
      }
      return RtEvent::NO_RT_EVENT;
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::resolve_false(bool speculated, bool launched)
    //--------------------------------------------------------------------------
    {
      // If we already launched then we can just return
      if (launched)
        return;
      // Otherwise, we need to update the internal space so we only set
      // our local points with the predicate false result
      if (redop == 0)
      {
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
#ifdef DEBUG_LEGION
        assert(sharding_function != NULL);
        assert(future_map.impl != NULL);
        ReplFutureMapImpl *impl =
          dynamic_cast<ReplFutureMapImpl*>(future_map.impl);
        assert(impl != NULL);
#else
        ReplFutureMapImpl *impl =
          static_cast<ReplFutureMapImpl*>(future_map.impl);
#endif
        impl->set_sharding_function(sharding_function);
        // Compute the local index space of points for this shard
        if (sharding_space.exists())
          internal_space = 
            sharding_function->find_shard_space(repl_ctx->owner_shard->shard_id,
                launch_space, sharding_space, get_provenance());
        else
          internal_space =
            sharding_function->find_shard_space(repl_ctx->owner_shard->shard_id,
                launch_space, launch_space->handle, get_provenance());
      }
#ifdef DEBUG_LEGION
      if (output_size_collective != NULL)
        output_size_collective->elide_collective();
#endif
      // Now continue through and do the base case
      IndexTask::resolve_false(speculated, launched);
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::initialize_replication(ReplicateContext *ctx)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(serdez_redop_collective == NULL);
      assert(all_reduce_collective == NULL);
#endif
      // If we have a reduction op then we need an exchange
      if (redop > 0)
      {
        if (serdez_redop_fns == NULL)
          all_reduce_collective = new FutureAllReduceCollective(this,
           COLLECTIVE_LOC_53, ctx, redop, reduction_op, deterministic_redop);
        else
          serdez_redop_collective = new BufferExchange(ctx, COLLECTIVE_LOC_53);
      }
      bool has_output_region = false;
      for (unsigned idx = 0; idx < output_regions.size(); ++idx)
        if (!output_region_options[idx].valid_requirement())
        {
          has_output_region = true;
          break;
        }
      if (has_output_region)
        output_size_collective =
          new OutputSizeExchange(ctx, COLLECTIVE_LOC_29, all_output_sizes);
      if (!runtime->unsafe_mapper)
        collective_check_id = ctx->get_next_collective_index(COLLECTIVE_LOC_29);
      if (concurrent_task)
      {
        concurrent_prebar = ctx->get_next_concurrent_precondition_barrier();
        concurrent_postbar = ctx->get_next_concurrent_postcondition_barrier();
        if (!runtime->unsafe_mapper)
          concurrent_validator = new ConcurrentExecutionValidator(this,
              COLLECTIVE_LOC_104, ctx, 0/*owner shard*/);
      }
    } 

    //--------------------------------------------------------------------------
    void ReplIndexTask::set_sharding_function(ShardingID functor,
                                              ShardingFunction *function)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(must_epoch != NULL);
      assert(sharding_function == NULL); 
#endif
      sharding_functor = functor;
      sharding_function = function;
    }

    //--------------------------------------------------------------------------
    FutureMapImpl* ReplIndexTask::create_future_map(TaskContext *ctx,
                                IndexSpace launch_space, IndexSpace shard_space)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(ctx);
#endif
      IndexSpaceNode *launch_node = runtime->forest->get_node(launch_space);
      IndexSpaceNode *shard_node = 
        ((launch_space == shard_space) || !shard_space.exists()) ?
        launch_node : runtime->forest->get_node(shard_space);
      // Make a replicate future map 
      return new ReplFutureMapImpl(repl_ctx, this, launch_node, shard_node,
          runtime, runtime->get_available_distributed_id(),
          runtime->address_space, get_provenance());
    } 

    //--------------------------------------------------------------------------
    void ReplIndexTask::initialize_concurrent_analysis(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // See if we are the first local shard on the lowest address space
      const CollectiveMapping &mapping = 
        repl_ctx->shard_manager->get_collective_mapping();
      const AddressSpace lowest = mapping[0];
      if ((lowest == runtime->address_space) && 
          repl_ctx->shard_manager->is_first_local_shard(repl_ctx->owner_shard))
      {
        Runtime::phase_barrier_arrive(concurrent_prebar, 1/*arrivals*/,
          runtime->acquire_concurrent_reservation(concurrent_postbar));
      }
      concurrent_precondition = concurrent_prebar;
      Runtime::phase_barrier_arrive(concurrent_postbar, 
          1/*arrivals*/, mapped_event);
      // If we are doing concurrent validation and we don't have any local
      // points then we need to kick that off now. Save an event to make
      // sure we don't delete the collective until we are done running
      if ((concurrent_validator != NULL) && !internal_space.exists())
      {
        map_applied_conditions.insert(concurrent_validator->get_done_event());
        concurrent_validator->perform_validation(concurrent_processors);
      }
    }

    //--------------------------------------------------------------------------
    RtEvent ReplIndexTask::verify_concurrent_execution(const DomainPoint &point,
                                                       Processor target)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(concurrent_task);
      assert(concurrent_validator != NULL);
#endif
      bool done = false;
      {
        AutoLock o_lock(op_lock);
#ifdef DEBUG_LEGION
        assert(concurrent_processors.find(point) == 
                concurrent_processors.end());
        assert(concurrent_processors.size() < total_points);
#endif
        concurrent_processors[point] = target;
        done = (concurrent_processors.size() == total_points);
      }
      const RtEvent result = concurrent_validator->get_done_event();
      if (done)
        concurrent_validator->perform_validation(concurrent_processors);
      return result;
    }

    //--------------------------------------------------------------------------
    RtEvent ReplIndexTask::find_intra_space_dependence(const DomainPoint &point)
    //--------------------------------------------------------------------------
    {
      AutoLock o_lock(op_lock);
      // Check to see if we already have it
      std::map<DomainPoint,RtEvent>::const_iterator finder = 
        intra_space_dependences.find(point);
      if (finder != intra_space_dependences.end())
        return finder->second;  
      // Make a temporary event and then do different things depending on 
      // whether we own this point or whether a remote shard owns it
      const RtUserEvent pending_event = Runtime::create_rt_user_event();
      intra_space_dependences[point] = pending_event;
      // If not, check to see if this is a point that we expect to own
#ifdef DEBUG_LEGION
      assert(sharding_function != NULL);
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      Domain launch_domain;
      if (sharding_space.exists())
        runtime->forest->find_launch_space_domain(sharding_space,launch_domain);
      else
        launch_space->get_launch_space_domain(launch_domain);
      const ShardID point_shard = 
        sharding_function->find_owner(point, launch_domain); 
      if (point_shard != repl_ctx->owner_shard->shard_id)
      {
        // A different shard owns it so send a message to that shard 
        // requesting it to fill in the dependence
        Serializer rez;
        rez.serialize(repl_ctx->shard_manager->repl_id);
        rez.serialize(point_shard);
        rez.serialize(context_index);
        rez.serialize(point);
        rez.serialize(pending_event);
        rez.serialize(repl_ctx->owner_shard->shard_id);
        repl_ctx->shard_manager->send_intra_space_dependence(point_shard, rez);
      }
      else // We own it so do the normal thing
        pending_intra_space_dependences[point] = pending_event;
      return pending_event; 
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::record_intra_space_dependence(const DomainPoint &point,
                                  const DomainPoint &next, RtEvent point_mapped)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(sharding_function != NULL);
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Determine if the next point is one that we own or is one that is
      // going to be coming from a remote shard
      Domain launch_domain;
      if (sharding_space.exists())
        runtime->forest->find_launch_space_domain(sharding_space,launch_domain);
      else
        launch_space->get_launch_space_domain(launch_domain);
      const ShardID next_shard = 
        sharding_function->find_owner(next, launch_domain); 
      if (next_shard != repl_ctx->owner_shard->shard_id)
      {
        // Make sure we only send this to the repl_ctx once for each 
        // unique shard ID that we see for this point task
        const std::pair<DomainPoint,ShardID> key(point, next_shard); 
        bool record_dependence = true;
        {
          AutoLock o_lock(op_lock);
          std::set<std::pair<DomainPoint,ShardID> >::const_iterator finder = 
            unique_intra_space_deps.find(key);
          if (finder != unique_intra_space_deps.end())
            record_dependence = false;
          else
            unique_intra_space_deps.insert(key);
        }
        if (record_dependence)
          repl_ctx->record_intra_space_dependence(context_index, point, 
                                                  point_mapped, next_shard);
      }
      else // The next shard is ourself, so we can do the normal thing
        IndexTask::record_intra_space_dependence(point, next, point_mapped);
    }

    //--------------------------------------------------------------------------
    void ReplIndexTask::finalize_output_regions(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      if (!repl_ctx->shard_manager->is_first_local_shard(repl_ctx->owner_shard))
        return;
      RegionTreeForest *forest = runtime->forest;
      const CollectiveMapping &mapping =
        repl_ctx->shard_manager->get_collective_mapping();

      for (unsigned idx = 0; idx < output_regions.size(); ++idx)
      {
        const OutputOptions &options = output_region_options[idx];
        if (options.valid_requirement())
          continue;
        IndexSpaceNode *parent = forest->get_node(
            output_regions[idx].parent.get_index_space());
#ifdef DEBUG_LEGION
        validate_output_sizes(idx, output_regions[idx], all_output_sizes[idx]);
#endif
        if (options.global_indexing())
        {
          // For globally indexed output regions, we need to check
          // the alignment between outputs from adjacent point tasks
          // and compute the ranges of subregions via prefix sum.
          IndexPartNode *part = runtime->forest->get_node(
            output_regions[idx].partition.get_index_partition());
          Domain root_domain = compute_global_output_ranges(
            parent, part, all_output_sizes[idx], local_output_sizes[idx]);

          log_index.debug()
            << "[Task " << get_task_name() << "(UID: " << get_unique_op_id()
            << ")] setting " << root_domain << " to index space " << std::hex
            << parent->handle.get_id();

          if (parent->set_domain(root_domain, runtime->address_space, &mapping))
            delete parent;
        }
        // For locally indexed output regions, sizes of subregions are already
        // set when they are fianlized by the point tasks. So we only need to
        // initialize the root index space by taking a union of subspaces.
        else if (parent->set_output_union(all_output_sizes[idx],
                              runtime->address_space, &mapping))
          delete parent;
      }
    }

    //--------------------------------------------------------------------------
    size_t ReplIndexTask::get_collective_points(void) const
    //--------------------------------------------------------------------------
    {
      return runtime->forest->get_node(internal_space)->get_volume();
    }

    //--------------------------------------------------------------------------
    bool ReplIndexTask::find_shard_participants(std::vector<ShardID> &shards)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(sharding_function != NULL);
#endif
      if (sharding_space.exists())
        return sharding_function->find_shard_participants(launch_space,
                                                sharding_space, shards);
      else
        return sharding_function->find_shard_participants(launch_space,
                                          launch_space->handle, shards);
    }

    /////////////////////////////////////////////////////////////
    // Repl Merge Close Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplMergeCloseOp::ReplMergeCloseOp(Runtime *rt)
      : MergeCloseOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplMergeCloseOp::ReplMergeCloseOp(const ReplMergeCloseOp &rhs)
      : MergeCloseOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplMergeCloseOp::~ReplMergeCloseOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplMergeCloseOp& ReplMergeCloseOp::operator=(const ReplMergeCloseOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplMergeCloseOp::activate(void)
    //--------------------------------------------------------------------------
    {
      MergeCloseOp::activate();
      mapped_barrier = RtBarrier::NO_RT_BARRIER;
      refinement_barrier = RtBarrier::NO_RT_BARRIER;
      did_collective = NULL;
    }

    //--------------------------------------------------------------------------
    void ReplMergeCloseOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      MergeCloseOp::deactivate(false/*free*/);
      if (did_collective != NULL)
        delete did_collective;
      if (freeop)
        runtime->free_repl_merge_close_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplMergeCloseOp::set_repl_close_info(RtBarrier mapped)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!mapped_barrier.exists());
#endif
      mapped_barrier = mapped;
    }

    //--------------------------------------------------------------------------
    void ReplMergeCloseOp::record_refinements(const FieldMask &refinement_mask,
                                              const bool overwrite)
    //--------------------------------------------------------------------------
    {
      // Call the base version of this
      MergeCloseOp::record_refinements(refinement_mask, overwrite);
      // Get a barrier for a refinement invalidation
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = 
        dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      refinement_barrier = repl_ctx->get_next_refinement_barrier();
    }

    //--------------------------------------------------------------------------
    void ReplMergeCloseOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(mapped_barrier.exists());
#endif
      if (!!refinement_mask)
      {
#ifdef DEBUG_LEGION
        assert(did_collective == NULL);
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        const ShardID origin = repl_ctx->get_next_equivalence_set_origin();
        const CollectiveID collective_id = 
         repl_ctx->get_next_collective_index(COLLECTIVE_LOC_20,true/*logical*/);
        did_collective =
          new ValueBroadcast<DistributedID>(collective_id, repl_ctx, origin);
        if (did_collective->is_origin())
        {
          const DistributedID did = runtime->get_available_distributed_id();
          did_collective->broadcast(did);
        }
      }
    }

    //--------------------------------------------------------------------------
    void ReplMergeCloseOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
      std::set<RtEvent> ready_events;
      if (!!refinement_mask && !refinement_overwrite)
      {
        const ContextID ctx = parent_ctx->get_context().get_id();
        RegionNode *region_node = runtime->forest->get_node(requirement.region);
        region_node->perform_versioning_analysis(ctx, parent_ctx, &version_info,
           refinement_mask, unique_op_id, runtime->address_space, ready_events);
#ifdef DEBUG_LEGION
        assert(refinement_barrier.exists());
#endif
        if (!ready_events.empty())
        {
          // Make sure that everyone is done computing their previous
          // equivalence sets before we allow anyone to do any invalidations
          Runtime::phase_barrier_arrive(refinement_barrier, 1/*count*/,
              Runtime::merge_events(ready_events));
          ready_events.clear();
        }
        else
          Runtime::phase_barrier_arrive(refinement_barrier, 1/*count*/);
        ready_events.insert(refinement_barrier);
      }
      else if (refinement_barrier.exists())
      {
        Runtime::phase_barrier_arrive(refinement_barrier, 1/*count*/);
        ready_events.insert(refinement_barrier);
      }
      if ((did_collective != NULL) && !did_collective->is_origin())
      {
        const RtEvent ready = 
          did_collective->perform_collective_wait(false/*block*/);
        if (ready.exists() && !ready.has_triggered())
          ready_events.insert(ready);
      }
      if (!ready_events.empty())
        enqueue_ready_operation(Runtime::merge_events(ready_events));
      else
        enqueue_ready_operation();
    }

    //--------------------------------------------------------------------------
    void ReplMergeCloseOp::trigger_mapping(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(mapped_barrier.exists());
#endif
      if (!!refinement_mask)
      {
#ifdef DEBUG_LEGION
        assert(requirement.handle_type == LEGION_SINGULAR_PROJECTION);
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
        assert(did_collective != NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        std::set<RtEvent> map_applied_conditions;
        InnerContext *context = find_physical_context(0/*index*/);
        const ContextID ctx = repl_ctx->get_context().get_id();
        RegionNode *region_node = runtime->forest->get_node(requirement.region); 
#ifdef DEBUG_LEGION
        assert(refinement_barrier.exists());
#endif
        // Make a new equivalence set and record it at this node
        bool first = false;
        const DistributedID did = did_collective->get_value(false/*block*/);
        EquivalenceSet *set = 
          repl_ctx->shard_manager->deduplicate_equivalence_set_creation(
                                        region_node, context, did, first);
        // Merge the state from the old equivalence sets if not overwriting
        if (first && !refinement_overwrite)
        {
          const FieldMaskSet<EquivalenceSet> &previous_sets = 
            version_info.get_equivalence_sets();
          for (FieldMaskSet<EquivalenceSet>::const_iterator it =
                previous_sets.begin(); it != previous_sets.end(); it++)
            set->clone_from(runtime->address_space, it->first, it->second,
                        false/*forward to owner*/, map_applied_conditions, 
                        false/*invalidate overlap*/);
        }
        // Invalidate the old refinement
        region_node->invalidate_refinement(ctx, refinement_mask,
            false/*self*/, *repl_ctx, map_applied_conditions, to_release);
        // Register this refinement in the tree 
        region_node->record_refinement(ctx, set, refinement_mask, 
                                       map_applied_conditions);
        // Remove the CONTEXT_REF on the set now that it is registered
        if (set->remove_base_valid_ref(CONTEXT_REF))
          assert(false); // should never actually hit this
        if (!map_applied_conditions.empty())
          Runtime::phase_barrier_arrive(mapped_barrier, 1/*count*/,
              Runtime::merge_events(map_applied_conditions));
        else
          Runtime::phase_barrier_arrive(mapped_barrier, 1/*count*/);
      }
      else // Arrive on our barrier
        Runtime::phase_barrier_arrive(mapped_barrier, 1/*count*/);
      // Then complete the mapping once the barrier has triggered
      complete_mapping(mapped_barrier);
      complete_execution();
    }

    /////////////////////////////////////////////////////////////
    // Repl Refinement Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplRefinementOp::ReplRefinementOp(Runtime *rt)
      : RefinementOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplRefinementOp::ReplRefinementOp(const ReplRefinementOp&rhs)
      : RefinementOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplRefinementOp::~ReplRefinementOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplRefinementOp& ReplRefinementOp::operator=(const ReplRefinementOp&rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplRefinementOp::activate(void)
    //--------------------------------------------------------------------------
    {
      RefinementOp::activate();
      mapped_barrier = RtBarrier::NO_RT_BARRIER;
      refinement_barrier = RtBarrier::NO_RT_BARRIER;
    }

    //--------------------------------------------------------------------------
    void ReplRefinementOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      RefinementOp::deactivate(false/*free*/);
      if (!collective_dids.empty())
      {
        for (std::vector<ValueBroadcast<DistributedID>*>::const_iterator it =
              collective_dids.begin(); it != collective_dids.end(); it++)
          delete (*it);
        collective_dids.clear();
      }
      replicated_regions.clear();
      replicated_partitions.clear();
      sharded_region_version_infos.clear();
      sharded_regions.clear();
      sharded_partitions.clear();
      if (freeop)
        runtime->free_repl_refinement_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplRefinementOp::set_repl_refinement_info(RtBarrier mapped_bar,
                                                    RtBarrier refinement_bar)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!mapped_barrier.exists());
      assert(!refinement_barrier.exists());
#endif
      mapped_barrier = mapped_bar;
      refinement_barrier = refinement_bar;
    }

    //--------------------------------------------------------------------------
    void ReplRefinementOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(mapped_barrier.exists());
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Iterate through each of the partitions and see if we are going to
      // shard them or not when making equivalence sets, we always duplicate
      // the creation of equivalence sets for intermediate regions
      // Anything that is projected and therefore sharded is not duplicated
      size_t total_replicate_subregions = 0;
      for (FieldMaskSet<RegionTreeNode>::const_iterator it =
            make_from.begin(); it != make_from.end(); it++)
      {
        // Check to see first if we are not projecting for any fields
        if (!projections.empty() && 
            (projections.find(it->first) != projections.end()) &&
            (it->second == projections[it->first].get_valid_mask()))
          continue;
        if (!it->first->is_region())
        {
          PartitionNode *part_node = it->first->as_partition_node();
          if (repl_ctx->replicate_partition_equivalence_sets(part_node))
          {
            replicated_partitions[part_node->handle] = part_node;
            total_replicate_subregions += part_node->get_num_children();
          }
        }
        else
        {
          RegionNode *region = it->first->as_region_node();
          replicated_regions[region->handle] = region;
          total_replicate_subregions++;
        }
      }
      if (total_replicate_subregions > 0)
      {
        // Create collective dids for all subregions of the replicate partitions
        collective_dids.resize(total_replicate_subregions);
        for (unsigned idx = 0; idx < total_replicate_subregions; idx++)
        {
          const ShardID origin = repl_ctx->get_next_equivalence_set_origin();
          const CollectiveID collective_id = 
            repl_ctx->get_next_collective_index(COLLECTIVE_LOC_21,
                                                true/*logical*/);
          collective_dids[idx] =
            new ValueBroadcast<DistributedID>(collective_id, repl_ctx, origin);
          if (collective_dids[idx]->is_origin())
          {
            const DistributedID did = runtime->get_available_distributed_id();
            collective_dids[idx]->broadcast(did);
          }
        }
      }
      // Do the base call at this point
      RefinementOp::trigger_dependence_analysis();
    }

    //--------------------------------------------------------------------------
    void ReplRefinementOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
      std::set<RtEvent> ready_events;      
      const ContextID ctx = parent_ctx->get_context().get_id();
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      FieldMask replicated_mask;
      Provenance *provenance = get_provenance();
      const AddressSpaceID local_space = runtime->address_space;
      // Fill in the sharded_regions and sharded_partitions
      // data structures, we'll use those to compute the equivalence sets
      for (FieldMaskSet<RegionTreeNode>::const_iterator it =
            make_from.begin(); it != make_from.end(); it++)
      {
        FieldMask version_mask = it->second;
        // Check to see if any fields are projected, if so then we only
        // need to compute equivalence sets for the projected regions
        LegionMap<RegionTreeNode*,
          FieldMaskSet<RefProjectionSummary> >::const_iterator 
            finder = projections.find(it->first);
        if (finder != projections.end())
        {
          for (FieldMaskSet<RefProjectionSummary>::const_iterator sit =
                finder->second.begin(); sit != finder->second.end(); sit++)
          {
            std::vector<RegionNode*> regions;
            sit->first->project_refinement(it->first, 
                repl_ctx->owner_shard->shard_id, regions, provenance);
            for (std::vector<RegionNode*>::const_iterator rit =
                  regions.begin(); rit != regions.end(); rit++)
            {
              PartitionNode *parent = (*rit)->parent;
              std::vector<RegionNode*> &children = sharded_regions[parent];
              if (children.size() < parent->get_num_children())
              {
                bool found = false;
                for (unsigned idx = 0; idx < children.size(); idx++)
                {
                  if ((*rit) != children[idx])
                    continue;
                  found = true;
                  break;
                }
                if (!found)
                  children.push_back(*rit);
              }
              sharded_partitions.insert(parent, sit->second);
            }
          }
          version_mask -= finder->second.get_valid_mask();
          if (!version_mask)
            continue;
        }
        if (!it->first->is_region() &&
            (replicated_partitions.find(it->first->as_partition_node()->handle) 
             == replicated_partitions.end()))
        {
          // Only compute equivalence sets for the subregions that
          // are sharded to this particular shard
          PartitionNode *part_node = it->first->as_partition_node();
          IndexPartNode *index_part = part_node->row_source;
          std::vector<RegionNode*> &children = sharded_regions[part_node];
          sharded_partitions.insert(part_node, version_mask);
          // This is probably too conservative a check, but it is sound
          if (children.size() < index_part->get_num_children())
          {
            const size_t max_check = children.size();
            if (index_part->total_children == index_part->max_linearized_color)
            {
              for (LegionColor color = repl_ctx->owner_shard->shard_id; 
                    color < index_part->total_children; 
                    color += repl_ctx->total_shards)
              {
                RegionNode *child = part_node->get_child(color);
                bool found = false;
                for (unsigned idx = 0; idx < max_check; idx++)
                {
                  if (children[idx] != child)
                      continue;
                  found = true;
                  break;
                }
                if (!found)
                  children.push_back(child);
              }
            }
            else
            {
              ColorSpaceIterator *itr = 
                index_part->color_space->create_color_space_iterator();
              // Skip ahead for our shard
              for (unsigned idx = 0; 
                    idx < repl_ctx->owner_shard->shard_id; idx++)
              {
                itr->yield_color();
                if (!itr->is_valid())
                  break;
              }
              while (itr->is_valid())
              {
                RegionNode *child = part_node->get_child(itr->yield_color());
                bool found = false;
                for (unsigned idx = 0; idx < max_check; idx++)
                {
                  if (children[idx] != child)
                    continue;
                  found = true;
                  break;
                }
                if (!found)
                  children.push_back(child);
                // Skip ahead to the next color
                for (unsigned idx = 0; idx < (repl_ctx->total_shards-1); idx++)
                {
                  itr->yield_color();
                  if (!itr->is_valid())
                    break;
                }
              }
              delete itr;
            }
          }
        }
        else // we can compute versions from the root to_refine
          replicated_mask |= version_mask;
      }
      // At this point we know which regions we need equivalence sets for
      // Start with the root ones which we'll put in the normal version_info
      if (!!replicated_mask)
      {
        if (!!uninitialized_fields)
          replicated_mask -= uninitialized_fields;
        if (!!replicated_mask)
          to_refine->perform_versioning_analysis(ctx, parent_ctx, &version_info,
                      replicated_mask, unique_op_id, local_space, ready_events);
      }
      // Now compute the shard specific ones
      for (FieldMaskSet<PartitionNode>::const_iterator pit =
            sharded_partitions.begin(); pit != sharded_partitions.end(); pit++)
      {
        const std::vector<RegionNode*> &children = sharded_regions[pit->first];
        if (!!uninitialized_fields)
        {
          const FieldMask request_mask = pit->second - uninitialized_fields;
          for (std::vector<RegionNode*>::const_iterator it =
                children.begin(); it != children.end(); it++)
          {
            VersionInfo &region_info = sharded_region_version_infos[*it];
            if (!!request_mask)
              (*it)->perform_versioning_analysis(ctx, parent_ctx, &region_info,
                  request_mask, unique_op_id, local_space, ready_events);
          }
        }
        else
        {
          for (std::vector<RegionNode*>::const_iterator it =
                children.begin(); it != children.end(); it++)
          {
            VersionInfo &region_info = sharded_region_version_infos[*it];
            (*it)->perform_versioning_analysis(ctx, parent_ctx, &region_info,
                pit->second, unique_op_id, local_space, ready_events);
          }
        }
      }
#ifdef DEBUG_LEGION
      assert(refinement_barrier.exists());
#endif
      // Make sure that everyone is done computing their equivalence sets
      // from the previous set before we allow anyone to do any invalidations
      if (!ready_events.empty())
      {
        Runtime::phase_barrier_arrive(refinement_barrier, 1/*count*/,
            Runtime::merge_events(ready_events));
        ready_events.clear();
      }
      else
        Runtime::phase_barrier_arrive(refinement_barrier, 1/*count*/);
      ready_events.insert(refinement_barrier);
      if (!collective_dids.empty())
      {
        for (std::vector<ValueBroadcast<DistributedID>*>::const_iterator it =
              collective_dids.begin(); it != collective_dids.end(); it++)
        {
          if ((*it)->is_origin())
            continue;
          RtEvent ready_event = (*it)->perform_collective_wait(false/*block*/);
          if (ready_event.exists() && !ready_event.has_triggered())
            ready_events.insert(ready_event);
        }
      }
      if (!ready_events.empty())
        enqueue_ready_operation(Runtime::merge_events(ready_events));
      else
        enqueue_ready_operation();
    }

    //--------------------------------------------------------------------------
    void ReplRefinementOp::trigger_mapping(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(mapped_barrier.exists());
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
      assert(replicated_partitions.size() <= make_from.size());
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      InnerContext *context = find_physical_context(0/*index*/);
      std::set<RtEvent> map_applied_conditions;
      // First we go through and make the pending refinements for any regions
      // which are sharded to us so that we can add valid references before we
      // invalidate the old refinements
      if (!sharded_region_version_infos.empty())
      {
        for (FieldMaskSet<PartitionNode>::const_iterator pit =
             sharded_partitions.begin(); pit != sharded_partitions.end(); pit++)
        {
          const std::vector<RegionNode*> &children = 
            sharded_regions[pit->first];
          for (std::vector<RegionNode*>::const_iterator rit =
                children.begin(); rit != children.end(); rit++)
          {
            LegionMap<RegionNode*,VersionInfo>::iterator finder =
              sharded_region_version_infos.find(*rit);
#ifdef DEBUG_LEGION
            assert(finder != sharded_region_version_infos.end());
#endif
            PendingEquivalenceSet *pending =
              new PendingEquivalenceSet(*rit, context);
            pending->record_all(finder->second, map_applied_conditions);
            // Context takes ownership at this point
            context->record_pending_disjoint_complete_set(pending, 
                                                          pit->second);
          }
        }
      }
      // Now go through and invalidate the current refinements for
      // the regions that we are updating
      const ContextID ctx = repl_ctx->get_context().get_id();
      if (!!uninitialized_fields)
      {
        const FieldMask invalidate_mask = 
          get_internal_mask() - uninitialized_fields;
        if (!!invalidate_mask)
          to_refine->invalidate_refinement(ctx, invalidate_mask, false/*self*/,
                                *repl_ctx, map_applied_conditions, to_release);
      }
      else
        to_refine->invalidate_refinement(ctx, get_internal_mask(),
            false/*self*/, *repl_ctx, map_applied_conditions, to_release);
      // First propagate the refinements for the sharded regions and partitions
      for (FieldMaskSet<PartitionNode>::const_iterator it =
            sharded_partitions.begin(); it != sharded_partitions.end(); it++)
      {
        const std::vector<RegionNode*> &children = sharded_regions[it->first];
        if (children.empty())
        {
          // Still propagate the refinement so we can do lookups
          // correctly for control replication
          it->first->propagate_refinement(ctx, NULL/*no child*/, it->second,
                                          map_applied_conditions);
          continue;
        }
        // We're not actually going to make the equivalence sets here
        // Instead we're going to just fill in the right data structure
        // on the partition so that any traversals of the children
        // will ping the context to figure out who the owner is. The
        // actual owner of the initial equivalence set will be done
        // with a first touch policy so that the first writer will
        // the one to make the equivalence sets
        it->first->propagate_refinement(ctx, children, it->second, 
                                        map_applied_conditions);
      }
      // Now we do the replicated partitions and regions
      if (!replicated_partitions.empty() || !replicated_regions.empty())
      {
        unsigned did_index = 0;
        // Now make the replicated partitions
        for (std::map<LogicalPartition,PartitionNode*>::const_iterator it =
              replicated_partitions.begin(); it != 
              replicated_partitions.end(); it++)
        {
#ifdef DEBUG_LEGION
          assert(did_index < collective_dids.size());
#endif
          IndexPartNode *index_part = it->second->row_source;
          FieldMask mask = make_from[it->second];
          // Prune out any projection fields for this node
          LegionMap<RegionTreeNode*,
            FieldMaskSet<RefProjectionSummary> >::const_iterator
              finder = projections.find(it->second);
          if (finder != projections.end())
            mask -= finder->second.get_valid_mask();
#ifdef DEBUG_LEGION
          assert(!!mask);
#endif
          // Iterate over each child and make an equivalence set  
          if (index_part->total_children == index_part->max_linearized_color)
          {
            for (LegionColor color = 0; 
                  color < index_part->total_children; color++)
            {
              RegionNode *child = it->second->get_child(color);
              bool first = false;
              const DistributedID did = 
                collective_dids[did_index++]->get_value(false/*block*/);
              EquivalenceSet *set = 
                repl_ctx->shard_manager->deduplicate_equivalence_set_creation(
                                                    child, context, did, first);
              // If we're the first shard of the owner we initialize the state
              if (first && set->is_owner())
                initialize_replicated_set(set, mask, map_applied_conditions);
              child->record_refinement(ctx, set, mask, map_applied_conditions);
              // Remove the CONTEXT_REF on the set now that it is registered
              if (set->remove_base_valid_ref(CONTEXT_REF))
                assert(false); // should never actually hit this
            }
          }
          else
          {
            ColorSpaceIterator *itr = 
              index_part->color_space->create_color_space_iterator();
            while (itr->is_valid())
            {
              const LegionColor color = itr->yield_color();
              RegionNode *child = it->second->get_child(color);
              bool first = false;
              const DistributedID did = 
                collective_dids[did_index++]->get_value(false/*block*/);
              EquivalenceSet *set = 
                repl_ctx->shard_manager->deduplicate_equivalence_set_creation(
                                                    child, context, did, first);
              // If we're the first shard of the owner we initialize the state
              if (first && set->is_owner())
                initialize_replicated_set(set, mask, map_applied_conditions);
              child->record_refinement(ctx, set, mask, map_applied_conditions);
              // Remove the CONTEXT_REF on the set now that it is registered
              if (set->remove_base_valid_ref(CONTEXT_REF))
                assert(false); // should never actually hit this
            }
            delete itr;
          }
        }
        for (std::map<LogicalRegion,RegionNode*>::const_iterator it =
              replicated_regions.begin(); it != replicated_regions.end(); it++)
        {
#ifdef DEBUG_LEGION
          assert(did_index < collective_dids.size());
#endif
          bool first = false;
          const DistributedID did = 
            collective_dids[did_index++]->get_value(false/*block*/);
          FieldMask mask = make_from[it->second];
          // Prune out any projection fields for this node
          LegionMap<RegionTreeNode*,
            FieldMaskSet<RefProjectionSummary> >::const_iterator
              finder = projections.find(it->second);
          if (finder != projections.end())
            mask -= finder->second.get_valid_mask();
#ifdef DEBUG_LEGION
          assert(!!mask);
#endif
          EquivalenceSet *set = 
            repl_ctx->shard_manager->deduplicate_equivalence_set_creation(
                                          it->second, context, did, first);
          // If we're the first shard of the owner we initialize the state
          if (first && set->is_owner())
            initialize_replicated_set(set, mask, map_applied_conditions);
          it->second->record_refinement(ctx, set, mask, 
                                        map_applied_conditions);
          // Remove the CONTEXT_REF on the set now that it is registered
          if (set->remove_base_valid_ref(CONTEXT_REF))
            assert(false); // should never actually hit this
        }
#ifdef DEBUG_LEGION
        assert(did_index == collective_dids.size());
#endif
      }
      if (!map_applied_conditions.empty())
        Runtime::phase_barrier_arrive(mapped_barrier, 1/*count*/,
            Runtime::merge_events(map_applied_conditions));
      else
        Runtime::phase_barrier_arrive(mapped_barrier, 1/*count*/);
      complete_mapping(mapped_barrier);
      complete_execution();
    }

    //--------------------------------------------------------------------------
    void ReplRefinementOp::initialize_replicated_set(EquivalenceSet *set,
                 const FieldMask &mask, std::set<RtEvent> &applied_events) const
    //--------------------------------------------------------------------------
    {
      const FieldMaskSet<EquivalenceSet> &previous_sets = 
        version_info.get_equivalence_sets();
      // Import state into this equivalence set
      for (FieldMaskSet<EquivalenceSet>::const_iterator it =
            previous_sets.begin(); it != previous_sets.end(); it++)
      {
        // See if the fields overlap first
        const FieldMask overlap = it->second & mask;
        if (!overlap)
          continue;
        IndexSpaceExpression *overlap_expr = 
          runtime->forest->intersect_index_spaces(set->set_expr,
                            it->first->region_node->row_source);
        if (overlap_expr->is_empty())
          continue;
        set->clone_from(runtime->address_space, it->first, mask,
                      false/*forward to owner*/, applied_events);
      }
    }

    /////////////////////////////////////////////////////////////
    // Repl Fill Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplFillOp::ReplFillOp(Runtime *rt)
      : FillOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplFillOp::ReplFillOp(const ReplFillOp &rhs)
      : FillOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplFillOp::~ReplFillOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplFillOp& ReplFillOp::operator=(const ReplFillOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplFillOp::initialize_replication(ReplicateContext *ctx,
                                            ShardID allocator_shard,
                                            bool is_first)
    //--------------------------------------------------------------------------
    {
      collective_id = ctx->get_next_collective_index(COLLECTIVE_LOC_77);
      fill_view_allocator_shard = allocator_shard;
      is_first_local_shard = is_first;
    }

    //--------------------------------------------------------------------------
    void ReplFillOp::activate(void)
    //--------------------------------------------------------------------------
    {
      FillOp::activate();
      collective_map_barrier = RtBarrier::NO_RT_BARRIER;
      collective = NULL;
      collective_id = 0;
      fill_view_allocator_shard = 0;
      is_first_local_shard = false;
    }

    //--------------------------------------------------------------------------
    void ReplFillOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      // Make sure we didn't leak our barrier
      assert(!collective_map_barrier.exists());
#endif
      FillOp::deactivate(false/*free*/);
      if (collective != NULL)
        delete collective;
      if (freeop)
        runtime->free_repl_fill_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplFillOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // If we get here then we're not doing a physical trace replay
      // so we're going to need a collective fill barrier to sync
      // execution of our physical analysis before and after
      collective_map_barrier = repl_ctx->get_next_collective_map_barriers();
      // Then do the base class analysis
      FillOp::trigger_dependence_analysis();
    }

    //--------------------------------------------------------------------------
    void ReplFillOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(collective_map_barrier.exists());
#endif
      // Signal that all of our mapping dependences are satisfied
      Runtime::phase_barrier_arrive(collective_map_barrier, 1/*count*/);
      std::set<RtEvent> preconditions;
      const RtEvent view_ready = initialize_fill_view();
      if (view_ready.exists())
        preconditions.insert(view_ready);
      runtime->forest->perform_versioning_analysis(this, 0/*idx*/,
                                                   requirement,
                                                   version_info,
                                                   preconditions);
      if (!collective_map_barrier.has_triggered())
        preconditions.insert(collective_map_barrier);
      Runtime::advance_barrier(collective_map_barrier);
      if (!preconditions.empty())
        enqueue_ready_operation(Runtime::merge_events(preconditions));
      else
        enqueue_ready_operation();
    }

    //--------------------------------------------------------------------------
    RtEvent ReplFillOp::initialize_fill_view(void)
    //--------------------------------------------------------------------------
    {
      // This is happening in the mapping stage of the pipeline so we 
      // need to do a collective rendezvous to see if everyone finds the
      // same values. If not then we'll need to make a view.
      if (future.impl != NULL)
        fill_view = parent_ctx->find_fill_view(future, map_applied_conditions);
      else
        fill_view = parent_ctx->find_fill_view(value, value_size,
                                               map_applied_conditions);
      // Create the rendezvous collective
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
      assert(collective == NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      collective = new CreateCollectiveFillView(repl_ctx, collective_id, this,
          (fill_view == NULL) ? 0 : fill_view->did, fill_view_allocator_shard);
      collective->perform_collective_async();
      return collective->perform_collective_wait(false/*block*/);
    }

    //--------------------------------------------------------------------------
    bool ReplFillOp::perform_collective_analysis(CollectiveMapping *&mapping,
                                                 bool &first_local)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      mapping = &(repl_ctx->shard_manager->get_collective_mapping()); 
      mapping->add_reference();
      first_local = is_first_local_shard;
      return true;
    }

    //--------------------------------------------------------------------------
    RtEvent ReplFillOp::finalize_complete_mapping(RtEvent pre)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(collective_map_barrier.exists());
#endif
      Runtime::phase_barrier_arrive(collective_map_barrier, 1/*count*/, pre);
#ifdef DEBUG_LEGION
      const RtEvent result = collective_map_barrier;
      collective_map_barrier = RtBarrier::NO_RT_BARRIER;
      return result;
#else
      return collective_map_barrier;
#endif
    }

    /////////////////////////////////////////////////////////////
    // Repl Index Fill Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplIndexFillOp::ReplIndexFillOp(Runtime *rt)
      : IndexFillOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplIndexFillOp::ReplIndexFillOp(const ReplIndexFillOp &rhs)
      : IndexFillOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplIndexFillOp::~ReplIndexFillOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplIndexFillOp& ReplIndexFillOp::operator=(const ReplIndexFillOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplIndexFillOp::activate(void)
    //--------------------------------------------------------------------------
    {
      IndexFillOp::activate();
      sharding_functor = UINT_MAX;
      sharding_function = NULL;
      shard_points = NULL;
      mapper = NULL;
      collective = NULL;
      collective_id = 0;
      fill_view_allocator_shard = 0;
#ifdef DEBUG_LEGION
      sharding_collective = NULL;
#endif
    }

    //--------------------------------------------------------------------------
    void ReplIndexFillOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      if (sharding_collective != NULL)
        delete sharding_collective;
#endif
      IndexFillOp::deactivate(false/*free*/);
      remove_launch_space_reference(shard_points);
      if (collective != NULL)
        delete collective;
      if (freeop)
        runtime->free_repl_index_fill_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplIndexFillOp::trigger_prepipeline_stage(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Do the mapper call to get the sharding function to use
      if (mapper == NULL)
        mapper = runtime->find_mapper(
            parent_ctx->get_executing_processor(), map_id);
      Mapper::SelectShardingFunctorInput* input = repl_ctx->shard_manager;
      SelectShardingFunctorOutput output;
      mapper->invoke_fill_select_sharding_functor(this, input, &output);
      if (output.chosen_functor == UINT_MAX)
        REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                      "Mapper %s failed to pick a valid sharding functor for "
                      "index fill in task %s (UID %lld)", 
                      mapper->get_mapper_name(),
                      parent_ctx->get_task_name(), parent_ctx->get_unique_id())
      this->sharding_functor = output.chosen_functor;
      sharding_function = 
        repl_ctx->shard_manager->find_sharding_function(sharding_functor);
#ifdef DEBUG_LEGION
      assert(sharding_collective != NULL);
      sharding_collective->contribute(this->sharding_functor);
      if (sharding_collective->is_target() &&
          !sharding_collective->validate(this->sharding_functor))
        REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                      "Mapper %s chose different sharding functions "
                      "for index fill in task %s (UID %lld)", 
                      mapper->get_mapper_name(), parent_ctx->get_task_name(),
                      parent_ctx->get_unique_id())
#endif
      // Now we can do the normal prepipeline stage
      IndexFillOp::trigger_prepipeline_stage();
    }
    
    //--------------------------------------------------------------------------
    void ReplIndexFillOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
      perform_base_dependence_analysis();
      RefinementTracker tracker(this, map_applied_conditions);
      ProjectionInfo projection_info(runtime, requirement, launch_space, 
                                     sharding_function, sharding_space);
      runtime->forest->perform_dependence_analysis(this, 0/*idx*/,
                                                   requirement,
                                                   projection_info,
                                                   privilege_path, tracker,
                                                   map_applied_conditions);
    }

    //--------------------------------------------------------------------------
    void ReplIndexFillOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
      assert(launch_space != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Compute the local index space of points for this shard
      IndexSpace local_space;
      if (sharding_space.exists())
        local_space =
          sharding_function->find_shard_space(repl_ctx->owner_shard->shard_id,
              launch_space, sharding_space, get_provenance());
      else
        local_space =
          sharding_function->find_shard_space(repl_ctx->owner_shard->shard_id,
              launch_space, launch_space->handle, get_provenance());
      // If we're recording then record the local_space
      if (is_recording())
      {
#ifdef DEBUG_LEGION
        assert((tpl != NULL) && tpl->is_recording());
#endif
        tpl->record_local_space(trace_local_id, local_space);
      }
      // If it's empty we're done, otherwise we go back on the queue
      if (!local_space.exists())
      {
#ifdef LEGION_SPY
        // Still have to do this for legion spy
        LegionSpy::log_operation_events(unique_op_id, 
            ApEvent::NO_AP_EVENT, ApEvent::NO_AP_EVENT);
#endif
        // We have no local points, so we can just trigger
        // Still do the view initialization to rendezvous with collectives
        const RtEvent view_ready = initialize_fill_view();
        if (!map_applied_conditions.empty())
          complete_mapping(Runtime::merge_events(map_applied_conditions));
        else
          complete_mapping();
        if (future.impl != NULL)
        {
          RtEvent future_ready = future.impl->subscribe(); 
          // Make sure both the future and the view are ready
          if (view_ready.exists() && !view_ready.has_triggered())
          {
            if (!future_ready.has_triggered())
              future_ready = Runtime::merge_events(view_ready, future_ready);
            else
              future_ready = view_ready;
          }
          if (!future_ready.has_triggered())
            parent_ctx->add_to_trigger_execution_queue(this, future_ready);
          else
            trigger_execution(); // can do the completion now
        }
        else
          trigger_execution();
      }
      else // We have valid points, so it goes on the ready queue
      {
        shard_points = runtime->forest->get_node(local_space);
        add_launch_space_reference(shard_points);
        IndexFillOp::trigger_ready();
      }
    }

    //--------------------------------------------------------------------------
    RtEvent ReplIndexFillOp::initialize_fill_view(void)
    //--------------------------------------------------------------------------
    {
      // This is happening in the mapping stage of the pipeline so we 
      // need to do a collective rendezvous to see if everyone finds the
      // same values. If not then we'll need to make a view.
      if (future.impl != NULL)
        fill_view = parent_ctx->find_fill_view(future, map_applied_conditions);
      else
        fill_view = parent_ctx->find_fill_view(value, value_size,
                                               map_applied_conditions);
      // Create the rendezvous collective
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
      assert(collective == NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      collective = new CreateCollectiveFillView(repl_ctx, collective_id, this,
          (fill_view == NULL) ? 0 : fill_view->did, fill_view_allocator_shard);
      collective->perform_collective_async();
      return collective->perform_collective_wait(false/*block*/);
    }

    //--------------------------------------------------------------------------
    void ReplIndexFillOp::trigger_replay(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(tpl != NULL);
#endif
      const IndexSpace local_space = tpl->find_local_space(trace_local_id);
      // If it's empty we're done, otherwise we do the replay
      if (!local_space.exists())
      {
        // Still have to do this for legion spy
        if (runtime->legion_spy_enabled)
          log_index_fill_requirement();
#ifdef LEGION_SPY
        LegionSpy::log_replay_operation(unique_op_id);
        LegionSpy::log_operation_events(unique_op_id, 
            ApEvent::NO_AP_EVENT, ApEvent::NO_AP_EVENT);
#endif
        // We have no local points, so we can just trigger
        complete_mapping();
        complete_execution();
      }
      else
      {
        shard_points = runtime->forest->get_node(local_space);
        add_launch_space_reference(shard_points);
        IndexFillOp::trigger_replay();
      }
    }

    //--------------------------------------------------------------------------
    bool ReplIndexFillOp::find_shard_participants(std::vector<ShardID> &shards)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(sharding_function != NULL);
#endif
      if (sharding_space.exists())
        return sharding_function->find_shard_participants(launch_space,
                                                sharding_space, shards);
      else
        return sharding_function->find_shard_participants(launch_space,
                                          launch_space->handle, shards);
    }

    //--------------------------------------------------------------------------
    void ReplIndexFillOp::initialize_replication(ReplicateContext *ctx,
                                                 ShardID allocator_shard)
    //--------------------------------------------------------------------------
    {
      collective_id = ctx->get_next_collective_index(COLLECTIVE_LOC_93);
      fill_view_allocator_shard = allocator_shard;
    }

    /////////////////////////////////////////////////////////////
    // Repl Copy Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplCopyOp::ReplCopyOp(Runtime *rt)
      : CopyOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplCopyOp::ReplCopyOp(const ReplCopyOp &rhs)
      : CopyOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplCopyOp::~ReplCopyOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplCopyOp& ReplCopyOp::operator=(const ReplCopyOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplCopyOp::initialize_replication(ReplicateContext *ctx)
    //--------------------------------------------------------------------------
    {
      IndexSpace handle;
      if (index_domain.get_dim() == 0)
      {
        DomainPoint point(0);
        Domain launch_domain(point, point);
        handle = ctx->find_index_launch_space(launch_domain, get_provenance());
      }
      else
        handle = ctx->find_index_launch_space(index_domain, get_provenance());
      launch_space = runtime->forest->get_node(handle);
      // Initialize our index domain of a single point
      index_domain = Domain(index_point, index_point);
    }

    //--------------------------------------------------------------------------
    void ReplCopyOp::activate(void)
    //--------------------------------------------------------------------------
    {
      CopyOp::activate();
      launch_space = NULL;
      sharding_functor = UINT_MAX;
      sharding_function = NULL;
#ifdef DEBUG_LEGION
      sharding_collective = NULL;
#endif
    }

    //--------------------------------------------------------------------------
    void ReplCopyOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      if (sharding_collective != NULL)
        delete sharding_collective;
#endif
      CopyOp::deactivate(false/*free*/);
      if (freeop)
        runtime->free_repl_copy_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplCopyOp::trigger_prepipeline_stage(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Do the mapper call to get the sharding function to use
      if (mapper == NULL)
        mapper = runtime->find_mapper(
            parent_ctx->get_executing_processor(), map_id); 
      Mapper::SelectShardingFunctorInput* input = repl_ctx->shard_manager;
      SelectShardingFunctorOutput output;
      mapper->invoke_copy_select_sharding_functor(this, input, &output);
      if (output.chosen_functor == UINT_MAX)
        REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                      "Mapper %s failed to pick a valid sharding functor for "
                      "copy in task %s (UID %lld)", mapper->get_mapper_name(),
                      parent_ctx->get_task_name(), parent_ctx->get_unique_id())
      this->sharding_functor = output.chosen_functor;
      sharding_function = 
        repl_ctx->shard_manager->find_sharding_function(sharding_functor);
#ifdef DEBUG_LEGION
      assert(sharding_collective != NULL);
      sharding_collective->contribute(this->sharding_functor);
      if (sharding_collective->is_target() &&
          !sharding_collective->validate(this->sharding_functor))
        REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                      "Mapper %s chose different sharding functions "
                      "for copy in task %s (UID %lld)", 
                      mapper->get_mapper_name(), parent_ctx->get_task_name(), 
                      parent_ctx->get_unique_id())
#endif
      // Now we can do the normal prepipeline stage
      CopyOp::trigger_prepipeline_stage();
    }

    //--------------------------------------------------------------------------
    void ReplCopyOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
      perform_base_dependence_analysis(false/*permit projection*/);
      RefinementTracker refinement_tracker(this, map_applied_conditions);
      // Make these requirements look like projection requirmeents since we
      // need the logical analysis to look at sharding to determine if any
      // kind of close operations are required
      for (unsigned idx = 0; idx < src_requirements.size(); idx++)
      {
        const RegionRequirement &req = src_requirements[idx];
        ProjectionInfo projection_info(runtime, req, launch_space,
                                       sharding_function, sharding_space);
        runtime->forest->perform_dependence_analysis(this, idx, req, 
                                                     projection_info,
                                                     src_privilege_paths[idx],
                                                     refinement_tracker,
                                                     map_applied_conditions);
      }
      for (unsigned idx = 0; idx < dst_requirements.size(); idx++)
      {
        unsigned index = src_requirements.size()+idx;
        RegionRequirement &req = dst_requirements[idx];
        ProjectionInfo projection_info(runtime, req, launch_space,
                                       sharding_function, sharding_space);
        // Perform this dependence analysis as if it was READ_WRITE
        // so that we can get the version numbers correct
        const bool is_reduce_req = IS_REDUCE(req);
        if (is_reduce_req)
          req.privilege = LEGION_READ_WRITE;
        runtime->forest->perform_dependence_analysis(this, index, req, 
                                                     projection_info,
                                                     dst_privilege_paths[idx],
                                                     refinement_tracker,
                                                     map_applied_conditions);
        // Switch the privileges back when we are done
        if (is_reduce_req)
          req.privilege = LEGION_REDUCE;
      }
      if (!src_indirect_requirements.empty())
      {
        gather_versions.resize(src_indirect_requirements.size());
        const size_t offset = src_requirements.size() + dst_requirements.size();
        for (unsigned idx = 0; idx < src_requirements.size(); idx++)
        {
          const RegionRequirement &req = src_indirect_requirements[idx];
          ProjectionInfo projection_info(runtime, req, launch_space,
                                         sharding_function, sharding_space); 
          runtime->forest->perform_dependence_analysis(this, offset + idx, req,
                                                 projection_info,
                                                 gather_privilege_paths[idx],
                                                 refinement_tracker,
                                                 map_applied_conditions);
        }
      }
      if (!dst_indirect_requirements.empty())
      {
        scatter_versions.resize(dst_indirect_requirements.size());
        const size_t offset = src_requirements.size() +
          dst_requirements.size() + src_indirect_requirements.size();
        for (unsigned idx = 0; idx < src_requirements.size(); idx++)
        {
          const RegionRequirement &req = dst_indirect_requirements[idx];
          ProjectionInfo projection_info(runtime, req, launch_space,
                                         sharding_function, sharding_space);
          runtime->forest->perform_dependence_analysis(this, offset + idx, req,
                                                 projection_info,
                                                 scatter_privilege_paths[idx],
                                                 refinement_tracker,
                                                 map_applied_conditions);
        }
      }
    }

    //--------------------------------------------------------------------------
    void ReplCopyOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Figure out whether this shard owns this point
      ShardID owner_shard;
      if (sharding_space.exists())
      {
        Domain shard_domain;
        runtime->forest->find_launch_space_domain(sharding_space, shard_domain);
        owner_shard = sharding_function->find_owner(index_point, shard_domain);
      }
      else
        owner_shard = sharding_function->find_owner(index_point, index_domain); 
      // If we're recording then record the owner shard
      if (is_recording())
      {
#ifdef DEBUG_LEGION
        assert((tpl != NULL) && tpl->is_recording());
#endif
        tpl->record_owner_shard(trace_local_id, owner_shard);
      }
      if (runtime->legion_spy_enabled)
        LegionSpy::log_owner_shard(get_unique_id(), owner_shard);
      // If we own it we go on the queue, otherwise we complete early
      if (owner_shard != repl_ctx->owner_shard->shard_id)
      {
#ifdef LEGION_SPY
        // Still have to do this for legion spy
        LegionSpy::log_operation_events(unique_op_id, 
            ApEvent::NO_AP_EVENT, ApEvent::NO_AP_EVENT);
#endif
        // We don't own it, so we can pretend like we
        // mapped and executed this copy already
        complete_mapping();
        complete_execution();
      }
      else // We own it, so do the base call
        CopyOp::trigger_ready();
    }

    //--------------------------------------------------------------------------
    void ReplCopyOp::trigger_replay(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(tpl != NULL);
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      const ShardID owner_shard = tpl->find_owner_shard(trace_local_id);
      if (runtime->legion_spy_enabled)
        LegionSpy::log_owner_shard(get_unique_id(), owner_shard);
      if (owner_shard != repl_ctx->owner_shard->shard_id)
      {
#ifdef LEGION_SPY
        LegionSpy::log_replay_operation(unique_op_id);
        LegionSpy::log_operation_events(unique_op_id, 
            ApEvent::NO_AP_EVENT, ApEvent::NO_AP_EVENT);
#endif
        complete_mapping();
        complete_execution();
      }
      else // We own it, so do the base call
        CopyOp::trigger_replay();
    }

    /////////////////////////////////////////////////////////////
    // Repl Index Copy Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplIndexCopyOp::ReplIndexCopyOp(Runtime *rt)
      : IndexCopyOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplIndexCopyOp::ReplIndexCopyOp(const ReplIndexCopyOp &rhs)
      : IndexCopyOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplIndexCopyOp::~ReplIndexCopyOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplIndexCopyOp& ReplIndexCopyOp::operator=(const ReplIndexCopyOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplIndexCopyOp::activate(void)
    //--------------------------------------------------------------------------
    {
      IndexCopyOp::activate();
      sharding_functor = UINT_MAX;
      sharding_function = NULL;
      shard_points = NULL;
#ifdef DEBUG_LEGION
      sharding_collective = NULL;
#endif
    }

    //--------------------------------------------------------------------------
    void ReplIndexCopyOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      if (sharding_collective != NULL)
        delete sharding_collective;
#endif
      IndexCopyOp::deactivate(false/*free*/);
      pre_indirection_barriers.clear();
      post_indirection_barriers.clear();
      if (!src_collectives.empty())
      {
        for (std::vector<IndirectRecordExchange*>::const_iterator it =
              src_collectives.begin(); it != src_collectives.end(); it++)
          delete (*it);
        src_collectives.clear();
      }
      if (!dst_collectives.empty())
      {
        for (std::vector<IndirectRecordExchange*>::const_iterator it =
              dst_collectives.begin(); it != dst_collectives.end(); it++)
          delete (*it);
        dst_collectives.clear();
      }
      unique_intra_space_deps.clear();
      remove_launch_space_reference(shard_points);
      if (freeop)
        runtime->free_repl_index_copy_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplIndexCopyOp::trigger_prepipeline_stage(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Do the mapper call to get the sharding function to use
      if (mapper == NULL)
        mapper = runtime->find_mapper(
            parent_ctx->get_executing_processor(), map_id); 
      Mapper::SelectShardingFunctorInput* input = repl_ctx->shard_manager;
      SelectShardingFunctorOutput output;
      mapper->invoke_copy_select_sharding_functor(this, input, &output);
      if (output.chosen_functor == UINT_MAX)
        REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                      "Mapper %s failed to pick a valid sharding functor for "
                      "index copy in task %s (UID %lld)", 
                      mapper->get_mapper_name(),
                      parent_ctx->get_task_name(), parent_ctx->get_unique_id())
      this->sharding_functor = output.chosen_functor;
      sharding_function = 
        repl_ctx->shard_manager->find_sharding_function(sharding_functor); 
#ifdef DEBUG_LEGION
      assert(sharding_collective != NULL);
      sharding_collective->contribute(this->sharding_functor);
      if (sharding_collective->is_target() &&
          !sharding_collective->validate(this->sharding_functor))
        REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                      "Mapper %s chose different sharding functions "
                      "for index copy in task %s (UID %lld)", 
                      mapper->get_mapper_name(), parent_ctx->get_task_name(),
                      parent_ctx->get_unique_id())
#endif
      // Now we can do the normal prepipeline stage
      IndexCopyOp::trigger_prepipeline_stage();
    }

    //--------------------------------------------------------------------------
    void ReplIndexCopyOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
      perform_base_dependence_analysis(true/*permit projection*/);
      RefinementTracker refinement_tracker(this, map_applied_conditions);
      for (unsigned idx = 0; idx < src_requirements.size(); idx++)
      {
        ProjectionInfo projection_info (runtime, src_requirements[idx], 
                       launch_space, sharding_function, sharding_space);
        runtime->forest->perform_dependence_analysis(this, idx, 
                                                     src_requirements[idx],
                                                     projection_info,
                                                     src_privilege_paths[idx],
                                                     refinement_tracker,
                                                     map_applied_conditions);
      }
      for (unsigned idx = 0; idx < dst_requirements.size(); idx++)
      {
        ProjectionInfo projection_info(runtime, dst_requirements[idx], 
                       launch_space, sharding_function, sharding_space);
        unsigned index = src_requirements.size()+idx;
        // Perform this dependence analysis as if it was READ_WRITE
        // so that we can get the version numbers correct
        const bool is_reduce_req = IS_REDUCE(dst_requirements[idx]);
        if (is_reduce_req)
          dst_requirements[idx].privilege = LEGION_READ_WRITE;
        runtime->forest->perform_dependence_analysis(this, index, 
                                                     dst_requirements[idx],
                                                     projection_info,
                                                     dst_privilege_paths[idx],
                                                     refinement_tracker,
                                                     map_applied_conditions);
        // Switch the privileges back when we are done
        if (is_reduce_req)
          dst_requirements[idx].privilege = LEGION_REDUCE;
      }
      if (!src_indirect_requirements.empty())
      {
        gather_versions.resize(src_indirect_requirements.size());
        const unsigned offset = 
          src_requirements.size() + dst_requirements.size();
        for (unsigned idx = 0; idx < src_indirect_requirements.size(); idx++)
        {
          ProjectionInfo gather_info(runtime, src_indirect_requirements[idx], 
                             launch_space, sharding_function, sharding_space);
          runtime->forest->perform_dependence_analysis(this, offset + idx, 
                                                 src_indirect_requirements[idx],
                                                 gather_info,
                                                 gather_privilege_paths[idx],
                                                 refinement_tracker,
                                                 map_applied_conditions);
        }
      }
      if (!dst_indirect_requirements.empty())
      {
        scatter_versions.resize(dst_indirect_requirements.size());
        const unsigned offset = src_requirements.size() + 
          dst_requirements.size() + src_indirect_requirements.size();
        for (unsigned idx = 0; idx < dst_indirect_requirements.size(); idx++)
        {
          ProjectionInfo scatter_info(runtime, dst_indirect_requirements[idx],
                              launch_space, sharding_function, sharding_space);
          runtime->forest->perform_dependence_analysis(this, offset + idx, 
                                                 dst_indirect_requirements[idx],
                                                 scatter_info,
                                                 scatter_privilege_paths[idx],
                                                 refinement_tracker,
                                                 map_applied_conditions);
        }
      }
    }

    //--------------------------------------------------------------------------
    void ReplIndexCopyOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
      assert(pre_indirection_barriers.size() == 
              post_indirection_barriers.size());
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Compute the local index space of points for this shard
      IndexSpace local_space;
      if (sharding_space.exists())
        local_space =
          sharding_function->find_shard_space(repl_ctx->owner_shard->shard_id,
              launch_space, sharding_space, get_provenance());
      else
        local_space =
          sharding_function->find_shard_space(repl_ctx->owner_shard->shard_id,
              launch_space, launch_space->handle, get_provenance());
      // If we're recording then record the local_space
      if (is_recording())
      {
#ifdef DEBUG_LEGION
        assert((tpl != NULL) && tpl->is_recording());
#endif
        tpl->record_local_space(trace_local_id, local_space);
      }
      // If it's empty we're done, otherwise we go back on the queue
      if (!local_space.exists())
      {
        // If we have indirections then we still need to participate in those
        std::vector<RtEvent> done_events;
        if (!src_indirect_requirements.empty() &&
            collective_src_indirect_points)
        {
          for (unsigned idx = 0; idx < collective_exchanges.size(); idx++)
          {
            const RtEvent done = finalize_exchange(idx, true/*source*/);
            if (done.exists())
              done_events.push_back(done);
          }
        }
        if (!dst_indirect_requirements.empty() && 
            collective_dst_indirect_points)
        {
          for (unsigned idx = 0; idx < collective_exchanges.size(); idx++)
          {
            const RtEvent done = finalize_exchange(idx, false/*source*/);
            if (done.exists())
              done_events.push_back(done);
          }
        }
        // Arrive on our indirection barriers if we have them
        if (!pre_indirection_barriers.empty())
        {
          const PhysicalTraceInfo trace_info(this, 0/*index*/);
          for (unsigned idx = 0; idx < pre_indirection_barriers.size(); idx++)
          {
            Runtime::phase_barrier_arrive(pre_indirection_barriers[idx], 1);
            if (trace_info.recording)
            {
              const std::pair<size_t,size_t> key(trace_local_id, idx);
              trace_info.record_collective_barrier(
                  pre_indirection_barriers[idx], ApEvent::NO_AP_EVENT, key);
            }
          }
          for (unsigned idx = 0; idx < post_indirection_barriers.size(); idx++)
          {
            Runtime::phase_barrier_arrive(post_indirection_barriers[idx], 1);
            if (trace_info.recording)
            {
              const std::pair<size_t,size_t> key(trace_local_id,
                          pre_indirection_barriers.size() + idx);
              trace_info.record_collective_barrier(
                  post_indirection_barriers[idx], ApEvent::NO_AP_EVENT, key);
            }
          }
        }
#ifdef LEGION_SPY
        // Still have to do this for legion spy
        LegionSpy::log_operation_events(unique_op_id, 
            ApEvent::NO_AP_EVENT, ApEvent::NO_AP_EVENT);
#endif
        // We have no local points, so we can just trigger
        complete_mapping();
        if (!done_events.empty())
          complete_execution(Runtime::merge_events(done_events));
        else
          complete_execution();
      }
      else // If we have any valid points do the base call
      {
        shard_points = runtime->forest->get_node(local_space);
        add_launch_space_reference(shard_points);
        IndexCopyOp::trigger_ready();
      }
    }

    //--------------------------------------------------------------------------
    void ReplIndexCopyOp::trigger_replay(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(tpl != NULL);
      assert(pre_indirection_barriers.size() == 
              post_indirection_barriers.size());
#endif
      // No matter what we need to tell the shard template about any
      // collective barriers that it is going to need for its replay
      if (!pre_indirection_barriers.empty())
      {
#ifdef DEBUG_LEGION
        ShardedPhysicalTemplate *shard_template =
          dynamic_cast<ShardedPhysicalTemplate*>(tpl);
        assert(shard_template != NULL);
#else
        ShardedPhysicalTemplate *shard_template =
          static_cast<ShardedPhysicalTemplate*>(tpl);
#endif
        std::pair<size_t,size_t> key(trace_local_id, 0);
        for (unsigned idx = 0; idx < pre_indirection_barriers.size(); idx++)
        {
          shard_template->prepare_collective_barrier_replay(key, 
                                  pre_indirection_barriers[idx]);
          key.second++;
        }
        for (unsigned idx = 0; idx < post_indirection_barriers.size(); idx++)
        {
          shard_template->prepare_collective_barrier_replay(key, 
                                post_indirection_barriers[idx]);
          key.second++;
        }
      }
      // Elide unused collectives
      for (std::vector<IndirectRecordExchange*>::const_iterator it =
            src_collectives.begin(); it != src_collectives.end(); it++)
        (*it)->elide_collective();
      for (std::vector<IndirectRecordExchange*>::const_iterator it =
            dst_collectives.begin(); it != dst_collectives.end(); it++)
        (*it)->elide_collective();
      const IndexSpace local_space = tpl->find_local_space(trace_local_id);
      // If it's empty we're done, otherwise we do the replay
      if (!local_space.exists())
      {
#ifdef LEGION_SPY
        LegionSpy::log_replay_operation(unique_op_id);
        LegionSpy::log_operation_events(unique_op_id, 
            ApEvent::NO_AP_EVENT, ApEvent::NO_AP_EVENT);
#endif
        // We have no local points, so we can just trigger
        complete_mapping();
        complete_execution();
      }
      else
      {
        shard_points = runtime->forest->get_node(local_space);
        add_launch_space_reference(shard_points);
        std::vector<ApBarrier> copy_pre_barriers, copy_post_barriers;
        IndexCopyOp::trigger_replay();
      }
    }

    //--------------------------------------------------------------------------
    RtEvent ReplIndexCopyOp::exchange_indirect_records(
        const unsigned index, const ApEvent local_pre, const ApEvent local_post,
        ApEvent &collective_pre, ApEvent &collective_post,
        const TraceInfo &trace_info, const InstanceSet &insts,
        const RegionRequirement &req,
        std::vector<IndirectRecord> &records, const bool sources)
    //--------------------------------------------------------------------------
    {
      if (sources && !collective_src_indirect_points)
        return CopyOp::exchange_indirect_records(index, local_pre, local_post,
                              collective_pre, collective_post, trace_info, 
                              insts, req, records, sources);
      if (!sources && !collective_dst_indirect_points)
        return CopyOp::exchange_indirect_records(index, local_pre, local_post,
                              collective_pre, collective_post, trace_info,
                              insts, req, records, sources);
#ifdef DEBUG_LEGION
      assert(local_pre.exists());
      assert(local_post.exists());
      assert(index < pre_indirection_barriers.size());
      assert(index < post_indirection_barriers.size());
#endif
      // Take the lock and record our sets and instances
      AutoLock o_lock(op_lock);
#ifdef DEBUG_LEGION
      assert(index < collective_exchanges.size());
#endif
      IndirectionExchange &exchange = collective_exchanges[index];
      if (sources)
      {
        collective_pre = pre_indirection_barriers[index];
        collective_post = post_indirection_barriers[index];;
        if (!exchange.src_ready.exists())
          exchange.src_ready = Runtime::create_rt_user_event();
        if (exchange.local_preconditions.size() < points.size())
        {
          exchange.local_preconditions.insert(local_pre);
          if (exchange.local_preconditions.size() == points.size())
          {
            const ApEvent local_precondition = 
              Runtime::merge_events(&trace_info, exchange.local_preconditions);
            Runtime::phase_barrier_arrive(pre_indirection_barriers[index],
                                          1/*count*/, local_precondition);
            if (trace_info.recording)
            {
              std::pair<size_t,size_t> key(trace_local_id, index);
              trace_info.record_collective_barrier(
                  pre_indirection_barriers[index], local_precondition, key);
            }
          }
        }
        if (exchange.local_postconditions.size() < points.size())
        {
          exchange.local_postconditions.insert(local_post);
          if (exchange.local_postconditions.size() == points.size())
          {
            const ApEvent local_postcondition =
              Runtime::merge_events(&trace_info, exchange.local_postconditions);
            Runtime::phase_barrier_arrive(post_indirection_barriers[index],
                                          1/*count*/, local_postcondition);
            if (trace_info.recording)
            {
              std::pair<size_t,size_t> key(trace_local_id,
                  pre_indirection_barriers.size() + index);
              trace_info.record_collective_barrier(
                  post_indirection_barriers[index], local_postcondition, key);
            }
          }
        }
#ifdef DEBUG_LEGION
        assert(index < src_indirect_records.size());
        assert(src_indirect_records[index].size() < points.size());
#endif
        src_indirect_records[index].emplace_back(
            IndirectRecord(runtime->forest, req, insts));
        exchange.src_records.push_back(&records);
        if (src_indirect_records[index].size() == points.size())
          return finalize_exchange(index, true/*sources*/);
        return exchange.src_ready;
      }
      else
      {
        collective_pre = pre_indirection_barriers[index];
        collective_post = post_indirection_barriers[index];
        if (!exchange.dst_ready.exists())
          exchange.dst_ready = Runtime::create_rt_user_event();
        if (exchange.local_preconditions.size() < points.size())
        {
          exchange.local_preconditions.insert(local_pre);
          if (exchange.local_preconditions.size() == points.size())
          {
            const ApEvent local_precondition = 
              Runtime::merge_events(&trace_info, exchange.local_preconditions);
            Runtime::phase_barrier_arrive(pre_indirection_barriers[index],
                                          1/*count*/, local_precondition);
            if (trace_info.recording)
            {
              std::pair<size_t,size_t> key(trace_local_id, index);
              trace_info.record_collective_barrier(
                  pre_indirection_barriers[index], local_precondition, key);
            }
          }
        }
        if (exchange.local_postconditions.size() < points.size())
        {
          exchange.local_postconditions.insert(local_post);
          if (exchange.local_postconditions.size() == points.size())
          {
            const ApEvent local_postcondition =
              Runtime::merge_events(&trace_info, exchange.local_postconditions);
            Runtime::phase_barrier_arrive(post_indirection_barriers[index],
                                          1/*count*/, local_postcondition);
            if (trace_info.recording)
            {
              std::pair<size_t,size_t> key(trace_local_id,
                  pre_indirection_barriers.size() + index);
              trace_info.record_collective_barrier(
                  post_indirection_barriers[index], local_postcondition, key);
            }
          }
        }
#ifdef DEBUG_LEGION
        assert(index < dst_indirect_records.size());
        assert(dst_indirect_records[index].size() < points.size());
#endif
        dst_indirect_records[index].emplace_back(
            IndirectRecord(runtime->forest, req, insts));
        exchange.dst_records.push_back(&records);
        if (dst_indirect_records[index].size() == points.size())
          return finalize_exchange(index, false/*sources*/);
        return exchange.dst_ready;
      }
    }

    //--------------------------------------------------------------------------
    RtEvent ReplIndexCopyOp::finalize_exchange(const unsigned index, 
                                               const bool source)
    //--------------------------------------------------------------------------
    {
      IndirectionExchange &exchange = collective_exchanges[index];
      if (source)
      {
#ifdef DEBUG_LEGION
        assert(index < src_collectives.size());
#endif
        const RtEvent ready = src_collectives[index]->exchange_records(
                      exchange.src_records, src_indirect_records[index]);
        if (exchange.src_ready.exists())
        {
          Runtime::trigger_event(exchange.src_ready, ready);
          return exchange.src_ready;
        }
        else
          return ready;
      }
      else
      {
#ifdef DEBUG_LEGION
        assert(index < dst_collectives.size());
#endif
        const RtEvent ready = dst_collectives[index]->exchange_records(
                      exchange.dst_records, dst_indirect_records[index]);
        if (exchange.dst_ready.exists())
        {
          Runtime::trigger_event(exchange.dst_ready, ready);
          return exchange.dst_ready;
        }
        else
          return ready;
      }
    }

    //--------------------------------------------------------------------------
    void ReplIndexCopyOp::initialize_replication(ReplicateContext *ctx)
    //--------------------------------------------------------------------------
    { 
      if (!src_indirect_requirements.empty() && collective_src_indirect_points)
      {
        src_collectives.resize(src_indirect_requirements.size());
        for (unsigned idx = 0; idx < src_indirect_requirements.size(); idx++)
          src_collectives[idx] = new IndirectRecordExchange(ctx,
            ctx->get_next_collective_index(COLLECTIVE_LOC_80));
      }
      if (!dst_indirect_requirements.empty() && collective_dst_indirect_points)
      {
        dst_collectives.resize(dst_indirect_requirements.size());
        for (unsigned idx = 0; idx < dst_indirect_requirements.size(); idx++)
          dst_collectives[idx] = new IndirectRecordExchange(ctx,
            ctx->get_next_collective_index(COLLECTIVE_LOC_81));
      }
      if (!src_indirect_requirements.empty() || 
          !dst_indirect_requirements.empty())
      {
#ifdef DEBUG_LEGION
        assert(src_indirect_requirements.empty() ||
               dst_indirect_requirements.empty() ||
               (src_indirect_requirements.size() == 
                dst_indirect_requirements.size()));
#endif
        pre_indirection_barriers.resize(
            (src_indirect_requirements.size() > 
              dst_indirect_requirements.size()) ?
                src_indirect_requirements.size() : 
                dst_indirect_requirements.size());
        post_indirection_barriers.resize(pre_indirection_barriers.size());
        for (unsigned idx = 0; idx < pre_indirection_barriers.size(); idx++)
        {
          pre_indirection_barriers[idx] = ctx->get_next_indirection_barriers();;
          post_indirection_barriers[idx] = pre_indirection_barriers[idx];
          Runtime::advance_barrier(post_indirection_barriers[idx]);
        }
      }
    }

    //--------------------------------------------------------------------------
    RtEvent ReplIndexCopyOp::find_intra_space_dependence(
                                                       const DomainPoint &point)
    //--------------------------------------------------------------------------
    {
      AutoLock o_lock(op_lock);
      // Check to see if we already have it
      std::map<DomainPoint,RtEvent>::const_iterator finder = 
        intra_space_dependences.find(point);
      if (finder != intra_space_dependences.end())
        return finder->second;  
      // Make a temporary event and then do different things depending on 
      // whether we own this point or whether a remote shard owns it
      const RtUserEvent pending_event = Runtime::create_rt_user_event();
      intra_space_dependences[point] = pending_event;
      // If not, check to see if this is a point that we expect to own
#ifdef DEBUG_LEGION
      assert(sharding_function != NULL);
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      Domain launch_domain;
      if (sharding_space.exists())
        runtime->forest->find_launch_space_domain(sharding_space,launch_domain);
      else
        launch_space->get_launch_space_domain(launch_domain);
      const ShardID point_shard = 
        sharding_function->find_owner(point, launch_domain); 
      if (point_shard != repl_ctx->owner_shard->shard_id)
      {
        // A different shard owns it so send a message to that shard 
        // requesting it to fill in the dependence
        Serializer rez;
        rez.serialize(repl_ctx->shard_manager->repl_id);
        rez.serialize(point_shard);
        rez.serialize(context_index);
        rez.serialize(point);
        rez.serialize(pending_event);
        rez.serialize(repl_ctx->owner_shard->shard_id);
        repl_ctx->shard_manager->send_intra_space_dependence(point_shard, rez);
      }
      else // We own it so do the normal thing
        pending_intra_space_dependences[point] = pending_event;
      return pending_event; 
    }

    //--------------------------------------------------------------------------
    void ReplIndexCopyOp::record_intra_space_dependence(
        const DomainPoint &point, const DomainPoint &next, RtEvent point_mapped)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(sharding_function != NULL);
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Determine if the next point is one that we own or is one that is
      // going to be coming from a remote shard
      Domain launch_domain;
      if (sharding_space.exists())
        runtime->forest->find_launch_space_domain(sharding_space,launch_domain);
      else
        launch_space->get_launch_space_domain(launch_domain);
      const ShardID next_shard = 
        sharding_function->find_owner(next, launch_domain); 
      if (next_shard != repl_ctx->owner_shard->shard_id)
      {
        // Make sure we only send this to the repl_ctx once for each 
        // unique shard ID that we see for this point task
        const std::pair<DomainPoint,ShardID> key(point, next_shard); 
        bool record_dependence = true;
        {
          AutoLock o_lock(op_lock);
          std::set<std::pair<DomainPoint,ShardID> >::const_iterator finder = 
            unique_intra_space_deps.find(key);
          if (finder != unique_intra_space_deps.end())
            record_dependence = false;
          else
            unique_intra_space_deps.insert(key);
        }
        if (record_dependence)
          repl_ctx->record_intra_space_dependence(context_index, point, 
                                                  point_mapped, next_shard);
      }
      else // The next shard is ourself, so we can do the normal thing
        IndexCopyOp::record_intra_space_dependence(point, next, point_mapped);
    }

    //--------------------------------------------------------------------------
    bool ReplIndexCopyOp::find_shard_participants(std::vector<ShardID> &shards)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(sharding_function != NULL);
#endif
      if (sharding_space.exists())
        return sharding_function->find_shard_participants(launch_space,
                                                sharding_space, shards);
      else
        return sharding_function->find_shard_participants(launch_space,
                                          launch_space->handle, shards);
    }

    /////////////////////////////////////////////////////////////
    // Repl Deletion Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplDeletionOp::ReplDeletionOp(Runtime *rt)
      : DeletionOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplDeletionOp::ReplDeletionOp(const ReplDeletionOp &rhs)
      : DeletionOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplDeletionOp::~ReplDeletionOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplDeletionOp& ReplDeletionOp::operator=(const ReplDeletionOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplDeletionOp::activate(void)
    //--------------------------------------------------------------------------
    {
      DeletionOp::activate();
      ready_barrier = RtBarrier::NO_RT_BARRIER;
      mapping_barrier = RtBarrier::NO_RT_BARRIER;
      execution_barrier = RtBarrier::NO_RT_BARRIER;
      is_first_local_shard = false;
    }

    //--------------------------------------------------------------------------
    void ReplDeletionOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      DeletionOp::deactivate(false/*freeop*/);
      if (freeop)
        runtime->free_repl_deletion_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplDeletionOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
      // Do the base call
      DeletionOp::trigger_dependence_analysis();
      // Then get any barriers that we need for our execution
      // We might have already received our barriers
      if (execution_barrier.exists())
        return;
#ifdef DEBUG_LEGION
      assert(!mapping_barrier.exists());
      assert(!execution_barrier.exists());
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Only field and region deletions need a ready barrier since they
      // will be touching the physical states of the region tree
      if ((kind == LOGICAL_REGION_DELETION) || (kind == FIELD_DELETION))
      {
        ready_barrier = repl_ctx->get_next_deletion_ready_barrier();
        // Only field deletions need a mapping barrier for downward facing
        // dependences in other shards
        if (kind == FIELD_DELETION)
          mapping_barrier = repl_ctx->get_next_deletion_mapping_barrier();
      }
      // All deletion kinds need an execution barrier
      execution_barrier = repl_ctx->get_next_deletion_execution_barrier();
    }

    //--------------------------------------------------------------------------
    void ReplDeletionOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
      if ((kind == FIELD_DELETION) || (kind == LOGICAL_REGION_DELETION))
        Runtime::phase_barrier_arrive(ready_barrier, 1/*count*/);
      if (kind == FIELD_DELETION)
      {
        // Field deletions need to compute their version infos
        if (is_first_local_shard)
        {
          std::set<RtEvent> preconditions;
          version_infos.resize(deletion_requirements.size());
          for (unsigned idx = 0; idx < deletion_requirements.size(); idx++)
            runtime->forest->perform_versioning_analysis(this, idx,
                                              deletion_requirements[idx],
                                              version_infos[idx],
                                              preconditions);
          if (!preconditions.empty())
          {
            preconditions.insert(ready_barrier);
            enqueue_ready_operation(Runtime::merge_events(preconditions));
            return;
          }
        }
      }
      enqueue_ready_operation(ready_barrier);
    }

    //--------------------------------------------------------------------------
    void ReplDeletionOp::trigger_mapping(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(execution_barrier.exists());
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // There are two different implementations here depending on whether we
      // know that we have a deletion operation on every shard or not
      // If not, we just let the deletion for shard 0 do all the work, 
      // otherwise we know we can evenly distribute the work
      if (kind == LOGICAL_REGION_DELETION)
      {
        std::set<RtEvent> preconditions;
        // Figure out the versioning context for this requirements
        for (unsigned idx = 0; idx < deletion_requirements.size(); idx++)
        {
          const RegionRequirement &req = deletion_requirements[idx];
          repl_ctx->invalidate_region_tree_context(req.region, 
                                    preconditions, to_release);
        }
        if (!preconditions.empty())
          complete_mapping(Runtime::merge_events(preconditions));
        else
          complete_mapping();
      }
      else if (kind == FIELD_DELETION)
      {
#ifdef DEBUG_LEGION
        assert(mapping_barrier.exists());
#endif
        if (is_first_local_shard)
        {
          // For this case we actually need to go through and prune out any
          // valid instances for these fields in the equivalence sets in order
          // to be able to free up the resources.
          const TraceInfo trace_info(this);
          for (unsigned idx = 0; idx < deletion_requirements.size(); idx++)
            runtime->forest->invalidate_fields(this, idx, 
                deletion_requirements[idx], version_infos[idx],
                PhysicalTraceInfo(trace_info, idx), map_applied_conditions,
                &repl_ctx->shard_manager->get_collective_mapping(),
                is_first_local_shard);
        }
        // make sure that we don't try to do the deletion calls until
        // after the allocator is ready
        if (allocator->ready_event.exists())
          map_applied_conditions.insert(allocator->ready_event);
        if (!map_applied_conditions.empty())
          Runtime::phase_barrier_arrive(mapping_barrier, 1/*count*/,
              Runtime::merge_events(map_applied_conditions));
        else
          Runtime::phase_barrier_arrive(mapping_barrier, 1/*count*/);
        complete_mapping(mapping_barrier);
      }
      else
        complete_mapping();
      // complete execution once all the shards are done
      if (execution_precondition.exists())
        Runtime::phase_barrier_arrive(execution_barrier, 1/*count*/, 
            Runtime::protect_event(execution_precondition));
      else
        Runtime::phase_barrier_arrive(execution_barrier, 1/*count*/);
      complete_execution(execution_barrier);
    }

    //--------------------------------------------------------------------------
    void ReplDeletionOp::trigger_complete(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      std::set<RtEvent> applied;
      const CollectiveMapping &mapping =
        repl_ctx->shard_manager->get_collective_mapping();
      if (is_first_local_shard)
      {
        switch (kind)
        {
          case INDEX_SPACE_DELETION:
            {
#ifdef DEBUG_LEGION
              assert(deletion_req_indexes.empty());
#endif
              runtime->forest->destroy_index_space(index_space,
                    runtime->address_space, applied, &mapping);
              if (!sub_partitions.empty())
              {
                for (std::vector<IndexPartition>::const_iterator it = 
                      sub_partitions.begin(); it != sub_partitions.end(); it++)
                  runtime->forest->destroy_index_partition(*it, applied,
                                                           &mapping);
              }
              break;
            }
          case INDEX_PARTITION_DELETION:
            {
#ifdef DEBUG_LEGION
              assert(deletion_req_indexes.empty());
#endif
              runtime->forest->destroy_index_partition(index_part, applied,
                                                       &mapping);
              if (!sub_partitions.empty())
              {
                for (std::vector<IndexPartition>::const_iterator it = 
                      sub_partitions.begin(); it != sub_partitions.end(); it++)
                  runtime->forest->destroy_index_partition(*it, applied,
                                                           &mapping);
              }
              break;
            }
          case FIELD_SPACE_DELETION:
            {
#ifdef DEBUG_LEGION
              assert(deletion_req_indexes.empty());
#endif
              runtime->forest->destroy_field_space(field_space, applied,
                                                   &mapping);
              break;
            }
          case FIELD_DELETION:
            // Everyone is going to do the same thing for field deletions
            break;
          case LOGICAL_REGION_DELETION:
            {
              // Only do something here if we don't have any parent req indexes
              // If we had no deletion requirements then we know there is
              // nothing to race with and we can just do our deletion
              if (parent_req_indexes.empty())
                runtime->forest->destroy_logical_region(logical_region, 
                                                        applied, &mapping);
              break;
            }
          default:
            assert(false);
        }
      }
      std::vector<LogicalRegion> regions_to_destroy;
      // If this is a field deletion then everyone does the same thing
      if (kind == FIELD_DELETION)
      {
        if (!local_fields.empty())
          runtime->forest->free_local_fields(field_space, local_fields, 
                              local_field_indexes, &mapping);
        if (!global_fields.empty())
          runtime->forest->free_fields(field_space, global_fields, applied, 
                                   (repl_ctx->owner_shard->shard_id != 0));
        parent_ctx->remove_deleted_fields(free_fields, parent_req_indexes);
        if (!local_fields.empty())
          parent_ctx->remove_deleted_local_fields(field_space, local_fields);
        if (!deletion_req_indexes.empty())
          parent_ctx->remove_deleted_requirements(deletion_req_indexes,
                                                  regions_to_destroy);
      }
      else if ((kind == LOGICAL_REGION_DELETION) && !parent_req_indexes.empty())
        parent_ctx->remove_deleted_requirements(parent_req_indexes,
                                                regions_to_destroy);
      if (!regions_to_destroy.empty() && is_first_local_shard)
      {
        for (std::vector<LogicalRegion>::const_iterator it =
             regions_to_destroy.begin(); it != regions_to_destroy.end(); it++)
          runtime->forest->destroy_logical_region(*it, applied, &mapping);
      }
      if (!to_release.empty())
      {
        for (std::vector<EquivalenceSet*>::const_iterator it =
              to_release.begin(); it != to_release.end(); it++)
          if ((*it)->remove_base_valid_ref(DISJOINT_COMPLETE_REF))
            delete (*it);
        to_release.clear();
      }
#ifdef LEGION_SPY
      // Still have to do this for legion spy
      LegionSpy::log_operation_events(unique_op_id, 
          ApEvent::NO_AP_EVENT, ApEvent::NO_AP_EVENT);
#endif
      if (!applied.empty())
        complete_operation(Runtime::merge_events(applied));
      else
        complete_operation();
    }

    //--------------------------------------------------------------------------
    void ReplDeletionOp::initialize_replication(ReplicateContext *ctx,
                                                bool is_first,
                                                RtBarrier *ready_bar,
                                                RtBarrier *mapping_bar,
                                                RtBarrier *execution_bar)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!ready_barrier.exists());
      assert(!mapping_barrier.exists());
      assert(!execution_barrier.exists());
#endif
      is_first_local_shard = is_first;
      if (execution_bar != NULL)
      {
        // Get our barriers now
        if ((kind == LOGICAL_REGION_DELETION) || (kind == FIELD_DELETION))
        {
          ready_barrier = *ready_bar;
          Runtime::advance_barrier(*ready_bar);
          // Only field deletions need a mapping barrier for downward facing
          // dependences in other shards
          if (kind == FIELD_DELETION)
          {
            mapping_barrier = *mapping_bar;
            Runtime::advance_barrier(*mapping_bar);
          }
        }
        // All deletion kinds need an execution barrier
        execution_barrier = *execution_bar;
        Runtime::advance_barrier(*execution_bar);
      }
    }

    //--------------------------------------------------------------------------
    void ReplDeletionOp::record_unordered_kind(
       std::map<IndexSpace,ReplDeletionOp*> &index_space_deletions,
       std::map<IndexPartition,ReplDeletionOp*> &index_partition_deletions,
       std::map<FieldSpace,ReplDeletionOp*> &field_space_deletions,
       std::map<std::pair<FieldSpace,FieldID>,ReplDeletionOp*> &field_deletions,
       std::map<LogicalRegion,ReplDeletionOp*> &logical_region_deletions)
    //--------------------------------------------------------------------------
    {
      switch (kind)
      {
        case INDEX_SPACE_DELETION:
          {
#ifdef DEBUG_LEGION
            assert(index_space_deletions.find(index_space) ==
                    index_space_deletions.end());
#endif
            index_space_deletions[index_space] = this;
            break;
          }
        case INDEX_PARTITION_DELETION:
          {
#ifdef DEBUG_LEGION
            assert(index_partition_deletions.find(index_part) ==
                    index_partition_deletions.end());
#endif
            index_partition_deletions[index_part] = this;
            break;
          }
        case FIELD_SPACE_DELETION:
          {
#ifdef DEBUG_LEGION
            assert(field_space_deletions.find(field_space) ==
                    field_space_deletions.end());
#endif
            field_space_deletions[field_space] = this;
            break;
          }
        case FIELD_DELETION:
          {
#ifdef DEBUG_LEGION
            assert(!free_fields.empty());
#endif
            const std::pair<FieldSpace,FieldID> key(field_space,
                *(free_fields.begin()));
#ifdef DEBUG_LEGION
            assert(field_deletions.find(key) == field_deletions.end());
#endif
            field_deletions[key] = this;
            break;
          }
        case LOGICAL_REGION_DELETION:
          {
#ifdef DEBUG_LEGION
            assert(logical_region_deletions.find(logical_region) ==
                    logical_region_deletions.end());
#endif
            logical_region_deletions[logical_region] = this;
            break;
          }
        default:
          assert(false); // should never get here
      }
    }

    /////////////////////////////////////////////////////////////
    // Repl Pending Partition Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplPendingPartitionOp::ReplPendingPartitionOp(Runtime *rt)
      : PendingPartitionOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplPendingPartitionOp::ReplPendingPartitionOp(
                                              const ReplPendingPartitionOp &rhs)
      : PendingPartitionOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplPendingPartitionOp::~ReplPendingPartitionOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplPendingPartitionOp& ReplPendingPartitionOp::operator=(
                                              const ReplPendingPartitionOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplPendingPartitionOp::activate(void)
    //--------------------------------------------------------------------------
    {
      PendingPartitionOp::activate();
    }

    //--------------------------------------------------------------------------
    void ReplPendingPartitionOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      PendingPartitionOp::deactivate(false/*free*/);
      if (freeop)
        runtime->free_repl_pending_partition_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplPendingPartitionOp::populate_sources(const FutureMap &fm)
    //--------------------------------------------------------------------------
    {
      future_map = fm;
#ifdef DEBUG_LEGION
      assert(sources.empty());
      assert(future_map.impl != NULL);
#endif
      if (thunk->need_all_futures())
        future_map.impl->get_all_futures(sources);
      else
        future_map.impl->get_shard_local_futures(sources);
    }

    //--------------------------------------------------------------------------
    void ReplPendingPartitionOp::trigger_execution(void)
    //--------------------------------------------------------------------------
    {
      // We know we are in a replicate context
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Perform the partitioning operation
      const ApEvent ready_event = thunk->perform_shard(this, runtime->forest,
        repl_ctx->owner_shard->shard_id, repl_ctx->shard_manager->total_shards);
      if (ready_event.exists())
        record_completion_effect(ready_event);
      complete_execution();
    }

    /////////////////////////////////////////////////////////////
    // Repl Dependent Partition Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplDependentPartitionOp::ReplDependentPartitionOp(Runtime *rt)
      : DependentPartitionOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplDependentPartitionOp::ReplDependentPartitionOp(
                                            const ReplDependentPartitionOp &rhs)
      : DependentPartitionOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplDependentPartitionOp::~ReplDependentPartitionOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplDependentPartitionOp& ReplDependentPartitionOp::operator=(
                                            const ReplDependentPartitionOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplDependentPartitionOp::initialize_by_field(ReplicateContext *ctx, 
                                                       ShardID target,
                                                       ApEvent ready_event,
                                                       IndexPartition pid,
                                                       LogicalRegion handle, 
                                                       LogicalRegion parent,
                                                       IndexSpace color_space,
                                                       FieldID fid,
                                                       MapperID id, 
                                                       MappingTagID t,
                                                       const UntypedBuffer &arg,
                                                       Provenance *provenance)
    //--------------------------------------------------------------------------
    {
      parent_task = ctx->get_task();
      initialize_operation(ctx, true/*track*/, 0/*regions*/, provenance); 
      // Start without the projection requirement, we'll ask
      // the mapper later if it wants to turn this into an index launch
      requirement = 
        RegionRequirement(handle, LEGION_READ_ONLY, LEGION_EXCLUSIVE, parent);
      requirement.add_field(fid);
      map_id = id;
      tag = t;
      mapper_data_size = arg.get_size();
      if (mapper_data_size > 0)
      {
#ifdef DEBUG_LEGION
        assert(mapper_data == NULL);
#endif
        mapper_data = malloc(mapper_data_size);
        memcpy(mapper_data, arg.get_ptr(), mapper_data_size);
      }
#ifdef DEBUG_LEGION
      assert(thunk == NULL);
#endif
      thunk = new ReplByFieldThunk(ctx, target, pid);
      mapping_barrier = ctx->get_next_dependent_partition_barrier();;
      partition_ready = ready_event;
      if (runtime->legion_spy_enabled)
        perform_logging();
      if (runtime->check_privileges)
        check_by_field(pid, color_space, handle, parent, fid);
    }

    //--------------------------------------------------------------------------
    void ReplDependentPartitionOp::initialize_by_image(ReplicateContext *ctx, 
#ifndef SHARD_BY_IMAGE
                                                       ShardID target,
#endif
                                                       ApEvent ready_event,
                                                       IndexPartition pid,
                                                       IndexSpace handle,
                                                   LogicalPartition projection,
                                             LogicalRegion parent, FieldID fid,
                                                   MapperID id, MappingTagID t,
                                                   const UntypedBuffer &marg,
                                                   ShardID shard, size_t total,
                                                        Provenance *provenance)
    //--------------------------------------------------------------------------
    {
      parent_task = ctx->get_task();
      initialize_operation(ctx, true/*track*/, 0/*regions*/, provenance);
      // Start without the projection requirement, we'll ask
      // the mapper later if it wants to turn this into an index launch
      LogicalRegion proj_parent = 
        runtime->forest->get_parent_logical_region(projection);
      requirement = 
        RegionRequirement(proj_parent,LEGION_READ_ONLY,LEGION_EXCLUSIVE,parent);
      requirement.add_field(fid);
      map_id = id;
      tag = t;
      mapper_data_size = marg.get_size();
      if (mapper_data_size > 0)
      {
#ifdef DEBUG_LEGION
        assert(mapper_data == NULL);
#endif
        mapper_data = malloc(mapper_data_size);
        memcpy(mapper_data, marg.get_ptr(), mapper_data_size);
      }
#ifdef DEBUG_LEGION
      assert(thunk == NULL);
#endif
#ifdef SHARD_BY_IMAGE
      thunk = new ReplByImageThunk(ctx, pid, projection.get_index_partition(),
                                   shard, total);
#else
      thunk = new ReplByImageThunk(ctx, target, pid,
                                   projection.get_index_partition(),
                                   shard, total);
#endif
      mapping_barrier = ctx->get_next_dependent_partition_barrier();
      partition_ready = ready_event;
      if (runtime->legion_spy_enabled)
        perform_logging();
      if (runtime->check_privileges)
        check_by_image(pid, handle, projection, parent, fid);
    }

    //--------------------------------------------------------------------------
    void ReplDependentPartitionOp::initialize_by_image_range(
                                                         ReplicateContext *ctx, 
#ifndef SHARD_BY_IMAGE
                                                         ShardID target,
#endif
                                                         ApEvent ready_event,
                                                         IndexPartition pid,
                                                         IndexSpace handle,
                                                LogicalPartition projection,
                                                LogicalRegion parent,
                                                FieldID fid, MapperID id,
                                                MappingTagID t,  
                                                const UntypedBuffer &marg,
                                                ShardID shard, 
                                                size_t total_shards,
                                                Provenance *provenance) 
    //--------------------------------------------------------------------------
    {
      parent_task = ctx->get_task();
      initialize_operation(ctx, true/*track*/, 0/*regions*/, provenance);
      // Start without the projection requirement, we'll ask
      // the mapper later if it wants to turn this into an index launch
      LogicalRegion proj_parent = 
        runtime->forest->get_parent_logical_region(projection);
      requirement = 
        RegionRequirement(proj_parent,LEGION_READ_ONLY,LEGION_EXCLUSIVE,parent);
      requirement.add_field(fid);
      map_id = id;
      tag = t;
      mapper_data_size = marg.get_size();
      if (mapper_data_size > 0)
      {
#ifdef DEBUG_LEGION
        assert(mapper_data == NULL);
#endif
        mapper_data = malloc(mapper_data_size);
        memcpy(mapper_data, marg.get_ptr(), mapper_data_size);
      }
#ifdef DEBUG_LEGION
      assert(thunk == NULL);
#endif
#ifdef SHARD_BY_IMAGE
      thunk = new ReplByImageRangeThunk(ctx, pid, 
                                        projection.get_index_partition(),
                                        shard, total_shards);
#else
      thunk = new ReplByImageRangeThunk(ctx, target, pid, 
                                        projection.get_index_partition(),
                                        shard, total_shards);
#endif
      mapping_barrier = ctx->get_next_dependent_partition_barrier();;
      partition_ready = ready_event;
      if (runtime->legion_spy_enabled)
        perform_logging();
      if (runtime->check_privileges)
        check_by_image_range(pid, handle, projection, parent, fid);
    }

    //--------------------------------------------------------------------------
    void ReplDependentPartitionOp::initialize_by_preimage(ReplicateContext *ctx,
                              ShardID target_shard, ApEvent ready_event,
                              IndexPartition pid, IndexPartition proj,
                              LogicalRegion handle, LogicalRegion parent,
                              FieldID fid, MapperID id, MappingTagID t,
                              const UntypedBuffer &marg, Provenance *provenance)
    //--------------------------------------------------------------------------
    {
      parent_task = ctx->get_task();
      initialize_operation(ctx, true/*track*/, 0/*regions*/, provenance);
      // Start without the projection requirement, we'll ask
      // the mapper later if it wants to turn this into an index launch
      requirement = 
        RegionRequirement(handle, LEGION_READ_ONLY, LEGION_EXCLUSIVE, parent);
      requirement.add_field(fid);
      map_id = id;
      tag = t;
      mapper_data_size = marg.get_size();
      if (mapper_data_size > 0)
      {
#ifdef DEBUG_LEGION
        assert(mapper_data == NULL);
#endif
        mapper_data = malloc(mapper_data_size);
        memcpy(mapper_data, marg.get_ptr(), mapper_data_size);
      }
#ifdef DEBUG_LEGION
      assert(thunk == NULL);
#endif
      thunk = new ReplByPreimageThunk(ctx, target_shard, pid, proj);
      mapping_barrier = ctx->get_next_dependent_partition_barrier();
      partition_ready = ready_event;
      if (runtime->legion_spy_enabled)
        perform_logging();
      if (runtime->check_privileges)
        check_by_preimage(pid, proj, handle, parent, fid);
    }

    //--------------------------------------------------------------------------
    void ReplDependentPartitionOp::initialize_by_preimage_range(
                              ReplicateContext *ctx, ShardID target_shard,
                              ApEvent ready_event,
                              IndexPartition pid, IndexPartition proj,
                              LogicalRegion handle, LogicalRegion parent,
                              FieldID fid, MapperID id, MappingTagID t,
                              const UntypedBuffer &marg, Provenance *provenance)
    //--------------------------------------------------------------------------
    {
      parent_task = ctx->get_task();
      initialize_operation(ctx, true/*track*/, 0/*regions*/, provenance);
      // Start without the projection requirement, we'll ask
      // the mapper later if it wants to turn this into an index launch
      requirement = 
        RegionRequirement(handle, LEGION_READ_ONLY, LEGION_EXCLUSIVE, parent);
      requirement.add_field(fid);
      map_id = id;
      tag = t;
      mapper_data_size = marg.get_size();
      if (mapper_data_size > 0)
      {
#ifdef DEBUG_LEGION
        assert(mapper_data == NULL);
#endif
        mapper_data = malloc(mapper_data_size);
        memcpy(mapper_data, marg.get_ptr(), mapper_data_size);
      }
#ifdef DEBUG_LEGION
      assert(thunk == NULL);
#endif
      thunk = new ReplByPreimageRangeThunk(ctx, target_shard, pid, proj);
      mapping_barrier = ctx->get_next_dependent_partition_barrier();
      partition_ready = ready_event;
      if (runtime->legion_spy_enabled)
        perform_logging();
      if (runtime->check_privileges)
        check_by_preimage_range(pid, proj, handle, parent, fid);
    }

    //--------------------------------------------------------------------------
    void ReplDependentPartitionOp::initialize_by_association(
                              ReplicateContext *ctx, LogicalRegion domain,
                              LogicalRegion domain_parent, FieldID fid,
                              IndexSpace range, MapperID id, MappingTagID tag,
                              const UntypedBuffer &marg, Provenance *provenance)
    //--------------------------------------------------------------------------
    {
      mapping_barrier = ctx->get_next_dependent_partition_barrier();
      DependentPartitionOp::initialize_by_association(ctx, domain, 
                          domain_parent, fid, range, id, tag, marg, provenance);
    }

    //--------------------------------------------------------------------------
    void ReplDependentPartitionOp::activate(void)
    //--------------------------------------------------------------------------
    {
      DependentPartitionOp::activate();
      sharding_function = NULL;
      shard_points = NULL;
#ifdef DEBUG_LEGION
      sharding_collective = NULL;
#endif
    }

    //--------------------------------------------------------------------------
    void ReplDependentPartitionOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      DependentPartitionOp::deactivate(false/*free*/);
#ifdef DEBUG_LEGION
      if (sharding_collective != NULL)
        delete sharding_collective;
#endif
      remove_launch_space_reference(shard_points);
      if (freeop)
        runtime->free_repl_dependent_partition_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplDependentPartitionOp::select_sharding_function(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
      assert(sharding_function == NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Do the mapper call to get the sharding function to use
      if (mapper == NULL)
        mapper = runtime->find_mapper(
            parent_ctx->get_executing_processor(), map_id);
      Mapper::SelectShardingFunctorInput* input = repl_ctx->shard_manager;
      SelectShardingFunctorOutput output;
      mapper->invoke_partition_select_sharding_functor(this, input, &output);
      if (output.chosen_functor == UINT_MAX)
        REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                      "Mapper %s failed to pick a valid sharding functor for "
                      "dependent partition in task %s (UID %lld)", 
                      mapper->get_mapper_name(),
                      parent_ctx->get_task_name(), parent_ctx->get_unique_id())
      sharding_function = repl_ctx->shard_manager->find_sharding_function(
                                                    output.chosen_functor);
#ifdef DEBUG_LEGION
      assert(sharding_collective != NULL);
      sharding_collective->contribute(output.chosen_functor);
      if (sharding_collective->is_target() &&
          !sharding_collective->validate(output.chosen_functor))
        REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                      "Mapper %s chose different sharding functions "
                      "for dependent partition op in task %s (UID %lld)", 
                      mapper->get_mapper_name(), parent_ctx->get_task_name(),
                      parent_ctx->get_unique_id())
#endif
    }

    //--------------------------------------------------------------------------
    void ReplDependentPartitionOp::select_partition_projection(void)
    //--------------------------------------------------------------------------
    {
      if (!runtime->unsafe_mapper)
      {
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
        assert(sharding_function == NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        // Check here that all the shards pick the same partition
        requirement.partition = LogicalPartition::NO_PART;   
        DependentPartitionOp::select_partition_projection();
        ValueBroadcast<LogicalPartition> part_check(
         repl_ctx->get_next_collective_index(COLLECTIVE_LOC_22,true/*logical*/),
         repl_ctx, 0/*origin shard*/);
        if (repl_ctx->owner_shard->shard_id > 0)
        {
          const LogicalPartition chosen_part = part_check.get_value();
          if (chosen_part != requirement.partition)
            REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                      "Invalid mapper output from invocation of "
                      "'select_partition_projection' on mapper %s for "
                      "depedent partitioning operation launched in %s "
                      "(UID %lld). Mapper selected a logical partition "
                      "on shard %d that is different than the logical "
                      "partition selected by shard 0. All shards must "
                      "select the same logical partition.",
                      mapper->get_mapper_name(), parent_ctx->get_task_name(), 
                      parent_ctx->get_unique_id(), 
                      repl_ctx->owner_shard->shard_id)
        }
        else
          part_check.broadcast(requirement.partition);
      }
      else
        DependentPartitionOp::select_partition_projection();
    }

    //--------------------------------------------------------------------------
    void ReplDependentPartitionOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
      if (runtime->check_privileges)
        check_privilege();
      // Before doing the dependence analysis we have to ask the
      // mapper whether it would like to make this an index space
      // operation or a single operation
      select_partition_projection();
      // Now that we know that we have the right region requirement we
      // can ask the mapper to also pick the sharding function
      select_sharding_function();
      // Do thise now that we've picked our region requirement
      initialize_privilege_path(privilege_path, requirement);
      if (runtime->legion_spy_enabled)
        log_requirement();
      ProjectionInfo projection_info;
      RefinementTracker tracker(this, map_applied_conditions);
      if (is_index_space)
      {
        projection_info = ProjectionInfo(runtime, requirement, 
                                         launch_space, sharding_function);

      }
      runtime->forest->perform_dependence_analysis(this, 0/*idx*/,
                                                   requirement,
                                                   projection_info,
                                                   privilege_path, tracker,
                                                   map_applied_conditions);
      // Record this dependent partition op with the context so that it 
      // can track implicit dependences on it for later operations
      parent_ctx->update_current_implicit(this);
    }

    //--------------------------------------------------------------------------
    void ReplDependentPartitionOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      
      // Do different things if this is an index space point or a single point
      if (is_index_space)
      {
#ifdef DEBUG_LEGION
        assert(sharding_function != NULL);
#endif
        // Compute the local index space of points for this shard
        IndexSpace local_space =
          sharding_function->find_shard_space(repl_ctx->owner_shard->shard_id,
              launch_space, launch_space->handle, get_provenance());
        // If it's empty we're done, otherwise we go back on the queue
        if (!local_space.exists())
        {
#ifdef LEGION_SPY
          // Still have to do this for legion spy
          LegionSpy::log_operation_events(unique_op_id, 
              ApEvent::NO_AP_EVENT, ApEvent::NO_AP_EVENT);
#endif
          // We aren't participating directly, but we still have to 
          // participate in the collective operations
          const ApEvent done_event = 
            thunk->perform(this,runtime->forest,ApEvent::NO_AP_EVENT,instances);
          // We have no local points, so we can just trigger
          Runtime::phase_barrier_arrive(mapping_barrier, 1/*count*/);
          complete_mapping(mapping_barrier);
          record_completion_effect(done_event);
          complete_execution();
        }
        else // If we have valid points then we do the base call
        {
          shard_points = runtime->forest->get_node(local_space);
          add_launch_space_reference(shard_points);
          DependentPartitionOp::trigger_ready();
        }
      }
      else
      {
        // Inform the thunk that we're eliding collectives since this
        // is a singular operation and not an index operation
        thunk->elide_collectives();
        // Shard 0 always owns dependent partition operations
        // If we own it we go on the queue, otherwise we complete early
        if (repl_ctx->owner_shard->shard_id != 0)
        {
#ifdef LEGION_SPY
          // Still have to do this for legion spy
          LegionSpy::log_operation_events(unique_op_id, 
              ApEvent::NO_AP_EVENT, ApEvent::NO_AP_EVENT);
#endif
          // We don't own it, so we can pretend like we
          // mapped and executed this task already
          Runtime::phase_barrier_arrive(mapping_barrier, 1/*count*/);
          complete_mapping(mapping_barrier);
          complete_execution();
        }
        else // If we're the shard then we do the base call
          DependentPartitionOp::trigger_ready();
      }
    }

    //--------------------------------------------------------------------------
    void ReplDependentPartitionOp::finalize_mapping(void)
    //--------------------------------------------------------------------------
    {
      RtEvent precondition;
      if (!map_applied_conditions.empty())
        precondition = Runtime::merge_events(map_applied_conditions);
      Runtime::phase_barrier_arrive(mapping_barrier, 1/*count*/, precondition);
      if (!acquired_instances.empty())
        precondition = release_nonempty_acquired_instances(mapping_barrier, 
                                                           acquired_instances);
      else
        precondition = mapping_barrier;
      complete_mapping(precondition);
    }

    //--------------------------------------------------------------------------
    ReplDependentPartitionOp::ReplByFieldThunk::ReplByFieldThunk(
        ReplicateContext *ctx, ShardID target, IndexPartition p)
      : ByFieldThunk(p), 
        gather_collective(FieldDescriptorGather(ctx, target, COLLECTIVE_LOC_54))
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ApEvent ReplDependentPartitionOp::ReplByFieldThunk::perform(
                              DependentPartitionOp *op,
                              RegionTreeForest *forest, ApEvent instances_ready,
                              const std::vector<FieldDataDescriptor> &instances)
    //--------------------------------------------------------------------------
    {
      if (op->is_index_space)
      {
        gather_collective.contribute(instances_ready, instances);
        if (gather_collective.is_target())
        {
          ApEvent all_ready;
          const std::vector<FieldDataDescriptor> &full_descriptors =
            gather_collective.get_full_descriptors(all_ready);
          // Perform the operation
          ApEvent done = forest->create_partition_by_field(op, pid,
                                      full_descriptors, all_ready);
          gather_collective.notify_remote_complete(done);
          return done;
        }
        else // nothing else for us to do
          return gather_collective.get_complete_event();
      }
      else // singular so just do the normal thing
        return forest->create_partition_by_field(op, pid, 
                                                 instances, instances_ready);
    }

    //--------------------------------------------------------------------------
#ifdef SHARD_BY_IMAGE
    ReplDependentPartitionOp::ReplByImageThunk::ReplByImageThunk(
                                          ReplicateContext *ctx, 
                                          IndexPartition p, IndexPartition proj,
                                          ShardID s, size_t total)
      : ByImageThunk(p, proj), 
        collective(FieldDescriptorExchange(ctx, COLLECTIVE_LOC_55)),
#else
    ReplDependentPartitionOp::ReplByImageThunk::ReplByImageThunk(
                                          ReplicateContext *ctx, ShardID target,
                                          IndexPartition p, IndexPartition proj,
                                          ShardID s, size_t total)
      : ByImageThunk(p, proj), 
        collective(FieldDescriptorGather(ctx, target, COLLECTIVE_LOC_55)),
#endif
        shard_id(s), total_shards(total)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ApEvent ReplDependentPartitionOp::ReplByImageThunk::perform(
                              DependentPartitionOp *op,
                              RegionTreeForest *forest, ApEvent instances_ready,
                              const std::vector<FieldDataDescriptor> &instances)
    //--------------------------------------------------------------------------
    {
      if (op->is_index_space)
      {
#ifdef SHARD_BY_IMAGE
        // There is a special case here if we're projecting the same 
        // partition that we used to make the instances, if it is then
        // we can avoid needing to do the exchange at all
        if ((op->requirement.handle_type == PART_PROJECTION) &&
            (op->requirement.partition.get_index_partition() == projection))
        {
          // Make sure we elide our collective to avoid leaking anything
          collective.elide_collective();
          if (!instances.empty())
            return forest->create_partition_by_image_range(op, pid, projection,
                instances, instances_ready, shard_id, total_shards);
          else
            return ApEvent::NO_AP_EVENT;
        }
        // Do the all-to-all gather of the field data descriptors
        ApEvent all_ready = collective.exchange_descriptors(instances_ready,
                                                            instances);
        ApEvent done = forest->create_partition_by_image(op, pid, projection,
                  collective.descriptors, all_ready, shard_id, total_shards);
        return collective.exchange_completion(done);
#else
        collective.contribute(instances_ready, instances);
        if (collective.is_target())
        {
          ApEvent all_ready;
          const std::vector<FieldDataDescriptor> &full_descriptors =
            collective.get_full_descriptors(all_ready);
          // Perform the operation
          ApEvent done = forest->create_partition_by_image(op, pid,
                          projection, full_descriptors, all_ready);
          collective.notify_remote_complete(done);
          return done;
        }
        else // nothing else for us to do
          return collective.get_complete_event();
#endif
      }
      else // singular so just do the normal thing
        return forest->create_partition_by_image(op, pid, projection, 
                                                 instances, instances_ready);
    }

    //--------------------------------------------------------------------------
#ifdef SHARD_BY_IMAGE
    ReplDependentPartitionOp::ReplByImageRangeThunk::ReplByImageRangeThunk(
                                          ReplicateContext *ctx, 
                                          IndexPartition p, IndexPartition proj,
                                          ShardID s, size_t total)
      : ByImageRangeThunk(p, proj), 
        collective(FieldDescriptorExchange(ctx, COLLECTIVE_LOC_60)),
#else
    ReplDependentPartitionOp::ReplByImageRangeThunk::ReplByImageRangeThunk(
                                          ReplicateContext *ctx, ShardID target,
                                          IndexPartition p, IndexPartition proj,
                                          ShardID s, size_t total)
      : ByImageRangeThunk(p, proj), 
        collective(FieldDescriptorGather(ctx, target, COLLECTIVE_LOC_60)),
#endif
        shard_id(s), total_shards(total)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ApEvent ReplDependentPartitionOp::ReplByImageRangeThunk::perform(
                              DependentPartitionOp *op,
                              RegionTreeForest *forest, ApEvent instances_ready,
                              const std::vector<FieldDataDescriptor> &instances)
    //--------------------------------------------------------------------------
    {
      if (op->is_index_space)
      {
#ifdef SHARD_BY_IMAGE
        // There is a special case here if we're projecting the same 
        // partition that we used to make the instances, if it is then
        // we can avoid needing to do the exchange at all
        if ((op->requirement.handle_type == PART_PROJECTION) &&
            (op->requirement.partition.get_index_partition() == projection))
        {
          // Make sure we elide our collective to avoid leaking anything
          collective.elide_collective();
          if (!instances.empty())
            return forest->create_partition_by_image_range(op, pid, projection,
                instances, instances_ready, shard_id, total_shards);
          else
            return ApEvent::NO_AP_EVENT;
        }
        // Do the all-to-all gather of the field data descriptors
        ApEvent all_ready = collective.exchange_descriptors(instances_ready,
                                                            instances);
        ApEvent done = forest->create_partition_by_image_range(op, pid, 
            projection,collective.descriptors,all_ready,shard_id,total_shards);
        return collective.exchange_completion(done);   
#else
        collective.contribute(instances_ready, instances);
        if (collective.is_target())
        {
          ApEvent all_ready;
          const std::vector<FieldDataDescriptor> &full_descriptors =
            collective.get_full_descriptors(all_ready);
          // Perform the operation
          ApEvent done = forest->create_partition_by_image_range(op, pid,
                              projection, full_descriptors, all_ready);
          collective.notify_remote_complete(done);
          return done;
        }
        else // nothing else for us to do
          return collective.get_complete_event();
#endif
      }
      else // singular so just do the normal thing
        return forest->create_partition_by_image_range(op, pid, projection, 
                                                 instances, instances_ready);
    }

    //--------------------------------------------------------------------------
    ReplDependentPartitionOp::ReplByPreimageThunk::ReplByPreimageThunk(
                                          ReplicateContext *ctx, ShardID target,
                                          IndexPartition p, IndexPartition proj)
      : ByPreimageThunk(p, proj), 
        gather_collective(FieldDescriptorGather(ctx, target, COLLECTIVE_LOC_56))
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ApEvent ReplDependentPartitionOp::ReplByPreimageThunk::perform(
                              DependentPartitionOp *op,
                              RegionTreeForest *forest, ApEvent instances_ready,
                              const std::vector<FieldDataDescriptor> &instances)
    //--------------------------------------------------------------------------
    {
      if (op->is_index_space)
      {
        gather_collective.contribute(instances_ready, instances);
        if (gather_collective.is_target())
        {
          ApEvent all_ready;
          const std::vector<FieldDataDescriptor> &full_descriptors =
            gather_collective.get_full_descriptors(all_ready);
          // Perform the operation
          ApEvent done = forest->create_partition_by_preimage(op, pid, 
                              projection, full_descriptors, all_ready);
          gather_collective.notify_remote_complete(done);
          return done;
        }
        else // nothing else for us to do
          return gather_collective.get_complete_event();
      }
      else // singular so just do the normal thing
        return forest->create_partition_by_preimage(op, pid, projection, 
                                                 instances, instances_ready);
    }
    
    //--------------------------------------------------------------------------
    ReplDependentPartitionOp::ReplByPreimageRangeThunk::
                 ReplByPreimageRangeThunk(ReplicateContext *ctx, ShardID target,
                                          IndexPartition p, IndexPartition proj)
      : ByPreimageRangeThunk(p, proj), 
        gather_collective(FieldDescriptorGather(ctx, target, COLLECTIVE_LOC_57))
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ApEvent ReplDependentPartitionOp::ReplByPreimageRangeThunk::perform(
                              DependentPartitionOp *op,
                              RegionTreeForest *forest, ApEvent instances_ready,
                              const std::vector<FieldDataDescriptor> &instances)
    //--------------------------------------------------------------------------
    {
      if (op->is_index_space)
      {
        gather_collective.contribute(instances_ready, instances);
        if (gather_collective.is_target())
        {
          ApEvent all_ready;
          const std::vector<FieldDataDescriptor> &full_descriptors =
            gather_collective.get_full_descriptors(all_ready);
          // Perform the operation
          ApEvent done = forest->create_partition_by_preimage_range(op, pid, 
                                    projection, full_descriptors, all_ready);
          gather_collective.notify_remote_complete(done);
          return done;
        }
        else // nothing else for us to do
          return gather_collective.get_complete_event();
      }
      else // singular so just do the normal thing
        return forest->create_partition_by_preimage_range(op, pid, projection, 
                                                 instances, instances_ready);
    }

    //--------------------------------------------------------------------------
    bool ReplDependentPartitionOp::find_shard_participants(
                                                   std::vector<ShardID> &shards)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(is_index_space);
      assert(sharding_function != NULL);
#endif
      return sharding_function->find_shard_participants(launch_space,
                                        launch_space->handle, shards);
    }

    /////////////////////////////////////////////////////////////
    // Repl Must Epoch Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplMustEpochOp::ReplMustEpochOp(Runtime *rt)
      : MustEpochOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplMustEpochOp::ReplMustEpochOp(const ReplMustEpochOp &rhs)
      : MustEpochOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplMustEpochOp::~ReplMustEpochOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplMustEpochOp& ReplMustEpochOp::operator=(const ReplMustEpochOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplMustEpochOp::activate(void)
    //--------------------------------------------------------------------------
    {
      MustEpochOp::activate();
      sharding_functor = UINT_MAX;
      sharding_function = NULL;
      mapping_collective_id = 0;
      collective_map_must_epoch_call = false;
      mapping_broadcast = NULL;
      mapping_exchange = NULL;
      dependence_exchange = NULL;
      completion_exchange = NULL;
      resource_return_barrier = RtBarrier::NO_RT_BARRIER;
      concurrent_prebar = RtBarrier::NO_RT_BARRIER;
      concurrent_postbar = RtBarrier::NO_RT_BARRIER;
#ifdef DEBUG_LEGION
      sharding_collective = NULL;
#endif
    }

    //--------------------------------------------------------------------------
    void ReplMustEpochOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      MustEpochOp::deactivate(false/*free*/);
      shard_single_tasks.clear();
      if (freeop)
        runtime->free_repl_epoch_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplMustEpochOp::instantiate_tasks(InnerContext *ctx, 
                                            const MustEpochLauncher &launcher)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(ctx);
#endif
      Provenance *provenance = get_provenance();
      // Initialize operations for everything in the launcher
      // Note that we do not track these operations as we want them all to
      // appear as a single operation to the parent context in order to
      // avoid deadlock with the maximum window size.
      indiv_tasks.resize(launcher.single_tasks.size());
      for (unsigned idx = 0; idx < launcher.single_tasks.size(); idx++)
      {
        ReplIndividualTask *task = 
          runtime->get_available_repl_individual_task();
        task->initialize_task(ctx, launcher.single_tasks[idx],
                              provenance, false/*track*/);
        task->set_must_epoch(this, idx, true/*register*/);
        // If we have a trace, set it for this operation as well
        if (trace != NULL)
          task->set_trace(trace, NULL);
        task->must_epoch_task = true;
        task->initialize_replication(repl_ctx);
        task->index_domain = this->launch_domain;
        task->sharding_space = launcher.sharding_space;
#ifdef DEBUG_LEGION
        task->set_sharding_collective(new ShardingGatherCollective(repl_ctx,
                                      0/*owner shard*/, COLLECTIVE_LOC_59));
#endif
        indiv_tasks[idx] = task;
      }
      indiv_triggered.resize(indiv_tasks.size(), false);
      index_tasks.resize(launcher.index_tasks.size());
      for (unsigned idx = 0; idx < launcher.index_tasks.size(); idx++)
      {
        IndexSpace launch_space = launcher.index_tasks[idx].launch_space;
        if (!launch_space.exists())
          launch_space = ctx->find_index_launch_space(
                          launcher.index_tasks[idx].launch_domain, provenance);
        ReplIndexTask *task = runtime->get_available_repl_index_task();
        task->initialize_task(ctx, launcher.index_tasks[idx],
                              launch_space, provenance, false/*track*/);
        task->set_must_epoch(this, indiv_tasks.size()+idx, true/*register*/);
        if (trace != NULL)
          task->set_trace(trace, NULL);
        task->must_epoch_task = true;
        task->initialize_replication(repl_ctx);
        task->sharding_space = launcher.sharding_space;
#ifdef DEBUG_LEGION
        task->set_sharding_collective(new ShardingGatherCollective(repl_ctx,
                                      0/*owner shard*/, COLLECTIVE_LOC_59));
#endif
        index_tasks[idx] = task;
      }
      index_triggered.resize(index_tasks.size(), false);
    }

    //--------------------------------------------------------------------------
    FutureMapImpl* ReplMustEpochOp::create_future_map(TaskContext *ctx,
                                IndexSpace launch_space, IndexSpace shard_space)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(ctx);
#endif
      IndexSpaceNode *launch_node = runtime->forest->get_node(launch_space);
      IndexSpaceNode *shard_node = 
        ((launch_space == shard_space) || !shard_space.exists()) ?
        launch_node : runtime->forest->get_node(shard_space);
      return new ReplFutureMapImpl(repl_ctx, this, launch_node, shard_node,
          runtime, runtime->get_available_distributed_id(),
          runtime->address_space, get_provenance());
    }

    //--------------------------------------------------------------------------
    RtEvent ReplMustEpochOp::get_concurrent_analysis_precondition(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // See if we are the first local shard on the lowest address space
      const CollectiveMapping &mapping = 
        repl_ctx->shard_manager->get_collective_mapping();
      const AddressSpace lowest = mapping[0];
      if ((lowest == runtime->address_space) && 
          repl_ctx->shard_manager->is_first_local_shard(repl_ctx->owner_shard))
      {
        Runtime::phase_barrier_arrive(concurrent_prebar, 1/*arrivals*/,
          runtime->acquire_concurrent_reservation(concurrent_postbar));
      }
      Runtime::phase_barrier_arrive(concurrent_postbar, 
          1/*arrivals*/, mapped_event);
      return concurrent_prebar;
    }

    //--------------------------------------------------------------------------
    MapperManager* ReplMustEpochOp::invoke_mapper(void)
    //--------------------------------------------------------------------------
    {
      Processor mapper_proc = parent_ctx->get_executing_processor();
      MapperManager *mapper = runtime->find_mapper(mapper_proc, map_id);
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // We want to do the map must epoch call
      // First find all the tasks that we own on this shard
      Domain shard_domain = launch_domain;
      if (sharding_space.exists())
        runtime->forest->find_launch_space_domain(sharding_space, shard_domain);
      for (std::vector<SingleTask*>::const_iterator it = 
            single_tasks.begin(); it != single_tasks.end(); it++)
      {
        const ShardID shard = 
          sharding_function->find_owner((*it)->index_point, shard_domain);
        if (runtime->legion_spy_enabled)
          LegionSpy::log_owner_shard((*it)->get_unique_id(), shard);
        // If it is not our shard then we don't own it
        if (shard != repl_ctx->owner_shard->shard_id)
          continue;
        shard_single_tasks.insert(*it);
      }
      // Find the set of constraints that apply to our local set of tasks
      std::vector<Mapper::MappingConstraint> local_constraints;
      std::vector<unsigned> original_constraint_indexes;
      for (unsigned idx = 0; idx < input.constraints.size(); idx++)
      {
        bool is_local = false;
        for (std::vector<const Task*>::const_iterator it = 
              input.constraints[idx].constrained_tasks.begin(); it !=
              input.constraints[idx].constrained_tasks.end(); it++)
        {
          SingleTask *single = static_cast<SingleTask*>(const_cast<Task*>(*it));
          if (shard_single_tasks.find(single) == shard_single_tasks.end())
            continue;
          is_local = true;
          break;
        }
        if (is_local)
        {
          local_constraints.push_back(input.constraints[idx]);
          original_constraint_indexes.push_back(idx);
        }
      }
      if (collective_map_must_epoch_call)
      {
        // Update the input tasks for our subset
        std::vector<const Task*> all_tasks(shard_single_tasks.begin(),
                                           shard_single_tasks.end());
        input.tasks.swap(all_tasks);
        // Sort them again by their index points to for determinism
        std::sort(input.tasks.begin(), input.tasks.end(), single_task_sorter);
        // Update the constraints to contain just our subset
        const size_t total_constraints = input.constraints.size();
        input.constraints.swap(local_constraints);
        // Fill in our shard mapping and local shard info
        input.shard_mapping = repl_ctx->shard_manager->shard_mapping;
        input.local_shard = repl_ctx->owner_shard->shard_id;
        // Update the outputs
        output.task_processors.resize(input.tasks.size());
        output.constraint_mappings.resize(input.constraints.size());
        output.weights.resize(input.constraints.size());
        // Now we can do the mapper call
        mapper->invoke_map_must_epoch(this, &input, &output);
        // Now we need to exchange our mapping decisions between all the shards
#ifdef DEBUG_LEGION
        assert(mapping_exchange == NULL);
        assert(mapping_collective_id > 0);
#endif
        mapping_exchange = 
          new MustEpochMappingExchange(repl_ctx, mapping_collective_id);
        mapping_exchange->exchange_must_epoch_mappings(
                  repl_ctx->owner_shard->shard_id,
                  repl_ctx->shard_manager->total_shards, total_constraints,
                  input.tasks, all_tasks, output.task_processors,
                  original_constraint_indexes, output.constraint_mappings,
                  output.weights, *get_acquired_instances_ref());
      }
      else
      {
#ifdef DEBUG_LEGION
        assert(mapping_broadcast == NULL);
        assert(mapping_collective_id > 0);
#endif
        mapping_broadcast = new MustEpochMappingBroadcast(repl_ctx, 
                                  0/*owner shard*/, mapping_collective_id);
        // Do the mapper call on shard 0 and then broadcast the results
        if (repl_ctx->owner_shard->shard_id == 0)
        {
          mapper->invoke_map_must_epoch(this, &input, &output);
          mapping_broadcast->broadcast(output.task_processors,
                                       output.constraint_mappings);
        }
        else
          mapping_broadcast->receive_results(output.task_processors,
              original_constraint_indexes, output.constraint_mappings,
              *get_acquired_instances_ref());
      }
      // No need to do any checks, the base class handles that
      return mapper;
    }

    //--------------------------------------------------------------------------
    void ReplMustEpochOp::map_and_distribute(std::set<RtEvent> &tasks_mapped,
                                             std::set<ApEvent> &tasks_complete)
    //--------------------------------------------------------------------------
    {
      // We have to exchange mapping and completion events with all the
      // other shards as well
      std::set<RtEvent> local_tasks_mapped;
      std::set<ApEvent> local_tasks_complete;
      for (std::vector<IndividualTask*>::const_iterator it = 
            indiv_tasks.begin(); it != indiv_tasks.end(); it++)
      {
        local_tasks_mapped.insert((*it)->get_mapped_event());
        local_tasks_complete.insert((*it)->get_completion_event());
      }
      for (std::vector<IndexTask*>::const_iterator it = 
            index_tasks.begin(); it != index_tasks.end(); it++)
      {
        local_tasks_mapped.insert((*it)->get_mapped_event());
        local_tasks_complete.insert((*it)->get_completion_event());
      }
      // Perform the mapping
      map_replicate_tasks();
      mapping_dependences.clear();
      RtEvent local_mapped = Runtime::merge_events(local_tasks_mapped);
      tasks_mapped.insert(local_mapped);
      ApEvent local_complete = Runtime::merge_events(NULL,local_tasks_complete);
      tasks_complete.insert(local_complete);
#ifdef DEBUG_LEGION
      assert(completion_exchange != NULL);
#endif
      completion_exchange->exchange_must_epoch_completion(
          local_mapped, local_complete, tasks_mapped, tasks_complete);
      // Then we can distribute the tasks
      distribute_replicate_tasks();
    }

    //--------------------------------------------------------------------------
    void ReplMustEpochOp::trigger_prepipeline_stage(void)
    //--------------------------------------------------------------------------
    {
      Processor mapper_proc = parent_ctx->get_executing_processor();
      MapperManager *mapper = runtime->find_mapper(mapper_proc, map_id);
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Select our sharding functor and then do the base call
      this->individual_tasks.resize(indiv_tasks.size());
      for (unsigned idx = 0; idx < indiv_tasks.size(); idx++)
        this->individual_tasks[idx] = indiv_tasks[idx];
      this->index_space_tasks.resize(index_tasks.size());
      for (unsigned idx = 0; idx < index_tasks.size(); idx++)
        this->index_space_tasks[idx] = index_tasks[idx];
      Mapper::SelectShardingFunctorInput sharding_input;
      sharding_input.shard_mapping = repl_ctx->shard_manager->shard_mapping;
      Mapper::MustEpochShardingFunctorOutput sharding_output;
      sharding_output.chosen_functor = UINT_MAX;
      sharding_output.collective_map_must_epoch_call = false;
      mapper->invoke_must_epoch_select_sharding_functor(this,
                                    &sharding_input, &sharding_output);
      // We can clear these now that we don't need them anymore
      individual_tasks.clear();
      index_space_tasks.clear();
      // Check that we have a sharding ID
      if (sharding_output.chosen_functor == UINT_MAX)
        REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
            "Invalid mapper output from invocation of "
            "'map_must_epoch' on mapper %s. Mapper failed to specify "
            "a valid sharding ID for a must epoch operation in control "
            "replicated context of task %s (UID %lld).",
            mapper->get_mapper_name(), repl_ctx->get_task_name(),
            repl_ctx->get_unique_id())
      this->sharding_functor = sharding_output.chosen_functor;
      this->collective_map_must_epoch_call = 
        sharding_output.collective_map_must_epoch_call;
#ifdef DEBUG_LEGION
      assert(sharding_function == NULL);
      // Check that the sharding IDs are all the same
      assert(sharding_collective != NULL);
      // Contribute the result
      sharding_collective->contribute(this->sharding_functor);
      if (sharding_collective->is_target() && 
          !sharding_collective->validate(this->sharding_functor))
      {
        log_run.error("ERROR: Mapper %s chose different sharding functions "
                      "for must epoch launch in %s (UID %lld)", 
                      mapper->get_mapper_name(), parent_ctx->get_task_name(),
                      parent_ctx->get_unique_id());
        assert(false); 
      }
      ReplFutureMapImpl *impl = 
          dynamic_cast<ReplFutureMapImpl*>(result_map.impl);
      assert(impl != NULL);
#else
      ReplFutureMapImpl *impl = 
          static_cast<ReplFutureMapImpl*>(result_map.impl);
#endif
      // Set the future map sharding functor
      sharding_function = 
        repl_ctx->shard_manager->find_sharding_function(sharding_functor);
      impl->set_sharding_function(sharding_function);
      // Set the sharding functor for all the point and index tasks too
      for (unsigned idx = 0; idx < indiv_tasks.size(); idx++)
      {
        ReplIndividualTask *task = 
          static_cast<ReplIndividualTask*>(indiv_tasks[idx]);
        task->set_sharding_function(sharding_functor, sharding_function);
      }
      for (unsigned idx = 0; idx < index_tasks.size(); idx++)
      {
        ReplIndexTask *task = static_cast<ReplIndexTask*>(index_tasks[idx]);
        task->set_sharding_function(sharding_functor, sharding_function);
      }
    }

    //--------------------------------------------------------------------------
    void ReplMustEpochOp::trigger_commit(void)
    //--------------------------------------------------------------------------
    {
      // We have to delete these here to make sure that they are
      // unregistered with the context before the context is deleted
      if (mapping_broadcast != NULL)
        delete mapping_broadcast;
      if (mapping_exchange != NULL)
        delete mapping_exchange;
      if (dependence_exchange != NULL)
        delete dependence_exchange;
      if (completion_exchange != NULL)
        delete completion_exchange;
#ifdef DEBUG_LEGION
      if (sharding_collective != NULL)
        delete sharding_collective;
#endif
      MustEpochOp::trigger_commit();
    }

    //--------------------------------------------------------------------------
    void ReplMustEpochOp::receive_resources(size_t return_index,
              std::map<LogicalRegion,unsigned> &created_regs,
              std::vector<DeletedRegion> &deleted_regs,
              std::set<std::pair<FieldSpace,FieldID> > &created_fids,
              std::vector<DeletedField> &deleted_fids,
              std::map<FieldSpace,unsigned> &created_fs,
              std::map<FieldSpace,std::set<LogicalRegion> > &latent_fs,
              std::vector<DeletedFieldSpace> &deleted_fs,
              std::map<IndexSpace,unsigned> &created_is,
              std::vector<DeletedIndexSpace> &deleted_is,
              std::map<IndexPartition,unsigned> &created_partitions,
              std::vector<DeletedPartition> &deleted_partitions,
              std::set<RtEvent> &preconditions)
    //--------------------------------------------------------------------------
    {
      // Wait until we've received all the resources before handing them
      // back to the enclosing parent context
      {
        AutoLock o_lock(op_lock);
        merge_received_resources(created_regs, deleted_regs, created_fids, 
            deleted_fids, created_fs, latent_fs, deleted_fs, created_is,
            deleted_is, created_partitions, deleted_partitions);
#ifdef DEBUG_LEGION
        assert(remaining_resource_returns > 0);
#endif
        if (--remaining_resource_returns > 0)
          return;
      }
      // Make sure the other shards have received all their returns too
      Runtime::phase_barrier_arrive(resource_return_barrier, 1/*count*/);
      if (!has_return_resources())
        return;
      if (!resource_return_barrier.has_triggered())
      {
        DeferMustEpochReturnResourcesArgs args(this);
        runtime->issue_runtime_meta_task(args,
            LG_THROUGHPUT_DEFERRED_PRIORITY, resource_return_barrier);
        preconditions.insert(args.done);
        return;
      }
      // If we get here then we can finally do the return to the parent context
      // because we've received resources from all of our constituent operations
      return_resources(parent_ctx, context_index, preconditions);
    }

    //--------------------------------------------------------------------------
    /*static*/ void ReplMustEpochOp::handle_defer_return_resources(
                                                               const void *args)
    //--------------------------------------------------------------------------
    {
      const DeferMustEpochReturnResourcesArgs *dargs =
        (const DeferMustEpochReturnResourcesArgs*)args;
      std::set<RtEvent> preconditions;
      dargs->op->return_resources(dargs->op->get_context(),
          dargs->op->get_context_index(), preconditions);
      if (!preconditions.empty())
        Runtime::trigger_event(dargs->done,
            Runtime::merge_events(preconditions));
      else
        Runtime::trigger_event(dargs->done);
    }

    //--------------------------------------------------------------------------
    void ReplMustEpochOp::map_replicate_tasks(void) const
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(dependence_exchange != NULL);
      assert(single_tasks.size() == mapping_dependences.size());
#endif
      std::map<DomainPoint,RtUserEvent> mapped_events;
      for (std::set<SingleTask*>::const_iterator it = 
            shard_single_tasks.begin(); it != shard_single_tasks.end(); it++)
        mapped_events[(*it)->index_point] = Runtime::create_rt_user_event();
      // Now exchange completion events for the point tasks we own
      // and end up with a set of the completion event for each task
      // First compute the set of mapped events for the points that we own
      dependence_exchange->exchange_must_epoch_dependences(mapped_events);

      MustEpochMapArgs args(const_cast<ReplMustEpochOp*>(this));
      std::set<RtEvent> local_mapped_events;
      // For correctness we still have to abide by the mapping dependences
      // computed on the individual tasks while we are mapping them
      for (unsigned idx = 0; idx < single_tasks.size(); idx++)
      {
        // Check to see if it is one of the ones that we own
        if (shard_single_tasks.find(single_tasks[idx]) == 
            shard_single_tasks.end())
        {
          // We don't own this point
          // We still need to do some work for individual tasks
          // to exchange versioning information, but no such 
          // work is necessary for point tasks
          SingleTask *task = single_tasks[idx];
          task->shard_off(mapped_events[task->index_point]);
          continue;
        }
        // Figure out our preconditions
        std::set<RtEvent> preconditions;
        for (std::set<unsigned>::const_iterator it = 
              mapping_dependences[idx].begin(); it != 
              mapping_dependences[idx].end(); it++)
        {
#ifdef DEBUG_LEGION
          assert((*it) < idx);
#endif
          preconditions.insert(mapped_events[single_tasks[*it]->index_point]);
        }
        args.task = single_tasks[idx];
        RtEvent done;
        if (!preconditions.empty())
        {
          RtEvent precondition = Runtime::merge_events(preconditions);
          done = runtime->issue_runtime_meta_task(args, 
                LG_THROUGHPUT_DEFERRED_PRIORITY, precondition); 
        }
        else
          done = runtime->issue_runtime_meta_task(args, 
                      LG_THROUGHPUT_DEFERRED_PRIORITY);
        local_mapped_events.insert(done);
        // We can trigger our completion event once the task is done
        RtUserEvent mapped = mapped_events[single_tasks[idx]->index_point];
        Runtime::trigger_event(mapped, done);
      }
      // Now we have to wait for all our mapping operations to be done
      if (!local_mapped_events.empty())
      {
        RtEvent mapped_event = Runtime::merge_events(local_mapped_events);
        mapped_event.wait();
      }
    }

    //--------------------------------------------------------------------------
    void ReplMustEpochOp::distribute_replicate_tasks(void)
    //--------------------------------------------------------------------------
    {
      // We only want to distribute the points that are owned by our shard
      ReplMustEpochOp *owner = const_cast<ReplMustEpochOp*>(this);
      MustEpochDistributorArgs dist_args(owner);
      MustEpochLauncherArgs launch_args(owner);
      std::set<RtEvent> wait_events;
      // Count how many resource returns we expect to see as part of this
      for (std::vector<IndividualTask*>::const_iterator it = 
            indiv_tasks.begin(); it != indiv_tasks.end(); it++)
      {
        // Skip any points that we do not own on this shard
        if (shard_single_tasks.find(*it) == shard_single_tasks.end())
          continue;
        remaining_resource_returns++;
        if (!runtime->is_local((*it)->target_proc))
        {
          dist_args.task = *it;
          RtEvent wait = 
            runtime->issue_runtime_meta_task(dist_args, 
                LG_THROUGHPUT_DEFERRED_PRIORITY);
          if (wait.exists())
            wait_events.insert(wait);
        }
        else
        {
          launch_args.task = *it;
          RtEvent wait = 
            runtime->issue_runtime_meta_task(launch_args,
                  LG_THROUGHPUT_DEFERRED_PRIORITY);
          if (wait.exists())
            wait_events.insert(wait);
        }
      }
      for (std::set<SliceTask*>::const_iterator it = 
            slice_tasks.begin(); it != slice_tasks.end(); it++)
      {
        // Check to see if we either do or not own this slice
        // We currently do not support mixed slices for which
        // we only own some of the points
        bool contains_any = false;
        bool contains_all = true;
        for (std::vector<PointTask*>::const_iterator pit = 
              (*it)->points.begin(); pit != (*it)->points.end(); pit++)
        {
          if (shard_single_tasks.find(*pit) != shard_single_tasks.end())
            contains_any = true;
          else if (contains_all)
          {
            contains_all = false;
            if (contains_any) // At this point we have all the answers
              break;
          }
        }
        if (!contains_any)
          continue;
        if (!contains_all)
        {
          Processor mapper_proc = parent_ctx->get_executing_processor();
          MapperManager *mapper = runtime->find_mapper(mapper_proc, map_id);
          REPORT_LEGION_FATAL(ERROR_INVALID_MAPPER_OUTPUT,
                              "Mapper %s specified a slice for a must epoch "
                              "launch in control replicated task %s "
                              "(UID %lld) for which not all the points "
                              "mapped to the same shard. Legion does not "
                              "currently support this use case. Please "
                              "specify slices and a sharding function to "
                              "ensure that all the points in a slice are "
                              "owned by the same shard", 
                              mapper->get_mapper_name(),
                              parent_ctx->get_task_name(),
                              parent_ctx->get_unique_id())
        }
        remaining_resource_returns++;
        (*it)->update_target_processor();
        if (!runtime->is_local((*it)->target_proc))
        {
          dist_args.task = *it;
          RtEvent wait = 
            runtime->issue_runtime_meta_task(dist_args, 
                LG_THROUGHPUT_DEFERRED_PRIORITY);
          if (wait.exists())
            wait_events.insert(wait);
        }
        else
        {
          launch_args.task = *it;
          RtEvent wait = 
            runtime->issue_runtime_meta_task(launch_args,
                 LG_THROUGHPUT_DEFERRED_PRIORITY);
          if (wait.exists())
            wait_events.insert(wait);
        }
      }
      // Trigger this if we're not expecting to see any returns
      if (remaining_resource_returns == 0)
        Runtime::phase_barrier_arrive(resource_return_barrier, 1/*count*/);
      if (!wait_events.empty())
      {
        RtEvent dist_event = Runtime::merge_events(wait_events);
        dist_event.wait();
      }
    }

    //--------------------------------------------------------------------------
    void ReplMustEpochOp::initialize_replication(ReplicateContext *ctx)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(mapping_collective_id == 0);
      assert(mapping_broadcast == NULL);
      assert(mapping_exchange == NULL);
      assert(dependence_exchange == NULL);
      assert(completion_exchange == NULL);
#endif
      // We can't actually make a collective for the mapping yet because we 
      // don't know if we are going to broadcast or exchange so we just get
      // a collective ID that we will use later 
      mapping_collective_id = ctx->get_next_collective_index(COLLECTIVE_LOC_58);
      dependence_exchange = 
        new MustEpochDependenceExchange(ctx, COLLECTIVE_LOC_70);
      completion_exchange = 
        new MustEpochCompletionExchange(ctx, COLLECTIVE_LOC_73);
      resource_return_barrier = ctx->get_next_resource_return_barrier();
      concurrent_prebar = ctx->get_next_concurrent_precondition_barrier();
      concurrent_postbar = ctx->get_next_concurrent_postcondition_barrier();
    }

    //--------------------------------------------------------------------------
    Domain ReplMustEpochOp::get_shard_domain(void) const
    //--------------------------------------------------------------------------
    {
      if (sharding_space.exists())
      {
        Domain shard_domain;
        runtime->forest->find_launch_space_domain(sharding_space, shard_domain);
        return shard_domain;
      }
      else
        return launch_domain;
    }

    //--------------------------------------------------------------------------
    size_t ReplMustEpochOp::count_shard_local_points(IndexSpaceNode *domain)
    //--------------------------------------------------------------------------
    {
      // No need for the lock here, the shard_single_tasks shouldn't be
      // changing anymore when we get here
      size_t result = 0;
      for (std::set<SingleTask*>::const_iterator it =
            shard_single_tasks.begin(); it != shard_single_tasks.end(); it++)
        if (domain->contains_point((*it)->index_point))
          result++;
      return result;
    }

    /////////////////////////////////////////////////////////////
    // Repl Timing Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplTimingOp::ReplTimingOp(Runtime *rt)
      : TimingOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTimingOp::ReplTimingOp(const ReplTimingOp &rhs)
      : TimingOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplTimingOp::~ReplTimingOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTimingOp& ReplTimingOp::operator=(const ReplTimingOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplTimingOp::activate(void)
    //--------------------------------------------------------------------------
    {
      TimingOp::activate();
      timing_collective = NULL;
    }

    //--------------------------------------------------------------------------
    void ReplTimingOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      TimingOp::deactivate(false/*freeop*/);
      if (timing_collective != NULL)
      {
        delete timing_collective;
        timing_collective = NULL;
      }
      if (freeop)
        runtime->free_repl_timing_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplTimingOp::trigger_mapping(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Shard 0 will handle the timing operation so do the normal mapping
      if (repl_ctx->owner_shard->shard_id > 0)
      {
        complete_mapping();
        RtEvent result_ready = 
          timing_collective->perform_collective_wait(false/*block*/);
        if (result_ready.exists() && !result_ready.has_triggered())
          parent_ctx->add_to_trigger_execution_queue(this, result_ready);
        else
          trigger_execution();
      }
      else // Shard 0 does the normal timing operation
        TimingOp::trigger_mapping();
    } 

    //--------------------------------------------------------------------------
    void ReplTimingOp::trigger_execution(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Shard 0 will handle the timing operation
      if (repl_ctx->owner_shard->shard_id > 0)     
      {
        long long value = timing_collective->get_value(false/*already waited*/);
        result.impl->set_local(&value, sizeof(value));
      }
      else
      {
        // Perform the measurement and then arrive on the barrier
        // with the result to broadcast it to the other shards
        switch (measurement)
        {
          case LEGION_MEASURE_SECONDS:
            {
              double value = Realm::Clock::current_time();
              result.impl->set_local(&value, sizeof(value));
              long long alt_value = 0;
              static_assert(sizeof(alt_value) == sizeof(value), "Fuck c++");
              memcpy(&alt_value, &value, sizeof(value));
              timing_collective->broadcast(alt_value);
              break;
            }
          case LEGION_MEASURE_MICRO_SECONDS:
            {
              long long value = Realm::Clock::current_time_in_microseconds();
              result.impl->set_local(&value, sizeof(value));
              timing_collective->broadcast(value);
              break;
            }
          case LEGION_MEASURE_NANO_SECONDS:
            {
              long long value = Realm::Clock::current_time_in_nanoseconds();
              result.impl->set_local(&value, sizeof(value));
              timing_collective->broadcast(value);
              break;
            }
          default:
            assert(false); // should never get here
        }
      }
#ifdef LEGION_SPY
      // Still have to do this call to let Legion Spy know we're done
      LegionSpy::log_operation_events(unique_op_id, ApEvent::NO_AP_EVENT,
                                      ApEvent::NO_AP_EVENT);
#endif
      complete_execution();
    }

    /////////////////////////////////////////////////////////////
    // Repl Tunable Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplTunableOp::ReplTunableOp(Runtime *rt)
      : TunableOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTunableOp::ReplTunableOp(const ReplTunableOp &rhs)
      : TunableOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplTunableOp::~ReplTunableOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTunableOp& ReplTunableOp::operator=(const ReplTunableOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplTunableOp::activate(void)
    //--------------------------------------------------------------------------
    {
      TunableOp::activate();
      value_broadcast = NULL;
    }

    //--------------------------------------------------------------------------
    void ReplTunableOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      TunableOp::deactivate();
      if (value_broadcast != NULL)
      {
        delete value_broadcast;
        value_broadcast = NULL;
      }
      if (freeop)
        runtime->free_repl_tunable_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplTunableOp::initialize_replication(ReplicateContext *repl_ctx)
    //--------------------------------------------------------------------------
    {
      if (!runtime->unsafe_mapper)
      {
#ifdef DEBUG_LEGION
        assert(value_broadcast == NULL);
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        // We'll always make node zero the owner shard here
        if (repl_ctx->owner_shard->shard_id > 0)
          value_broadcast = new BufferBroadcast(repl_ctx, 0/*owner shard*/,
                                                COLLECTIVE_LOC_100);
        else
          value_broadcast = new BufferBroadcast(repl_ctx, COLLECTIVE_LOC_100);
      }
    }

    //--------------------------------------------------------------------------
    void ReplTunableOp::process_result(MapperManager *mapper, 
                                       void *buffer, size_t size) const
    //--------------------------------------------------------------------------
    {
      if (!runtime->unsafe_mapper)
      {
#ifdef DEBUG_LEGION
        assert(value_broadcast != NULL);
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        if (repl_ctx->owner_shard->shard_id != value_broadcast->origin)
        {
          size_t expected_size = 0;
          const void *expected_buffer =
            value_broadcast->get_buffer(expected_size);
          if ((expected_size != size) ||
              (memcmp(buffer, expected_buffer, size) != 0))
            REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                "Mapper %s returned different values for selection of "
                "tunable value %d in parent task %s (UID %lld)",
                mapper->get_mapper_name(), tunable_id,
                parent_ctx->get_task_name(), parent_ctx->get_unique_id())
        }
        else
          value_broadcast->broadcast(buffer, size);
      }
    }

    /////////////////////////////////////////////////////////////
    // Repl All Reduce Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplAllReduceOp::ReplAllReduceOp(Runtime *rt)
      : AllReduceOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplAllReduceOp::ReplAllReduceOp(const ReplAllReduceOp &rhs)
      : AllReduceOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplAllReduceOp::~ReplAllReduceOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplAllReduceOp& ReplAllReduceOp::operator=(const ReplAllReduceOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplAllReduceOp::initialize_replication(ReplicateContext *ctx)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(redop != NULL);
      assert(serdez_redop_collective == NULL);
      assert(all_reduce_collective == NULL);
#endif
      if (serdez_redop_fns != NULL)
        serdez_redop_collective = new BufferExchange(ctx, COLLECTIVE_LOC_97);
      else
        all_reduce_collective = new FutureAllReduceCollective(this,
            COLLECTIVE_LOC_97, ctx, redop_id, redop, deterministic);
    }

    //--------------------------------------------------------------------------
    void ReplAllReduceOp::activate(void)
    //--------------------------------------------------------------------------
    {
      AllReduceOp::activate();
      serdez_redop_collective = NULL;
      all_reduce_collective = NULL;
    }

    //--------------------------------------------------------------------------
    void ReplAllReduceOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      AllReduceOp::deactivate(false/*free*/);
      if (serdez_redop_collective != NULL)
        delete serdez_redop_collective;
      if (all_reduce_collective != NULL)
        delete all_reduce_collective;
      if (freeop)
        runtime->free_repl_all_reduce_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplAllReduceOp::populate_sources(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(sources.empty());
#endif
      future_map.impl->get_shard_local_futures(sources);
    }

    //--------------------------------------------------------------------------
    void ReplAllReduceOp::create_future_instances(
                                           std::vector<Memory> &target_memories)
    //--------------------------------------------------------------------------
    {
      // Do the base call first
      AllReduceOp::create_future_instances(target_memories);
      // Now check to see if we need to make a shadow instance for
      // the all-reduce future collective
      if (all_reduce_collective != NULL)
      {
#ifdef DEBUG_LEGION
        assert(!targets.empty());
#endif
        FutureInstance *target = targets.front();
        // If the instance is in a memory we cannot see or is "too big"
        // then we need to make the shadow instance for the future
        // all-reduce collective to use now while still in the mapping stage
        if ((!target->is_meta_visible) ||
            (target->size > LEGION_MAX_RETURN_SIZE))
        {
          MemoryManager *manager = runtime->find_memory_manager(target->memory);
          FutureInstance *shadow_instance = 
            manager->create_future_instance(this, unique_op_id,
                get_completion_event(), redop->sizeof_rhs, false/*eager*/);
          all_reduce_collective->set_shadow_instance(shadow_instance);
        }
      }
    }

    //--------------------------------------------------------------------------
    void ReplAllReduceOp::all_reduce_serdez(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(serdez_redop_fns != NULL);
#endif
      for (std::map<DomainPoint,Future>::const_iterator it = 
            sources.begin(); it != sources.end(); it++)
      {
        FutureImpl *impl = it->second.impl;
        size_t src_size = 0;
        const void *source = impl->find_internal_buffer(parent_ctx, src_size);
        (*(serdez_redop_fns->fold_fn))(redop, serdez_redop_buffer, 
                                       future_result_size, source);
        if (runtime->legion_spy_enabled)
        {
          const ApEvent ready_event = impl->get_ready_event();
          if (ready_event.exists())
            LegionSpy::log_future_use(unique_op_id, ready_event);
        }
      }
      // Now we need an all-to-all to get the values from other shards
      const std::map<ShardID,std::pair<void*,size_t> > &remote_buffers =
        serdez_redop_collective->exchange_buffers(serdez_redop_buffer,
                                    future_result_size, deterministic);
      if (deterministic)
      {
        // Reset this back to empty so we can reduce in order across shards
        // Note the serdez_redop_collective took ownership of deleting
        // the buffer in this case so we know that it is not leaking
        serdez_redop_buffer = NULL;
        for (std::map<ShardID,std::pair<void*,size_t> >::const_iterator it =
              remote_buffers.begin(); it != remote_buffers.end(); it++)
        {
          if (serdez_redop_buffer == NULL)
          {
            future_result_size = it->second.second;
            serdez_redop_buffer = malloc(future_result_size);
            memcpy(serdez_redop_buffer, it->second.first, future_result_size);
          }
          else
            (*(serdez_redop_fns->fold_fn))(redop, serdez_redop_buffer,
                                future_result_size, it->second.first);
        }
      }
      else
      {
        for (std::map<ShardID,std::pair<void*,size_t> >::const_iterator it =
              remote_buffers.begin(); it != remote_buffers.end(); it++)
        {
#ifdef DEBUG_LEGION
          assert(it->first != serdez_redop_collective->local_shard);
#endif
          (*(serdez_redop_fns->fold_fn))(redop, serdez_redop_buffer,
                              future_result_size, it->second.first);
        }
      }
    }

    //--------------------------------------------------------------------------
    RtEvent ReplAllReduceOp::all_reduce_redop(void)
    //--------------------------------------------------------------------------
    {
      std::vector<FutureInstance*> instances;
      instances.reserve(sources.size());
      for (std::map<DomainPoint,Future>::const_iterator it = 
            sources.begin(); it != sources.end(); it++)
      {
        FutureImpl *impl = it->second.impl;
        FutureInstance *instance = impl->get_canonical_instance();
        if (instance->size != redop->sizeof_rhs)
          REPORT_LEGION_ERROR(ERROR_FUTURE_MAP_REDOP_TYPE_MISMATCH,
              "Future in future map reduction in task %s (UID %lld) does not "
              "have the right input size for the given reduction operator. "
              "Future has size %zd bytes but reduction operator expects "
              "RHS inputs of %zd bytes.", parent_ctx->get_task_name(),
              parent_ctx->get_unique_id(), instance->size, redop->sizeof_rhs)
        instances.push_back(instance);
        if (runtime->legion_spy_enabled)
        {
          const ApEvent ready_event = impl->get_ready_event();
          if (ready_event.exists())
            LegionSpy::log_future_use(unique_op_id, ready_event);
        }
      }
#ifdef DEBUG_LEGION
      assert(!targets.empty());
#endif
      // We're going to need to do an all-reduce between the shards so
      // we'll just do our local reductions into the first target initially
      // and then we'll broadcast the result to the targets afterwards
      FutureInstance *local_target = targets.front();
      ApEvent local_precondition = local_target->initialize(redop, this);
      if (deterministic)
      {
        for (std::vector<FutureInstance*>::const_iterator it =
              instances.begin(); it != instances.end(); it++)
          local_precondition = local_target->reduce_from(*it, this, redop_id,
                                redop, true/*exclusive*/, local_precondition);
      }
      else
      {
        std::set<ApEvent> postconditions;
        for (std::vector<FutureInstance*>::const_iterator it =
              instances.begin(); it != instances.end(); it++)
        {
          const ApEvent postcondition = local_target->reduce_from(*it, this,
                    redop_id, redop, false/*exclusive*/, local_precondition);
          if (postcondition.exists())
            postconditions.insert(postcondition);
        }
        if (!postconditions.empty())
          local_precondition = Runtime::merge_events(NULL, postconditions);
      }
      const RtEvent collective_done =
       all_reduce_collective->async_reduce(targets.front(), local_precondition);
      // Finally do the copy out to all the other targets
      if (targets.size() > 1)
      {
        std::vector<ApEvent> broadcast_events(targets.size());
        broadcast_events[0] = local_precondition;
        broadcast_events[1] =
          targets[1]->copy_from(local_target, this, broadcast_events[0]);
        for (unsigned idx = 1; idx < targets.size(); idx++)
        {
          if (targets.size() <= (2*idx))
            break;
          broadcast_events[2*idx] = 
           targets[2*idx]->copy_from(targets[idx], this, broadcast_events[idx]);
          if (targets.size() <= (2*idx+1))
            break;
          broadcast_events[2*idx+1] =
           targets[2*idx+1]->copy_from(targets[idx],this,broadcast_events[idx]);
        }
        std::set<ApEvent> postconditions;
        for (std::vector<ApEvent>::const_iterator it =
              broadcast_events.begin(); it != broadcast_events.end(); it++)
          if (it->exists())
            postconditions.insert(*it);
        if (!postconditions.empty())
          local_precondition = Runtime::merge_events(NULL, postconditions);
      }
      record_completion_effect(local_precondition);
      return collective_done;
    }

    /////////////////////////////////////////////////////////////
    // Repl Fence Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplFenceOp::ReplFenceOp(Runtime *rt)
      : FenceOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplFenceOp::ReplFenceOp(const ReplFenceOp &rhs)
      : FenceOp(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplFenceOp::~ReplFenceOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplFenceOp& ReplFenceOp::operator=(const ReplFenceOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplFenceOp::activate(void)
    //--------------------------------------------------------------------------
    {
      FenceOp::activate();
      mapping_fence_barrier = RtBarrier::NO_RT_BARRIER;
      execution_fence_barrier = ApBarrier::NO_AP_BARRIER;
    }

    //--------------------------------------------------------------------------
    void ReplFenceOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      FenceOp::deactivate(false/*free*/);
      if (freeop)
        runtime->free_repl_fence_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplFenceOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
      initialize_fence_barriers();
      FenceOp::trigger_dependence_analysis();
    }

    //--------------------------------------------------------------------------
    void ReplFenceOp::initialize_fence_barriers(ReplicateContext *repl_ctx)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!mapping_fence_barrier.exists());
      assert(!execution_fence_barrier.exists());
      if (repl_ctx == NULL)
        repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      if (repl_ctx == NULL)
        repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // If we get here that means we weren't replayed so make our fences
      mapping_fence_barrier = repl_ctx->get_next_mapping_fence_barrier();
      if (fence_kind == EXECUTION_FENCE)
        execution_fence_barrier = repl_ctx->get_next_execution_fence_barrier();
    }

    //--------------------------------------------------------------------------
    void ReplFenceOp::trigger_mapping(void)
    //--------------------------------------------------------------------------
    {
      switch (fence_kind)
      {
        case MAPPING_FENCE:
          {
            // Do our arrival
            if (!map_applied_conditions.empty())
              Runtime::phase_barrier_arrive(mapping_fence_barrier, 1/*count*/,
                  Runtime::merge_events(map_applied_conditions));
            else
              Runtime::phase_barrier_arrive(mapping_fence_barrier, 1/*count*/);
            // We're mapped when everyone is mapped
            complete_mapping(mapping_fence_barrier);
            complete_execution();
            break;
          }
        case EXECUTION_FENCE:
          {
            // If we're recording find all the prior event dependences
            if (is_recording())
              tpl->find_execution_fence_preconditions(execution_preconditions);
            const PhysicalTraceInfo trace_info(this, 0/*index*/);
            // We arrive on our barrier when all our previous operations
            // have finished executing
            ApEvent execution_fence_precondition;
            if (!execution_preconditions.empty())
              execution_fence_precondition = 
                  Runtime::merge_events(&trace_info, execution_preconditions);
            Runtime::phase_barrier_arrive(execution_fence_barrier, 1/*count*/, 
                                          execution_fence_precondition);
            // Do our arrival on our mapping fence, we're mapped when
            // everyone is mapped
            if (!map_applied_conditions.empty())
              Runtime::phase_barrier_arrive(mapping_fence_barrier, 1/*count*/,
                  record_complete_replay(trace_info, 
                    Runtime::merge_events(map_applied_conditions)));
            else
              Runtime::phase_barrier_arrive(mapping_fence_barrier, 1/*count*/,
                  record_complete_replay(trace_info));
            complete_mapping(mapping_fence_barrier);
            // We can always trigger the completion event when these are done
            record_completion_effect(execution_fence_barrier);
            complete_execution();
            break;
          }
        default:
          assert(false); // should never get here
      }
    }

    //--------------------------------------------------------------------------
    void ReplFenceOp::trigger_replay(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!mapping_fence_barrier.exists());
      assert(!execution_fence_barrier.exists());
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // Get ourselves an execution fence barrier
      // No need for a mapping fence since we're just replaying
      if (fence_kind == EXECUTION_FENCE)
        execution_fence_barrier = repl_ctx->get_next_execution_fence_barrier();
      FenceOp::trigger_replay();
    }

    //--------------------------------------------------------------------------
    void ReplFenceOp::complete_replay(ApEvent pre, ApEvent complete_event)
    //--------------------------------------------------------------------------
    {
      Runtime::phase_barrier_arrive(execution_fence_barrier, 
                                    1/*count*/, complete_event);
      FenceOp::complete_replay(pre, execution_fence_barrier);
    }

    /////////////////////////////////////////////////////////////
    // Repl Map Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplMapOp::ReplMapOp(Runtime *rt)
      : ReplCollectiveViewCreator<CollectiveViewCreator<MapOp> >(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplMapOp::ReplMapOp(const ReplMapOp &rhs)
      : ReplCollectiveViewCreator<CollectiveViewCreator<MapOp> >(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplMapOp::~ReplMapOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplMapOp& ReplMapOp::operator=(const ReplMapOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplMapOp::initialize_replication(ReplicateContext *ctx) 
    //--------------------------------------------------------------------------
    {
      if (!remap_region && !runtime->unsafe_mapper)
      {
        mapping_check = ctx->get_next_collective_index(COLLECTIVE_LOC_74);
        sources_check = ctx->get_next_collective_index(COLLECTIVE_LOC_104);
      }
      if (!grants.empty())
        REPORT_LEGION_ERROR(ERROR_CONTROL_REPLICATION_VIOLATION,
            "Illegal use of grants with an inline mapping in control "
            "replicated parent task %s (UID %lld). Use of non-canonical "
            "Legion features such as grants are not permitted with "
            "control replication.", parent_ctx->get_task_name(),
            parent_ctx->get_unique_id())
      if (!wait_barriers.empty())
        REPORT_LEGION_ERROR(ERROR_CONTROL_REPLICATION_VIOLATION,
            "Illegal use of wait phase barriers with an inline mapping in "
            "control replicated parent task %s (UID %lld). Use of "
            "non-canonical Legion features such as wait phase barriers are "
            "not permitted with control replication.",
            parent_ctx->get_task_name(), parent_ctx->get_unique_id())
      if (!arrive_barriers.empty())
        REPORT_LEGION_ERROR(ERROR_CONTROL_REPLICATION_VIOLATION,
            "Illegal use of arrive phase barriers with an inline mapping in "
            "control replicated parent task %s (UID %lld). Use of "
            "non-canonical Legion features such as arrive phase barriers are "
            "not permitted with control replication.",
            parent_ctx->get_task_name(), parent_ctx->get_unique_id())
    }

    //--------------------------------------------------------------------------
    void ReplMapOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
      if (runtime->check_privileges)
        check_privilege();
#ifdef DEBUG_LEGION
      assert(requirement.handle_type == LEGION_SINGULAR_PROJECTION);
#endif
      ProjectionInfo projection_info;
      RefinementTracker tracker(this, map_applied_conditions);
      runtime->forest->perform_dependence_analysis(this, 0/*idx*/, 
                                                   requirement,
                                                   projection_info,
                                                   privilege_path, tracker,
                                                   map_applied_conditions);
      // If this a write requirement then we need to perform syncs on the
      // way in and the way out of the physical analysis across the shards
      // to ensure we don't do any exclusive updates out of order
      if (IS_WRITE(requirement))
      {
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx =dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
        assert(!collective_map_barrier.exists());
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        collective_map_barrier = repl_ctx->get_next_collective_map_barriers();
      }
      // We're always going to do collective rendezvous for this requirement
      create_collective_view_rendezvous(requirement.parent.get_tree_id(), 0);
    }

    //--------------------------------------------------------------------------
    void ReplMapOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
      // Signal that all our mapping dependences have been met
      if (collective_map_barrier.exists())
        Runtime::phase_barrier_arrive(collective_map_barrier, 1/*count*/);
      std::set<RtEvent> preconditions;
      // Compute the version numbers for this mapping operation
      runtime->forest->perform_versioning_analysis(this, 0/*idx*/,
                                                   requirement, 
                                                   version_info,
                                                   preconditions);
      if (collective_map_barrier.exists())
      {
        if (!collective_map_barrier.has_triggered())
          preconditions.insert(collective_map_barrier);
        Runtime::advance_barrier(collective_map_barrier);
      }
      if (!preconditions.empty())
        enqueue_ready_operation(Runtime::merge_events(preconditions));
      else
        enqueue_ready_operation();
    }

    //--------------------------------------------------------------------------
    bool ReplMapOp::invoke_mapper(InstanceSet &mapped_instances,
                                std::vector<PhysicalManager*> &source_instances)
    //--------------------------------------------------------------------------
    {
      const bool result = 
        MapOp::invoke_mapper(mapped_instances, source_instances);
      if (!runtime->unsafe_mapper)
      {
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx =
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        // For read-write or write-discard cases make sure that all the 
        // shards mapped to independent physical instances
        if (IS_WRITE(requirement))
        {
          CheckCollectiveMapping mapping_collective(repl_ctx, mapping_check);
          mapping_collective.verify(mapped_instances, mapper);
        }
        // For anything that is not a reduce inline mapping we check that
        // the names of the sources are the same across all the shards
        if (!IS_REDUCE(requirement))
        {
          CheckCollectiveSources sources_collective(repl_ctx, sources_check);
          if (!sources_collective.verify(source_instances))
            REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                  "Invalid mapper output from invocation of 'map_inline' "
                  "by mapper %s. Mapper selected different 'source_instances' "
                  "on shard 0 and shard %d when mapping an inline mapping in "
                  "control-replicated parent task %s (UID %lld). Each inline "
                  "mapping in a control-replicated parent task must provide "
                  "same 'source_instances' across all shards.",
                  mapper->get_mapper_name(), repl_ctx->owner_shard->shard_id,
                  parent_ctx->get_task_name(), parent_ctx->get_unique_id())
        }
      }
      return result;
    }

    //--------------------------------------------------------------------------
    RtEvent ReplMapOp::finalize_complete_mapping(RtEvent precondition)
    //--------------------------------------------------------------------------
    {
      if (collective_map_barrier.exists())
      {
        Runtime::phase_barrier_arrive(collective_map_barrier, 1/*count*/,
                                      precondition);
        const RtEvent result = collective_map_barrier;
#ifdef DEBUG_LEGION
        collective_map_barrier = RtBarrier::NO_RT_BARRIER;
#endif
        return result;
      }
      else
        return precondition;
    }

    //--------------------------------------------------------------------------
    bool ReplMapOp::perform_collective_analysis(CollectiveMapping *&mapping,
                                                bool &first_local)
    //--------------------------------------------------------------------------
    {
      // Yes, we want to do a collective analysis, but we'll need to 
      // construct a collective view here for all the instances
      return true;
    }

    //--------------------------------------------------------------------------
    bool ReplMapOp::find_shard_participants(std::vector<ShardID> &shards)
    //--------------------------------------------------------------------------
    {
      // All the shards are participating
      return true;
    }

    //--------------------------------------------------------------------------
    void ReplMapOp::activate(void)
    //--------------------------------------------------------------------------
    {
      ReplCollectiveViewCreator<CollectiveViewCreator<MapOp> >::activate();
      mapping_check = 0;
      sources_check = 0;
      collective_map_barrier = RtBarrier::NO_RT_BARRIER;
    }

    //--------------------------------------------------------------------------
    void ReplMapOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      // Make sure that we consumed this if we had one
      assert(!collective_map_barrier.exists());
#endif
      ReplCollectiveViewCreator<
        CollectiveViewCreator<MapOp> >::deactivate(false);
      if (freeop)
        runtime->free_repl_map_op(this);
    }

    /////////////////////////////////////////////////////////////
    // Repl Attach Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplAttachOp::ReplAttachOp(Runtime *rt)
      : ReplCollectiveViewCreator<CollectiveViewCreator<AttachOp> >(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplAttachOp::ReplAttachOp(const ReplAttachOp &rhs)
      : ReplCollectiveViewCreator<CollectiveViewCreator<AttachOp> >(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplAttachOp::~ReplAttachOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplAttachOp& ReplAttachOp::operator=(const ReplAttachOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplAttachOp::initialize_replication(ReplicateContext *ctx,
                                              bool collective_inst,
                                              bool dedup_across_shards,
                                              bool first_local_shard)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(did_broadcast == NULL);
      assert(single_broadcast == NULL);
#endif
      resource_barrier = ctx->get_next_attach_resource_barrier();
      collective_instances = collective_inst;
      deduplicate_across_shards = dedup_across_shards;
      is_first_local_shard = first_local_shard;
      // Setup the distributed ID broadcast and send out the value
      if (collective_instances)
      {
        // Check to make sure that everything is local here
        if (resource == LEGION_EXTERNAL_INSTANCE)
        {
          const PointerConstraint &pointer =
                                  layout_constraint_set.pointer_constraint;
          const AddressSpaceID owner_space = pointer.memory.address_space();
          if (owner_space != runtime->address_space)
          {
            const char *mem_names[] = {
#define MEM_NAMES(name, desc) desc,
              REALM_MEMORY_KINDS(MEM_NAMES) 
#undef MEM_NAMES
            };
            REPORT_LEGION_ERROR(ERROR_NONLOCAL_COLLECTIVE_ATTACH,
                "Illegal collective attach operation performed by shard %d of "
                "control-replicated task %s (UID %lld). External instance in "
                "%s memory " IDFMT " of address space %d, but shard %d is "
                "executing on address space %d. All collective attach ops must "
                "attach to instances in the local address space.",
                ctx->owner_shard->shard_id, ctx->get_task_name(),
                ctx->get_unique_id(), mem_names[pointer.memory.kind()],
                pointer.memory.id, pointer.memory.address_space(), 
                ctx->owner_shard->shard_id, runtime->address_space)
          }
        }
      }
      else
      {
        // Figure out which shard should be the one to make the
        // owner manager and therefore the distributed ID
        ShardID owner_shard = 0; 
        switch (resource)
        {
          case LEGION_EXTERNAL_POSIX_FILE:
          case LEGION_EXTERNAL_HDF5_FILE:
            // always use shard 0 for files
            break;
          case LEGION_EXTERNAL_INSTANCE:
            {
              const PointerConstraint &pointer =
                                      layout_constraint_set.pointer_constraint;
              const AddressSpaceID owner_space = pointer.memory.address_space();
              const ShardMapping &mapping = ctx->shard_manager->get_mapping();
              for (ShardID sid = 0; sid < mapping.size(); sid++)
              {
                if (mapping[sid] != owner_space)
                  continue;
                owner_shard = sid;
                contains_individual = true;
                break;
              }
              // If we didn't find it we default to 0
              break;
            }
          default:
            break;
        }
        did_broadcast = 
          new ValueBroadcast<DistributedID>(ctx,owner_shard,COLLECTIVE_LOC_78);
        // Can only do the broadcast if we know we can make the ID safely
        // For external instances, if they are remote from all shards then
        // we'll need to create a remote manager with a remote distributed ID
        if (did_broadcast->is_origin() &&
            ((resource != LEGION_EXTERNAL_INSTANCE) || contains_individual))
          did_broadcast->broadcast(runtime->get_available_distributed_id());
        single_broadcast = new ValueBroadcast<InstanceEventPair>(
                                ctx, owner_shard, COLLECTIVE_LOC_75);
      }
    }

    //--------------------------------------------------------------------------
    void ReplAttachOp::activate(void)
    //--------------------------------------------------------------------------
    {
      ReplCollectiveViewCreator<CollectiveViewCreator<AttachOp> >::activate();
      collective_map_barrier = RtBarrier::NO_RT_BARRIER;
      exchange_index = 0;
      collective_instances = false;
      deduplicate_across_shards = false;
      is_first_local_shard = false;
      contains_individual = false;
      resource_barrier = RtBarrier::NO_RT_BARRIER;
      did_broadcast = NULL;
      single_broadcast = NULL;
    }

    //--------------------------------------------------------------------------
    void ReplAttachOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!collective_map_barrier.exists());
#endif
      ReplCollectiveViewCreator<
        CollectiveViewCreator<AttachOp> >::deactivate(false/*free*/);
      if (did_broadcast != NULL)
        delete did_broadcast;
      if (single_broadcast != NULL)
        delete single_broadcast;
      if (freeop)
        runtime->free_repl_attach_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplAttachOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
      if (runtime->check_privileges)
        check_privilege();
      ProjectionInfo projection_info;
      RefinementTracker tracker(this, map_applied_conditions);
      runtime->forest->perform_dependence_analysis(this, 0/*idx*/,
                                                   requirement,
                                                   projection_info,
                                                   privilege_path, tracker,
                                                   map_applied_conditions);
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
      assert(!collective_map_barrier.exists());
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // We need collective attach barriers for synchronizing the collective
      // updates to the equivalence sets across the shards
      collective_map_barrier = repl_ctx->get_next_collective_map_barriers();
      if (collective_instances)
        create_collective_view_rendezvous(requirement.parent.get_tree_id(), 0);
    }

    //--------------------------------------------------------------------------
    void ReplAttachOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
      std::set<RtEvent> preconditions;  
#ifdef DEBUG_LEGION
      assert(collective_map_barrier.exists());
#endif
      // Signal that all our mapping dependences are met
      Runtime::phase_barrier_arrive(collective_map_barrier, 1/*count*/);
      runtime->forest->perform_versioning_analysis(this, 0/*idx*/,
                                                   requirement,
                                                   version_info,
                                                   preconditions);
      if (!collective_map_barrier.has_triggered())
        preconditions.insert(collective_map_barrier);
      Runtime::advance_barrier(collective_map_barrier);
      if (!preconditions.empty())
        enqueue_ready_operation(Runtime::merge_events(preconditions));
      else
        enqueue_ready_operation();
    }

    //--------------------------------------------------------------------------
    void ReplAttachOp::trigger_mapping(void)
    //--------------------------------------------------------------------------
    {
      // Create the external instance
      create_external_instance();
      // Register this instance with the memory manager
      PhysicalManager *external_manager =
        external_instances[0].get_physical_manager();
      if (collective_instances)
      {
        // Everybody does the attach in the case of collective construction
        if (!deduplicate_across_shards || is_first_local_shard)
        {
          const RtEvent attached = external_manager->attach_external_instance();
          Runtime::phase_barrier_arrive(resource_barrier, 1/*count*/, attached);
        }
        else
          Runtime::phase_barrier_arrive(resource_barrier, 1/*count*/);
      }
      else if (external_manager->is_owner())
      {
        const RtEvent attached = external_manager->attach_external_instance();
        Runtime::phase_barrier_arrive(resource_barrier, 1/*count*/, attached);
      }
      else
        Runtime::phase_barrier_arrive(resource_barrier, 1/*count*/);
      // Make sure the attaches are done across all shards before continuing
      if (!resource_barrier.has_triggered())
        resource_barrier.wait();
      // Now perform the base call
      AttachOp::trigger_mapping();
    }

    //--------------------------------------------------------------------------
    RtEvent ReplAttachOp::finalize_complete_mapping(RtEvent pre)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(collective_map_barrier.exists());
#endif
      Runtime::phase_barrier_arrive(collective_map_barrier, 1/*count*/, pre);
      return collective_map_barrier;
    }

    //--------------------------------------------------------------------------
    bool ReplAttachOp::perform_collective_analysis(CollectiveMapping *&mapping,
                                                   bool &first_local)
    //--------------------------------------------------------------------------
    {
      if (!collective_instances)
      {
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
        assert(!collective_map_barrier.exists());
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        mapping = &repl_ctx->shard_manager->get_collective_mapping();
        mapping->add_reference();
        first_local = is_first_local_shard;
      }
      return true;
    }

    //--------------------------------------------------------------------------
    bool ReplAttachOp::find_shard_participants(std::vector<ShardID> &shards)
    //--------------------------------------------------------------------------
    {
      // All shards are participating
      return true;
    }

    //--------------------------------------------------------------------------
    PhysicalManager* ReplAttachOp::create_manager(RegionNode *node,
                                   const std::vector<FieldID> &field_set,
                                   const std::vector<size_t> &field_sizes,
                                   const std::vector<unsigned> &mask_index_map,
                                   const std::vector<CustomSerdezID> &serdez,
                                              const FieldMask &external_mask)
    //--------------------------------------------------------------------------
    {
      ApEvent ready_event;
      LayoutConstraintSet constraints;
      PhysicalInstance instance = PhysicalInstance::NO_INST;
      if (is_first_local_shard || 
          (collective_instances && !deduplicate_across_shards))
      {
        switch (resource)
        {
          case LEGION_EXTERNAL_POSIX_FILE:
            {
              std::vector<Realm::FieldID> field_ids(field_set.size());
              unsigned idx = 0;
              for (std::vector<FieldID>::const_iterator it = 
                    field_set.begin(); it != field_set.end(); it++, idx++)
              {
                field_ids[idx] = *it;
              }
              // Do the call to make the instance if we're collective
              // or we're the origin for the single instance case
              if (collective_instances || single_broadcast->is_origin())
              {
                instance = node->row_source->create_file_instance(file_name,
                            field_ids, field_sizes, file_mode, ready_event);
                if (!collective_instances)
                  single_broadcast->broadcast({instance, ready_event});
              }
              constraints.specialized_constraint = 
                SpecializedConstraint(LEGION_GENERIC_FILE_SPECIALIZE);
              constraints.field_constraint = 
                FieldConstraint(requirement.privilege_fields, 
                                false/*contiguous*/, false/*inorder*/);
              constraints.memory_constraint = 
                MemoryConstraint(instance.get_location().kind());
              // TODO: Fill in the other constraints: 
              // OrderingConstraint, SplittingConstraints DimensionConstraints,
              // AlignmentConstraints, OffsetConstraints
              break;
            }
          case LEGION_EXTERNAL_HDF5_FILE:
            {
              // First build the set of field paths
              std::vector<Realm::FieldID> field_ids(field_map.size());
              std::vector<const char*> field_files(field_map.size());
              unsigned idx = 0;
              for (std::map<FieldID,const char*>::const_iterator it = 
                    field_map.begin(); it != field_map.end(); it++, idx++)
              {
                field_ids[idx] = it->first;
                field_files[idx] = it->second;
              }
              // Now ask the low-level runtime to create the instance
              if (collective_instances || single_broadcast->is_origin())
              {
                instance = node->row_source->create_hdf5_instance(file_name,
                                      field_ids, field_sizes, field_files,
                                      layout_constraint_set.ordering_constraint,
                                      (file_mode == LEGION_FILE_READ_ONLY),
                                      ready_event);
                if (!collective_instances)
                  single_broadcast->broadcast({instance, ready_event});
              }
              constraints.specialized_constraint = 
                SpecializedConstraint(LEGION_HDF5_FILE_SPECIALIZE);
              constraints.field_constraint = 
                FieldConstraint(requirement.privilege_fields, 
                                false/*contiguous*/, false/*inorder*/);
              constraints.memory_constraint = 
                MemoryConstraint(instance.get_location().kind());
              constraints.ordering_constraint = 
                layout_constraint_set.ordering_constraint;
              break;
            }
          case LEGION_EXTERNAL_INSTANCE:
            {
              const PointerConstraint &pointer = 
                              layout_constraint_set.pointer_constraint;
#ifdef DEBUG_LEGION
              assert(pointer.is_valid);
#endif
              if (collective_instances || single_broadcast->is_origin())
              {
                ready_event = create_realm_instance(node->row_source, pointer,
                                            field_set, field_sizes, instance);
                if (!collective_instances)
                  single_broadcast->broadcast({instance, ready_event});
              }
              constraints = layout_constraint_set;
              constraints.specialized_constraint = 
                SpecializedConstraint(LEGION_AFFINE_SPECIALIZE);
              break;
            }
          default:
            assert(false);
        }
      }
      // Do the arrival on the attach barrier for any collective instances
      if ((single_broadcast != NULL) && !single_broadcast->is_origin())
      {
        // If we're making a single instance get the name
        const InstanceEventPair result = single_broadcast->get_value();
        instance = result.first;
        ready_event = result.second;
      }
      if (runtime->legion_spy_enabled)
      {
        // We always need a unique ready event for Legion Spy
        if (!ready_event.exists())
        {
          ApUserEvent rename_ready = Runtime::create_ap_user_event(NULL);
          Runtime::trigger_event(NULL, rename_ready);
          ready_event = rename_ready;
        }
        for (std::set<FieldID>::const_iterator it = 
              requirement.privilege_fields.begin(); it !=
              requirement.privilege_fields.end(); it++)
          LegionSpy::log_mapping_decision(unique_op_id, 0/*idx*/, 
                                          *it, ready_event);
#ifdef LEGION_SPY
        LegionSpy::log_operation_events(unique_op_id, ApEvent::NO_AP_EVENT,
                                        get_completion_event());
#endif
      }
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      ShardManager *shard_manager = repl_ctx->shard_manager;
      // Now we need to make the instance to span the shards
      if (collective_instances)
      {
        if (deduplicate_across_shards)
        {
          if (is_first_local_shard)
          {
            PhysicalManager *manager =
              node->column_source->create_external_manager(instance,
                  ready_event, footprint, constraints, field_set, 
                  field_sizes, external_mask, mask_index_map, node,
                  serdez, runtime->get_available_distributed_id()); 
            shard_manager->exchange_shard_local_op_data(context_index, 
                                            exchange_index++, manager);
            return manager;
          }
          else
            return shard_manager->find_shard_local_op_data<PhysicalManager*>(
                                             context_index, exchange_index++);
        }
        else
        {
          // Each shard is just going to make its own physical manager
          return node->column_source->create_external_manager(instance,
              ready_event, footprint, constraints, field_set, field_sizes,
              external_mask, mask_index_map, node, serdez, 
              runtime->get_available_distributed_id());
        }
      }
      else
      {
        // Figure out what the collective mapping is for this instance
        CollectiveMapping *mapping = &shard_manager->get_collective_mapping();
        std::atomic<DistributedID> manager_did(0);
        if ((resource == LEGION_EXTERNAL_INSTANCE) && !contains_individual)
        {
          // We need to send a message to the remote node where no shard
          // lives in order to make this particular instance, we'll give
          // it a collective mapping containing all our address spaces
          // plus the remote address space where the instance lives
          if (did_broadcast->is_origin())
          {
            // Create new collective mapping with the remote address
            // space contained in all of our spaces
            const AddressSpaceID owner_space = instance.address_space();
#ifdef DEBUG_LEGION
            assert(!mapping->contains(owner_space));
#endif
            mapping = mapping->clone_with(owner_space);
            // We're the ones to send the message to the owner
            const RtUserEvent wait_for = Runtime::create_rt_user_event();
            Serializer rez;
            {
              RezCheck z(rez);
              rez.serialize(node->column_source->handle);
              rez.serialize(instance);
              rez.serialize(ready_event);
              rez.serialize(footprint);
              constraints.serialize(rez);
              rez.serialize(external_mask);
              rez.serialize<size_t>(field_set.size());
              for (unsigned idx = 0; idx < field_set.size(); idx++)
              {
                rez.serialize(field_set[idx]);
                rez.serialize(field_sizes[idx]);
                rez.serialize(mask_index_map[idx]);
                rez.serialize(serdez[idx]);
              }
              rez.serialize(node->row_source->handle);
              mapping->pack(rez);
              rez.serialize(&manager_did);
              rez.serialize(wait_for);
            }
            runtime->send_external_create_request(owner_space, rez);
            // Wait for the response to come back
            wait_for.wait();
#ifdef DEBUG_LEGION
            assert(manager_did.load() > 0);
#endif
            did_broadcast->broadcast(manager_did.load());
          }
          else
            manager_did.store(did_broadcast->get_value(false/*not origin*/));
        }
        else
          manager_did.store(did_broadcast->get_value(!did_broadcast->is_origin()));
        // Making an individual instance across all shards
        // Have the first shard be the one to make it 
        if (is_first_local_shard)
        {
          mapping->add_reference();
          PhysicalManager *manager =
            node->column_source->create_external_manager(instance, ready_event,
            footprint, constraints, field_set, field_sizes, external_mask,
            mask_index_map, node, serdez, manager_did.load(), mapping);
          // If we're the owner address space, record that we have 
          // instances on all other address spaces in the control
          // replicated parent task's collective mapping
          if (manager->is_owner())
          {
            for (unsigned idx = 0; idx < mapping->size(); idx++)
            {
              const AddressSpaceID space = (*mapping)[idx];
              if (space == manager->owner_space)
                continue;
              manager->update_remote_instances(space);
            }
          }
          if (mapping->remove_reference())
            delete mapping;
          shard_manager->exchange_shard_local_op_data(context_index,
                                          exchange_index++, manager);
          return manager;
        }
        else
        {
          PhysicalManager *manager =
            shard_manager->find_shard_local_op_data<PhysicalManager*>(
                                      context_index, exchange_index++);
          return manager;
        }
      }
    }

    /////////////////////////////////////////////////////////////
    // Repl Detach Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplDetachOp::ReplDetachOp(Runtime *rt)
      : ReplCollectiveViewCreator<CollectiveViewCreator<DetachOp> >(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplDetachOp::ReplDetachOp(const ReplDetachOp &rhs)
      : ReplCollectiveViewCreator<CollectiveViewCreator<DetachOp> >(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplDetachOp::~ReplDetachOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplDetachOp& ReplDetachOp::operator=(const ReplDetachOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplDetachOp::initialize_replication(ReplicateContext *ctx,
                                        bool collective, bool first_local_shard)
    //--------------------------------------------------------------------------
    {
      collective_instances = collective;
      is_first_local_shard = first_local_shard;
    }

    //--------------------------------------------------------------------------
    void ReplDetachOp::activate(void)
    //--------------------------------------------------------------------------
    {
      ReplCollectiveViewCreator<CollectiveViewCreator<DetachOp> >::activate();
      collective_map_barrier = RtBarrier::NO_RT_BARRIER;
      collective_instances = false;
      is_first_local_shard = false;
    }

    //--------------------------------------------------------------------------
    void ReplDetachOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!collective_map_barrier.exists());
#endif
      ReplCollectiveViewCreator<
        CollectiveViewCreator<DetachOp> >::deactivate(false/*free*/);
      if (freeop)
        runtime->free_repl_detach_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplDetachOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
      ProjectionInfo projection_info;
      RefinementTracker tracker(this, map_applied_conditions);
      runtime->forest->perform_dependence_analysis(this, 0/*idx*/,
                                                   requirement,
                                                   projection_info,
                                                   privilege_path, tracker,
                                                   map_applied_conditions);
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
      assert(!collective_map_barrier.exists());
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      collective_map_barrier = repl_ctx->get_next_collective_map_barriers(); 
      if (collective_instances)
      {
        create_collective_view_rendezvous(requirement.parent.get_tree_id(), 0);
        // If we're flushing we need a second analysis rendezvous
        if (flush)
          create_collective_view_rendezvous(requirement.parent.get_tree_id(),
              0/*requirement index*/, 1/*analysis index*/);
      }
    }

    //--------------------------------------------------------------------------
    void ReplDetachOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
      std::set<RtEvent> preconditions;  
#ifdef DEBUG_LEGION
      assert(collective_map_barrier.exists());
#endif
      // Signal that all our mapping dependences are met
      Runtime::phase_barrier_arrive(collective_map_barrier, 1/*count*/);
      runtime->forest->perform_versioning_analysis(this, 0/*idx*/,
                                                   requirement,
                                                   version_info,
                                                   preconditions);
      if (!collective_map_barrier.has_triggered())
        preconditions.insert(collective_map_barrier);
      Runtime::advance_barrier(collective_map_barrier);
      if (!preconditions.empty())
        enqueue_ready_operation(Runtime::merge_events(preconditions));
      else
        enqueue_ready_operation();
    }

    //--------------------------------------------------------------------------
    RtEvent ReplDetachOp::finalize_complete_mapping(RtEvent pre)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(collective_map_barrier.exists());
#endif
      Runtime::phase_barrier_arrive(collective_map_barrier, 1/*count*/, pre);
      return collective_map_barrier;
    }

    //--------------------------------------------------------------------------
    bool ReplDetachOp::perform_collective_analysis(CollectiveMapping *&mapping,
                                                   bool &first_local)
    //--------------------------------------------------------------------------
    {
      if (!collective_instances)
      {
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
        assert(!collective_map_barrier.exists());
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        mapping = &repl_ctx->shard_manager->get_collective_mapping();
        mapping->add_reference();
        first_local = is_first_local_shard;
      }
      return true;
    }

    //--------------------------------------------------------------------------
    bool ReplDetachOp::find_shard_participants(std::vector<ShardID> &shards)
    //--------------------------------------------------------------------------
    {
      // All shards are participating
      return true;
    }

    //--------------------------------------------------------------------------
    void ReplDetachOp::record_unordered_kind(
          std::map<std::pair<LogicalRegion,FieldID>,ReplDetachOp*> &detachments)
    //--------------------------------------------------------------------------
    {
      const RegionRequirement &req = region.impl->get_requirement();
#ifdef DEBUG_LEGION
      assert(!req.privilege_fields.empty());
#endif
      const std::pair<LogicalRegion,FieldID> key(req.region,
          *(req.privilege_fields.begin()));
#ifdef DEBUG_LEGION
      assert(detachments.find(key) == detachments.end());
#endif
      detachments[key] = this; 
    }

    /////////////////////////////////////////////////////////////
    // Repl Index Attach Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplIndexAttachOp::ReplIndexAttachOp(Runtime *rt)
      : ReplCollectiveViewCreator<IndexAttachOp>(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplIndexAttachOp::ReplIndexAttachOp(const ReplIndexAttachOp &rhs)
      : ReplCollectiveViewCreator<IndexAttachOp>(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplIndexAttachOp::~ReplIndexAttachOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplIndexAttachOp& ReplIndexAttachOp::operator=(
                                                   const ReplIndexAttachOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplIndexAttachOp::activate(void)
    //--------------------------------------------------------------------------
    {
      ReplCollectiveViewCreator<IndexAttachOp>::activate();
      collective = NULL;
      participants = NULL;
      sharding_function = NULL;
    }

    //--------------------------------------------------------------------------
    void ReplIndexAttachOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      ReplCollectiveViewCreator<IndexAttachOp>::deactivate(false/*free*/);
      if (collective != NULL)
        delete collective;
      if (participants != NULL)
        delete participants;
      if (freeop)
        runtime->free_repl_index_attach_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplIndexAttachOp::initialize_replication(ReplicateContext *ctx)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(collective == NULL);
#endif
      collective = new IndexAttachExchange(ctx, COLLECTIVE_LOC_25);
      std::vector<IndexSpace> spaces(points.size());
      for (unsigned idx = 0; idx < points.size(); idx++)
        spaces[idx] = points[idx]->get_requirement().region.get_index_space();
      collective->exchange_spaces(spaces);
      participants = new ShardParticipantsExchange(ctx, COLLECTIVE_LOC_103);
      participants->exchange(points.size() > 0);
    }

    //--------------------------------------------------------------------------
    void ReplIndexAttachOp::trigger_prepipeline_stage(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(sharding_function == NULL);
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      sharding_function = repl_ctx->get_attach_detach_sharding_function();
      IndexAttachOp::trigger_prepipeline_stage();
    }

    //--------------------------------------------------------------------------
    void ReplIndexAttachOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(sharding_function != NULL);
#endif
      std::vector<IndexSpace> spaces;
      unsigned local_start = 0;
      size_t local_size = collective->get_spaces(spaces, local_start); 
      if (requirement.handle_type == LEGION_PARTITION_PROJECTION)
        requirement.projection = parent_ctx->compute_index_attach_projection(
            runtime->forest->get_node(requirement.partition.index_partition),
            this, local_start, local_size, spaces, false/*can use identity*/);
      else
        requirement.projection = parent_ctx->compute_index_attach_projection(
            runtime->forest->get_node(requirement.region.index_space),
            this, local_start, local_size, spaces, false/*can use identity*/);
      // Save this for later when we go to detach it
      resources.impl->set_projection(requirement.projection);
      if (runtime->check_privileges)
      {
        check_privilege();
        check_point_requirements(spaces);
      }
      if (runtime->legion_spy_enabled)
        log_requirement();
      RefinementTracker tracker(this, map_applied_conditions);
      ProjectionInfo projection_info(runtime, requirement, 
                                     launch_space, sharding_function);
      runtime->forest->perform_dependence_analysis(this, 0/*idx*/,
                                                   requirement,
                                                   projection_info,
                                                   privilege_path, tracker,
                                                   map_applied_conditions);
      // Always perform a collective rendezvous for these points
      create_collective_view_rendezvous(requirement.parent.get_tree_id(), 0);
    }

    //--------------------------------------------------------------------------
    void ReplIndexAttachOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
      if (points.empty())
      {
        // Still need to wait for our collectives to be done
        complete_mapping();
        const RtEvent collective_done =
          participants->perform_collective_wait(false/*block*/);
        complete_execution(collective_done);
      }
      else
        IndexAttachOp::trigger_ready();
    }

    //--------------------------------------------------------------------------
    void ReplIndexAttachOp::check_point_requirements(
                                          const std::vector<IndexSpace> &spaces)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      unsigned check_count = 0;
      const ShardID local_shard = repl_ctx->owner_shard->shard_id;
      const unsigned total_shards = repl_ctx->shard_manager->total_shards; 
      for (unsigned idx1 = 1; idx1 < spaces.size(); idx1++)
      {
        for (unsigned idx2 = 0; idx2 < idx1; idx2++)
        {
          // Perfectly balance checks across the shards, this isn't the
          // best for locality, but it will guarantee perfect local balance
          if (((check_count++) % total_shards) != local_shard)
            continue;
          if (!runtime->forest->are_disjoint(spaces[idx1], spaces[idx2]))
            REPORT_LEGION_ERROR(ERROR_INDEX_SPACE_ATTACH,
                "Index attach operation (UID %lld) in parent task %s "
                "(UID %lld) has interfering attachments to regions (%d,%d,%d) "
                "and (%d,%d,%d). All regions must be non-interfering",
                unique_op_id, parent_ctx->get_task_name(),
                parent_ctx->get_unique_id(), spaces[idx1].id,
                requirement.parent.field_space.id, requirement.parent.tree_id,
                spaces[idx2].id, requirement.parent.field_space.id,
                requirement.parent.tree_id)
        }
      }
    }
    
    //--------------------------------------------------------------------------
    bool ReplIndexAttachOp::are_all_direct_children(bool local)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      AllReduceCollective<ProdReduction<bool> > all_direct_children(repl_ctx,
       repl_ctx->get_next_collective_index(COLLECTIVE_LOC_27, true/*logical*/));
      return all_direct_children.sync_all_reduce(local);
    }

    //--------------------------------------------------------------------------
    bool ReplIndexAttachOp::find_shard_participants(
                                                   std::vector<ShardID> &shards)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(participants != NULL);
#endif
      return participants->find_shard_participants(shards);  
    }

    /////////////////////////////////////////////////////////////
    // Repl Index Detach Op 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplIndexDetachOp::ReplIndexDetachOp(Runtime *rt)
      : ReplCollectiveViewCreator<IndexDetachOp>(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplIndexDetachOp::ReplIndexDetachOp(const ReplIndexDetachOp &rhs)
      : ReplCollectiveViewCreator<IndexDetachOp>(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplIndexDetachOp::~ReplIndexDetachOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplIndexDetachOp& ReplIndexDetachOp::operator=(
                                                   const ReplIndexDetachOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplIndexDetachOp::activate(void)
    //--------------------------------------------------------------------------
    {
      ReplCollectiveViewCreator<IndexDetachOp>::activate();
      sharding_function = NULL;
      participants = NULL;
    }

    //--------------------------------------------------------------------------
    void ReplIndexDetachOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      ReplCollectiveViewCreator<IndexDetachOp>::deactivate(false/*free*/);
      if (participants != NULL)
        delete participants;
      if (freeop)
        runtime->free_repl_index_detach_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplIndexDetachOp::initialize_replication(ReplicateContext *ctx)
    //--------------------------------------------------------------------------
    {
      participants = new ShardParticipantsExchange(ctx, COLLECTIVE_LOC_103);
      participants->exchange(points.size() > 0);
    }

    //--------------------------------------------------------------------------
    void ReplIndexDetachOp::trigger_prepipeline_stage(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(sharding_function == NULL);
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      sharding_function = repl_ctx->get_attach_detach_sharding_function();
      IndexDetachOp::trigger_prepipeline_stage();
    }

    //--------------------------------------------------------------------------
    void ReplIndexDetachOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(sharding_function != NULL);
#endif
      // Get the projection ID which we know is valid on the external resources
      requirement.projection = resources.impl->get_projection();
      if (runtime->legion_spy_enabled)
        log_requirement();
      RefinementTracker tracker(this, map_applied_conditions);
      ProjectionInfo projection_info(runtime, requirement,
                                     launch_space, sharding_function);
      runtime->forest->perform_dependence_analysis(this, 0/*idx*/,
                                                   requirement,
                                                   projection_info,
                                                   privilege_path, tracker,
                                                   map_applied_conditions);
      create_collective_view_rendezvous(requirement.parent.get_tree_id(), 0);
      // If we're flushing we need a second analysis rendezvous
      if (flush)
        create_collective_view_rendezvous(requirement.parent.get_tree_id(),0,1);
    }

    //--------------------------------------------------------------------------
    void ReplIndexDetachOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
      if (points.empty())
      {
        // Still need to make sure our collective is done
        complete_mapping();
        const RtEvent collective_done =
          participants->perform_collective_wait(false/*block*/);
        complete_execution(collective_done);
      }
      else
        IndexDetachOp::trigger_ready();
    }

    //--------------------------------------------------------------------------
    bool ReplIndexDetachOp::find_shard_participants(
                                                   std::vector<ShardID> &shards)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(participants != NULL);
#endif
      return participants->find_shard_participants(shards);
    }

    /////////////////////////////////////////////////////////////
    // Repl Acquire Op
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplAcquireOp::ReplAcquireOp(Runtime *rt)
      : ReplCollectiveViewCreator<CollectiveViewCreator<AcquireOp> >(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplAcquireOp::ReplAcquireOp(const ReplAcquireOp &rhs)
      : ReplCollectiveViewCreator<CollectiveViewCreator<AcquireOp> >(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplAcquireOp::~ReplAcquireOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplAcquireOp& ReplAcquireOp::operator=(const ReplAcquireOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplAcquireOp::initialize_replication(ReplicateContext *context,
                                               bool first_local_shard)
    //--------------------------------------------------------------------------
    {
      is_first_local_shard = first_local_shard;
      if (restricted_region.impl == NULL)
        REPORT_LEGION_ERROR(ERROR_CONTROL_REPLICATION_VIOLATION,
            "Acquire operation in control replicated parent task %s "
            "(UID %lld) did not specify a `physical_region' argument. "
            "All acquire operations in control replicated contexts must "
            "specify an explicit PhysicalRegion.",
            parent_ctx->get_task_name(), parent_ctx->get_unique_id())
      if (!grants.empty())
        REPORT_LEGION_ERROR(ERROR_CONTROL_REPLICATION_VIOLATION,
            "Illegal use of grants with an acquire operation in control "
            "replicated parent task %s (UID %lld). Use of non-canonical "
            "Legion features such as grants are not permitted with "
            "control replication.", parent_ctx->get_task_name(),
            parent_ctx->get_unique_id())
      if (!wait_barriers.empty())
        REPORT_LEGION_ERROR(ERROR_CONTROL_REPLICATION_VIOLATION,
            "Illegal use of wait phase barriers with an acquire operation in "
            "control replicated parent task %s (UID %lld). Use of "
            "non-canonical Legion features such as wait phase barriers are "
            "not permitted with control replication.",
            parent_ctx->get_task_name(), parent_ctx->get_unique_id())
      if (!arrive_barriers.empty())
        REPORT_LEGION_ERROR(ERROR_CONTROL_REPLICATION_VIOLATION,
            "Illegal use of arrive phase barriers with an acquire operation in "
            "control replicated parent task %s (UID %lld). Use of "
            "non-canonical Legion features such as arrive phase barriers are "
            "not permitted with control replication.",
            parent_ctx->get_task_name(), parent_ctx->get_unique_id())
    }

    //--------------------------------------------------------------------------
    void ReplAcquireOp::activate(void)
    //--------------------------------------------------------------------------
    {
      ReplCollectiveViewCreator<CollectiveViewCreator<AcquireOp> >::activate();
      collective_map_barrier = RtBarrier::NO_RT_BARRIER;
      is_first_local_shard = false;
    }

    //--------------------------------------------------------------------------
    void ReplAcquireOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      // Make sure we didn't leak our barrier
      assert(!collective_map_barrier.exists());
#endif
      ReplCollectiveViewCreator<
        CollectiveViewCreator<AcquireOp> >::deactivate(false/*free*/);
      if (freeop)
        runtime->free_repl_acquire_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplAcquireOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // If we get here then we're not doing a physical trace replay
      // so we're going to need a collective fill barrier to sync
      // execution of our physical analysis before and after
      collective_map_barrier = repl_ctx->get_next_collective_map_barriers();
      // See if we need to make a collective view rendezvous
      if (restricted_region.impl->collective)
        create_collective_view_rendezvous(requirement.parent.get_tree_id(), 0);
      // Then do the base class analysis
      AcquireOp::trigger_dependence_analysis();
    }

    //--------------------------------------------------------------------------
    void ReplAcquireOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(collective_map_barrier.exists());
#endif
      // Signal that all of our mapping dependences are satisfied
      Runtime::phase_barrier_arrive(collective_map_barrier, 1/*count*/);
      std::set<RtEvent> preconditions;
      runtime->forest->perform_versioning_analysis(this, 0/*idx*/,
                                                   requirement,
                                                   version_info,
                                                   preconditions);
      if (!collective_map_barrier.has_triggered())
        preconditions.insert(collective_map_barrier);
      Runtime::advance_barrier(collective_map_barrier);
      if (!preconditions.empty())
        enqueue_ready_operation(Runtime::merge_events(preconditions));
      else
        enqueue_ready_operation();
    }

    //--------------------------------------------------------------------------
    RtEvent ReplAcquireOp::finalize_complete_mapping(RtEvent pre)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(collective_map_barrier.exists());
#endif
      Runtime::phase_barrier_arrive(collective_map_barrier, 1/*count*/, pre);
      return collective_map_barrier;
    }

    //--------------------------------------------------------------------------
    bool ReplAcquireOp::perform_collective_analysis(CollectiveMapping *&mapping,
                                                    bool &first_local)
    //--------------------------------------------------------------------------
    {
      if (!restricted_region.impl->collective)
      {
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
        assert(!collective_map_barrier.exists());
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        mapping = &repl_ctx->shard_manager->get_collective_mapping();
        mapping->add_reference();
        first_local = is_first_local_shard;
      }
      return true;
    }

    //--------------------------------------------------------------------------
    void ReplAcquireOp::resolve_false(bool speculated, bool launched)
    //--------------------------------------------------------------------------
    {
      if (launched)
        return;
#ifdef DEBUG_LEGION
      assert(!collective_map_barrier.exists());
#endif
      AcquireOp::resolve_false(speculated, launched);
    }

    /////////////////////////////////////////////////////////////
    // Repl Release Op
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplReleaseOp::ReplReleaseOp(Runtime *rt)
      : ReplCollectiveViewCreator<CollectiveViewCreator<ReleaseOp> >(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplReleaseOp::ReplReleaseOp(const ReplReleaseOp &rhs)
      : ReplCollectiveViewCreator<CollectiveViewCreator<ReleaseOp> >(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplReleaseOp::~ReplReleaseOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplReleaseOp& ReplReleaseOp::operator=(const ReplReleaseOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplReleaseOp::initialize_replication(ReplicateContext *context,
                                               bool first_local_shard)
    //--------------------------------------------------------------------------
    {
      if (!runtime->unsafe_mapper)
        sources_check = context->get_next_collective_index(COLLECTIVE_LOC_23);
      is_first_local_shard = first_local_shard;
      if (restricted_region.impl == NULL)
        REPORT_LEGION_ERROR(ERROR_CONTROL_REPLICATION_VIOLATION,
            "Acquire operation in control replicated parent task %s "
            "(UID %lld) did not specify a `physical_region' argument. "
            "All acquire operations in control replicated contexts must "
            "specify an explicit PhysicalRegion.",
            parent_ctx->get_task_name(), parent_ctx->get_unique_id())
      if (!grants.empty())
        REPORT_LEGION_ERROR(ERROR_CONTROL_REPLICATION_VIOLATION,
            "Illegal use of grants with a release operation in control "
            "replicated parent task %s (UID %lld). Use of non-canonical "
            "Legion features such as grants are not permitted with "
            "control replication.", parent_ctx->get_task_name(),
            parent_ctx->get_unique_id())
      if (!wait_barriers.empty())
        REPORT_LEGION_ERROR(ERROR_CONTROL_REPLICATION_VIOLATION,
            "Illegal use of wait phase barriers with a release operation in "
            "control replicated parent task %s (UID %lld). Use of "
            "non-canonical Legion features such as wait phase barriers are "
            "not permitted with control replication.",
            parent_ctx->get_task_name(), parent_ctx->get_unique_id())
      if (!arrive_barriers.empty())
        REPORT_LEGION_ERROR(ERROR_CONTROL_REPLICATION_VIOLATION,
            "Illegal use of arrive phase barriers with a release operation in "
            "control replicated parent task %s (UID %lld). Use of "
            "non-canonical Legion features such as arrive phase barriers are "
            "not permitted with control replication.",
            parent_ctx->get_task_name(), parent_ctx->get_unique_id())
    }

    //--------------------------------------------------------------------------
    void ReplReleaseOp::activate(void)
    //--------------------------------------------------------------------------
    {
      ReplCollectiveViewCreator<CollectiveViewCreator<ReleaseOp> >::activate();
      sources_check = 0;
      collective_map_barrier = RtBarrier::NO_RT_BARRIER;
      is_first_local_shard = false;
    }

    //--------------------------------------------------------------------------
    void ReplReleaseOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      // Make sure we didn't leak our barrier
      assert(!collective_map_barrier.exists());
#endif
      ReplCollectiveViewCreator<
        CollectiveViewCreator<ReleaseOp> >::deactivate(false/*free*/);
      if (freeop)
        runtime->free_repl_release_op(this);
    }

    //--------------------------------------------------------------------------
    void ReplReleaseOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx = dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      // If we get here then we're not doing a physical trace replay
      // so we're going to need a collective fill barrier to sync
      // execution of our physical analysis before and after
      collective_map_barrier = repl_ctx->get_next_collective_map_barriers();
      // See if we need to make a collective view rendezvous
      if (restricted_region.impl->collective)
        create_collective_view_rendezvous(requirement.parent.get_tree_id(), 0);
      // Then do the base class analysis
      ReleaseOp::trigger_dependence_analysis();
    }

    //--------------------------------------------------------------------------
    void ReplReleaseOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(collective_map_barrier.exists());
#endif
      // Signal that all of our mapping dependences are satisfied
      Runtime::phase_barrier_arrive(collective_map_barrier, 1/*count*/);
      std::set<RtEvent> preconditions;
      runtime->forest->perform_versioning_analysis(this, 0/*idx*/,
                                                   requirement,
                                                   version_info,
                                                   preconditions);
      if (!collective_map_barrier.has_triggered())
        preconditions.insert(collective_map_barrier);
      Runtime::advance_barrier(collective_map_barrier);
      if (!preconditions.empty())
        enqueue_ready_operation(Runtime::merge_events(preconditions));
      else
        enqueue_ready_operation();
    }

    //--------------------------------------------------------------------------
    RtEvent ReplReleaseOp::finalize_complete_mapping(RtEvent pre)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(collective_map_barrier.exists());
#endif
      Runtime::phase_barrier_arrive(collective_map_barrier, 1/*count*/, pre);
      return collective_map_barrier;
    }

    //--------------------------------------------------------------------------
    bool ReplReleaseOp::perform_collective_analysis(CollectiveMapping *&mapping,
                                                    bool &first_local)
    //--------------------------------------------------------------------------
    {
      if (!restricted_region.impl->collective)
      {
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
        assert(!collective_map_barrier.exists());
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        mapping = &repl_ctx->shard_manager->get_collective_mapping();
        mapping->add_reference();
        first_local = is_first_local_shard;
      }
      return true;
    }

    //--------------------------------------------------------------------------
    void ReplReleaseOp::resolve_false(bool speculated, bool launched)
    //--------------------------------------------------------------------------
    {
      if (launched)
        return;
#ifdef DEBUG_LEGION
      assert(!collective_map_barrier.exists());
#endif
      ReleaseOp::resolve_false(speculated, launched);
    }

    //--------------------------------------------------------------------------
    void ReplReleaseOp::invoke_mapper(
                                std::vector<PhysicalManager*> &source_instances)
    //--------------------------------------------------------------------------
    {
      // Do the base call
      ReleaseOp::invoke_mapper(source_instances);
      // If we're checking the mapping then do that now to make sure
      // all the shards have the same source instances
      if (!runtime->unsafe_mapper)
      {
#ifdef DEBUG_LEGION
        ReplicateContext *repl_ctx =dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        CheckCollectiveSources sources_collective(repl_ctx, sources_check);
        if (!sources_collective.verify(source_instances))
          REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
              "Invalid mapper output from the invocation of 'map_release' "
              "by mapper %s. Mapper selected difference 'source_instances' "
              "on shard 0 and shard %d when mapping a release operation in "
              "control-replicated parent task %s (UID %lld). Each release "
              "mapping in a control-replicated parent task must provide the "
              "same 'source_instances' across all the shards.",
              mapper->get_mapper_name(), repl_ctx->owner_shard->shard_id,
              parent_ctx->get_task_name(), parent_ctx->get_unique_id())
      }
    }

    /////////////////////////////////////////////////////////////
    // ReplTraceOp 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplTraceOp::ReplTraceOp(Runtime *rt)
      : ReplFenceOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTraceOp::ReplTraceOp(const ReplTraceOp &rhs)
      : ReplFenceOp(NULL)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplTraceOp::~ReplTraceOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTraceOp& ReplTraceOp::operator=(const ReplTraceOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplTraceOp::sync_for_replayable_check(void)
    //--------------------------------------------------------------------------
    {
      // Should only be called by derived classes
      assert(false);
    }

    //--------------------------------------------------------------------------
    bool ReplTraceOp::exchange_replayable(ReplicateContext *ctx,bool replayable)
    //--------------------------------------------------------------------------
    {
      // Should only be called by derived classes
      assert(false);
      return false;
    }

    //--------------------------------------------------------------------------
    void ReplTraceOp::sync_compute_frontiers(RtEvent precondition)
    //--------------------------------------------------------------------------
    {
      // Should only be called by derived classes
      assert(false);
    }

    /////////////////////////////////////////////////////////////
    // ReplTraceCaptureOp 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplTraceCaptureOp::ReplTraceCaptureOp(Runtime *rt)
      : ReplTraceOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTraceCaptureOp::ReplTraceCaptureOp(const ReplTraceCaptureOp &rhs)
      : ReplTraceOp(NULL)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplTraceCaptureOp::~ReplTraceCaptureOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTraceCaptureOp& ReplTraceCaptureOp::operator=(
                                                  const ReplTraceCaptureOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplTraceCaptureOp::initialize_capture(ReplicateContext *ctx, 
                  Provenance *provenance, bool has_block, bool remove_trace_ref)
    //--------------------------------------------------------------------------
    {
      initialize(ctx, EXECUTION_FENCE, false/*need future*/, provenance);
      tracing = false;
      current_template = NULL;
      has_blocking_call = has_block;
      remove_trace_reference = remove_trace_ref;
      // Get a collective ID to use for check all replayable
      replayable_collective_id = 
        ctx->get_next_collective_index(COLLECTIVE_LOC_85); 
      replay_sync_collective_id =
        ctx->get_next_collective_index(COLLECTIVE_LOC_91);
      sync_compute_frontiers_collective_id =
        ctx->get_next_collective_index(COLLECTIVE_LOC_92);
    }

    //--------------------------------------------------------------------------
    void ReplTraceCaptureOp::activate(void)
    //--------------------------------------------------------------------------
    {
      ReplTraceOp::activate();
      current_template = NULL;
      recording_fence = RtBarrier::NO_RT_BARRIER;
      replayable_collective_id = 0;
      has_blocking_call = false;
      remove_trace_reference = false;
      is_recording = false;
    }

    //--------------------------------------------------------------------------
    void ReplTraceCaptureOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      ReplTraceOp::deactivate(false/*free*/);
      if (freeop)
        runtime->free_repl_capture_op(this);
    }

    //--------------------------------------------------------------------------
    const char* ReplTraceCaptureOp::get_logging_name(void) const
    //--------------------------------------------------------------------------
    {
      return op_names[TRACE_CAPTURE_OP_KIND];
    }

    //--------------------------------------------------------------------------
    Operation::OpKind ReplTraceCaptureOp::get_operation_kind(void) const
    //--------------------------------------------------------------------------
    {
      return TRACE_CAPTURE_OP_KIND;
    }

    //--------------------------------------------------------------------------
    void ReplTraceCaptureOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(trace != NULL);
#endif
      // Indicate that we are done capturing this trace
      trace->end_trace_capture();
      if (trace->is_recording())
      {
        PhysicalTrace *physical_trace = trace->get_physical_trace();
#ifdef DEBUG_LEGION
        assert(physical_trace != NULL);
#endif
        current_template = physical_trace->get_current_template();
        physical_trace->record_previous_template_completion(
            get_completion_event());
        physical_trace->clear_cached_template();
        // Get an additional mapping fence to ensure that all our prior
        // operations are done mapping before anybody tries to finalize
        // the capture which could induce races
#ifdef DEBUG_LEGION
        assert(!recording_fence.exists());
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        recording_fence = repl_ctx->get_next_mapping_fence_barrier();
        // Save this for later since we can't access it safely in mapping stage
        is_recording = true;
      }
      // Register this fence with all previous users in the parent's context
      ReplFenceOp::trigger_dependence_analysis();
      parent_ctx->record_previous_trace(trace);
    }

    //--------------------------------------------------------------------------
    void ReplTraceCaptureOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
      if (recording_fence.exists())
      {
        Runtime::phase_barrier_arrive(recording_fence, 1/*count*/);
        enqueue_ready_operation(recording_fence);
      }
      else
        enqueue_ready_operation();
    }

    //--------------------------------------------------------------------------
    void ReplTraceCaptureOp::trigger_mapping(void)
    //--------------------------------------------------------------------------
    {
      // Now finish capturing the physical trace
      if (is_recording)
      {
        PhysicalTrace *physical_trace = trace->get_physical_trace();
#ifdef DEBUG_LEGION
        assert(physical_trace != NULL);
        assert(current_template != NULL);
        assert(trace->get_physical_trace() != NULL);
        assert(current_template->is_recording());
#endif
        current_template->finalize(parent_ctx, unique_op_id, 
                                   has_blocking_call, this);
        if (!current_template->is_replayable())
        {
          physical_trace->record_failed_capture(current_template);
          ApEvent pending_deletion;
          if (!current_template->defer_template_deletion(pending_deletion,
                                                  map_applied_conditions))
            delete current_template;
          if (pending_deletion.exists())
            execution_preconditions.insert(pending_deletion);
        }
        else
        {
          ApEvent pending_deletion = physical_trace->record_replayable_capture(
                                      current_template, map_applied_conditions);
          if (pending_deletion.exists())
            execution_preconditions.insert(pending_deletion);
        }
        // Reset the local trace
        trace->initialize_tracing_state();
      }
      if (remove_trace_reference && trace->remove_reference())
        delete trace;
      ReplFenceOp::trigger_mapping();
    }

    //--------------------------------------------------------------------------
    void ReplTraceCaptureOp::sync_for_replayable_check(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx =dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      SlowBarrier replay_sync_barrier(repl_ctx, replay_sync_collective_id);
      replay_sync_barrier.perform_collective_sync();
    }

    //--------------------------------------------------------------------------
    bool ReplTraceCaptureOp::exchange_replayable(ReplicateContext *repl_ctx,
                                                 bool shard_replayable)
    //--------------------------------------------------------------------------
    {
      // Check to see if this template is replayable across all the shards
      AllReduceCollective<ProdReduction<bool> > 
        all_replayable_collective(repl_ctx, replayable_collective_id);
      return all_replayable_collective.sync_all_reduce(shard_replayable);
    }

    //--------------------------------------------------------------------------
    void ReplTraceCaptureOp::sync_compute_frontiers(RtEvent precondition)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx =dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      SlowBarrier pre_sync_barrier(repl_ctx,
          sync_compute_frontiers_collective_id);
      pre_sync_barrier.perform_collective_sync(precondition);
    }

    /////////////////////////////////////////////////////////////
    // ReplTraceCompleteOp 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplTraceCompleteOp::ReplTraceCompleteOp(Runtime *rt)
      : ReplTraceOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTraceCompleteOp::ReplTraceCompleteOp(const ReplTraceCompleteOp &rhs)
      : ReplTraceOp(NULL)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplTraceCompleteOp::~ReplTraceCompleteOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTraceCompleteOp& ReplTraceCompleteOp::operator=(
                                                 const ReplTraceCompleteOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplTraceCompleteOp::initialize_complete(ReplicateContext *ctx, 
                                         Provenance *provenance, bool has_block)
    //--------------------------------------------------------------------------
    {
      initialize(ctx, EXECUTION_FENCE, false/*need future*/, provenance);
#ifdef DEBUG_LEGION
      assert(trace != NULL);
#endif
      tracing = false;
      current_template = NULL;
      template_completion = ApEvent::NO_AP_EVENT;
      replayed = false;
      has_blocking_call = has_block;
      // Get a collective ID to use for check all replayable
      replayable_collective_id = 
        ctx->get_next_collective_index(COLLECTIVE_LOC_86);
      replay_sync_collective_id =
        ctx->get_next_collective_index(COLLECTIVE_LOC_91);
      sync_compute_frontiers_collective_id =
        ctx->get_next_collective_index(COLLECTIVE_LOC_92);
    }

    //--------------------------------------------------------------------------
    void ReplTraceCompleteOp::activate(void)
    //--------------------------------------------------------------------------
    {
      ReplTraceOp::activate();
      current_template = NULL;
      template_completion = ApEvent::NO_AP_EVENT;
      recording_fence = RtBarrier::NO_RT_BARRIER;
      replayable_collective_id = 0;
      replayed = false;
      has_blocking_call = false;
      is_recording = false;
    }

    //--------------------------------------------------------------------------
    void ReplTraceCompleteOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      ReplTraceOp::deactivate(false/*free*/);
      if (freeop)
        runtime->free_repl_trace_op(this);
    }

    //--------------------------------------------------------------------------
    const char* ReplTraceCompleteOp::get_logging_name(void) const
    //--------------------------------------------------------------------------
    {
      return op_names[TRACE_COMPLETE_OP_KIND];
    }

    //--------------------------------------------------------------------------
    Operation::OpKind ReplTraceCompleteOp::get_operation_kind(void) const
    //--------------------------------------------------------------------------
    {
      return TRACE_COMPLETE_OP_KIND; 
    }

    //--------------------------------------------------------------------------
    void ReplTraceCompleteOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
#ifdef LEGION_SPY
      if (trace->is_replaying())
      {
        PhysicalTrace *physical_trace = trace->get_physical_trace();
#ifdef DEBUG_LEGION
        assert(physical_trace != NULL);
#endif
        trace->perform_logging(
         physical_trace->get_current_template()->get_fence_uid(), unique_op_id);
      }
#endif
      trace->end_trace_execution(this);
      parent_ctx->record_previous_trace(trace);

      if (trace->is_replaying())
      {
        if (has_blocking_call)
          REPORT_LEGION_ERROR(ERROR_INVALID_PHYSICAL_TRACING,
            "Physical tracing violation! Trace %d in task %s (UID %lld) "
            "encountered a blocking API call that was unseen when it was "
            "recorded. It is required that traces do not change their "
            "behavior.", trace->get_trace_id(),
            parent_ctx->get_task_name(), parent_ctx->get_unique_id())
        PhysicalTrace *physical_trace = trace->get_physical_trace();
#ifdef DEBUG_LEGION
        assert(physical_trace != NULL);
#endif
        current_template = physical_trace->get_current_template();
#ifdef DEBUG_LEGION
        assert(current_template != NULL);
#endif
#ifdef LEGION_SPY
        trace->perform_logging(
            current_template->get_fence_uid(), unique_op_id);
#endif
        // Get our fence barriers
        initialize_fence_barriers();
        parent_ctx->update_current_fence(this, true, true);
        // This is where we make sure that replays are done in order
        // We need to do this because we're not registering this as
        // a fence with the context
        physical_trace->chain_replays(this);
        physical_trace->record_previous_template_completion(
                                      get_completion_event());
        trace->initialize_tracing_state();
        replayed = true;
        return;
      }
      else if (trace->is_recording())
      {
        PhysicalTrace *physical_trace = trace->get_physical_trace();
#ifdef DEBUG_LEGION
        assert(physical_trace != NULL);
#endif
        current_template = physical_trace->get_current_template();
        physical_trace->record_previous_template_completion(
                                      get_completion_event());
        physical_trace->clear_cached_template();
        // Get an additional mapping fence to ensure that all our prior
        // operations are done mapping before anybody tries to finalize
        // the capture which could induce races
#ifdef DEBUG_LEGION
        assert(!recording_fence.exists());
        ReplicateContext *repl_ctx = 
          dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
        recording_fence = repl_ctx->get_next_mapping_fence_barrier();
        // Save this for later since we can't access it safely in mapping stage
        is_recording = true;
      } 

      // If this is a static trace, then we remove our reference when we're done
      if (trace->is_static_trace())
      {
        StaticTrace *static_trace = static_cast<StaticTrace*>(trace);
        if (static_trace->remove_reference())
          delete static_trace;
      }
      ReplFenceOp::trigger_dependence_analysis();
    }

    //--------------------------------------------------------------------------
    void ReplTraceCompleteOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
      if (recording_fence.exists())
      {
        Runtime::phase_barrier_arrive(recording_fence, 1/*count*/);
        enqueue_ready_operation(recording_fence);
        return;
      }
      else if (replayed)
      {
        // Having all our mapping dependences satisfied means that the previous 
        // replay of this template is done so we can start ours now
        std::set<RtEvent> replayed_events;
        current_template->perform_replay(runtime, replayed_events);
        if (!replayed_events.empty())
        {
          enqueue_ready_operation(Runtime::merge_events(replayed_events));
          return;
        }
      }
      enqueue_ready_operation();
    }

    //--------------------------------------------------------------------------
    void ReplTraceCompleteOp::trigger_mapping(void)
    //--------------------------------------------------------------------------
    {
      // Now finish capturing the physical trace
      if (is_recording)
      {
        PhysicalTrace *physical_trace = trace->get_physical_trace();
#ifdef DEBUG_LEGION
        assert(physical_trace != NULL);
        assert(current_template != NULL);
        assert(trace->get_physical_trace() != NULL);
        assert(current_template->is_recording());
#endif
        current_template->finalize(parent_ctx, unique_op_id, 
                                   has_blocking_call, this);
        if (!current_template->is_replayable())
        {
          physical_trace->record_failed_capture(current_template);
          ApEvent pending_deletion;
          if (!current_template->defer_template_deletion(pending_deletion,
                                                  map_applied_conditions))
            delete current_template;
          if (pending_deletion.exists())
            execution_preconditions.insert(pending_deletion);
        }
        else
        {
          ApEvent pending_deletion = physical_trace->record_replayable_capture(
                                      current_template, map_applied_conditions);
          if (pending_deletion.exists())
            execution_preconditions.insert(pending_deletion);
        }
        trace->initialize_tracing_state();
      }
      else if (replayed)
      { 
#ifdef DEBUG_LEGION
        assert(current_template != NULL);
        assert(map_applied_conditions.empty());
#endif
        std::set<ApEvent> template_postconditions;
        current_template->finish_replay(template_postconditions);
        // Do our arrival on the mapping fence
        Runtime::phase_barrier_arrive(mapping_fence_barrier, 1/*count*/);
        complete_mapping(mapping_fence_barrier);
        if (!template_postconditions.empty())
          Runtime::phase_barrier_arrive(execution_fence_barrier, 1/*count*/,
              Runtime::merge_events(NULL, template_postconditions));
        else
          Runtime::phase_barrier_arrive(execution_fence_barrier, 1/*count*/);
        record_completion_effect(execution_fence_barrier);
        complete_execution();
        return;
      }
      ReplFenceOp::trigger_mapping();
    }

    //--------------------------------------------------------------------------
    void ReplTraceCompleteOp::sync_for_replayable_check(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx =dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      SlowBarrier replay_sync_barrier(repl_ctx, replay_sync_collective_id);
      replay_sync_barrier.perform_collective_sync();
    }

    //--------------------------------------------------------------------------
    bool ReplTraceCompleteOp::exchange_replayable(ReplicateContext *repl_ctx,
                                                  bool shard_replayable)
    //--------------------------------------------------------------------------
    {
      // Check to see if this template is replayable across all the shards
      AllReduceCollective<ProdReduction<bool> > 
        all_replayable_collective(repl_ctx, replayable_collective_id);
      return all_replayable_collective.sync_all_reduce(shard_replayable);
    }

    //--------------------------------------------------------------------------
    void ReplTraceCompleteOp::sync_compute_frontiers(RtEvent precondition)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      ReplicateContext *repl_ctx =dynamic_cast<ReplicateContext*>(parent_ctx);
      assert(repl_ctx != NULL);
#else
      ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif
      SlowBarrier pre_sync_barrier(repl_ctx,
          sync_compute_frontiers_collective_id);
      pre_sync_barrier.perform_collective_sync(precondition);
    }

    /////////////////////////////////////////////////////////////
    // ReplTraceReplayOp
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplTraceReplayOp::ReplTraceReplayOp(Runtime *rt)
      : ReplTraceOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTraceReplayOp::ReplTraceReplayOp(const ReplTraceReplayOp &rhs)
      : ReplTraceOp(NULL)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplTraceReplayOp::~ReplTraceReplayOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTraceReplayOp& ReplTraceReplayOp::operator=(
                                                   const ReplTraceReplayOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplTraceReplayOp::initialize_replay(ReplicateContext *ctx, 
                                        LegionTrace *tr, Provenance *provenance)
    //--------------------------------------------------------------------------
    {
      initialize(ctx, EXECUTION_FENCE, false/*need future*/, provenance);
      trace = tr;
      for (int idx = 0; idx < TRACE_SELECTION_ROUNDS; idx++)
        trace_selection_collective_ids[idx] = 
          ctx->get_next_collective_index(COLLECTIVE_LOC_87);
    }

    //--------------------------------------------------------------------------
    void ReplTraceReplayOp::activate(void)
    //--------------------------------------------------------------------------
    {
      ReplTraceOp::activate();
    }

    //--------------------------------------------------------------------------
    void ReplTraceReplayOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      ReplTraceOp::deactivate(false/*free*/);
      if (freeop)
        runtime->free_repl_replay_op(this);
    }

    //--------------------------------------------------------------------------
    const char* ReplTraceReplayOp::get_logging_name(void) const
    //--------------------------------------------------------------------------
    {
      return op_names[TRACE_REPLAY_OP_KIND];
    }

    //--------------------------------------------------------------------------
    Operation::OpKind ReplTraceReplayOp::get_operation_kind(void) const
    //--------------------------------------------------------------------------
    {
      return TRACE_REPLAY_OP_KIND;
    }

    //--------------------------------------------------------------------------
    void ReplTraceReplayOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
      initialize_fence_barriers();
      PhysicalTrace *physical_trace = trace->get_physical_trace();
#ifdef DEBUG_LEGION
      assert(physical_trace != NULL); 
#endif
      bool recurrent = true;
      bool fence_registered = false;
      bool is_recording = trace->is_recording();
      if ((physical_trace->get_current_template() == NULL) || is_recording)
      {
        recurrent = false;
        {
          // Wait for the previous recordings to be done before checking
          // template preconditions, otherwise no template would exist.
          RtEvent mapped_event = parent_ctx->get_current_mapping_fence_event();
          if (mapped_event.exists())
            mapped_event.wait();
        }
#ifdef DEBUG_LEGION
        assert(!(trace->is_recording() || trace->is_replaying()));
        ReplicateContext *repl_ctx =dynamic_cast<ReplicateContext*>(parent_ctx);
        assert(repl_ctx != NULL);
#else
        ReplicateContext *repl_ctx = static_cast<ReplicateContext*>(parent_ctx);
#endif

        if (physical_trace->get_current_template() == NULL)
        {
          int selected_template_index = -2;  
          std::vector<int> viable_templates;
          for (int round = 0; round < TRACE_SELECTION_ROUNDS; round++)
          {
            // Exponential back-off: the more rounds we go, the
            // more templates we try to find to build consensus
            const unsigned number_to_find = 1 << round;
            if ((viable_templates.empty() || (viable_templates.back() >= 0)) &&
                physical_trace->find_viable_templates(this, 
                  map_applied_conditions, number_to_find, viable_templates))
            {
              // If we checked all the templates figure out what kind of 
              // guard to add:
              // Use -1 to indicate that we're done but have viable templates
              // Use -2 to indicate we have no viable templates
              if (!viable_templates.empty())
                viable_templates.push_back(-1);
              else
                viable_templates.push_back(-2);
            }
#ifdef DEBUG_LEGION
            assert(!viable_templates.empty());
#endif
            // Perform an exchange to see if we have consensus
            TemplateIndexExchange index_exchange(repl_ctx, 
                    trace_selection_collective_ids[round]);
            index_exchange.initiate_exchange(viable_templates);
            std::map<int/*index*/,unsigned/*count*/> result_templates;
            index_exchange.complete_exchange(result_templates);
            // First, if we have at least one shard that says that it
            // has no viable templates then we're done
            if (result_templates.find(-2) == result_templates.end())
            {
              // Otherwise go through in reverse order and look for one that
              // has consensus from all the shards
              const size_t total_shards = repl_ctx->shard_manager->total_shards;
              for (std::map<int,unsigned>::reverse_iterator rit = 
                    result_templates.rbegin(); rit != 
                    result_templates.rend(); rit++)
              {
#ifdef DEBUG_LEGION
                assert(rit->second <= total_shards);
#endif
                // If we have a template that is viable for all the shards
                // then we've succesffully identified a template to use
                if (rit->second == total_shards)
                {
                  // Note this could also be -1 in the case were all
                  // the shards have identified all their viable templates
                  selected_template_index = rit->first;
                  break;
                }
              }
            }
            else
              selected_template_index = -1;
            // If we picked an index then we're done
            if (selected_template_index != -2)
              break;
          }
          // If we successfully identified a template for all the shards
          // to use then we record that in the trace 
          if (selected_template_index >= 0)
            physical_trace->select_template(selected_template_index);
        }
#ifdef DEBUG_LEGION
        assert(physical_trace->get_current_template() == NULL ||
               !physical_trace->get_current_template()->is_recording());
#endif
        parent_ctx->perform_fence_analysis(this, execution_preconditions,
                                           true/*mapping*/, true/*execution*/);
        physical_trace->set_current_execution_fence_event(
            get_completion_event());
        fence_registered = true;
      }

      const bool replaying = (physical_trace->get_current_template() != NULL);
      // Tell the parent context about the physical trace replay result
      parent_ctx->record_physical_trace_replay(mapped_event, replaying);
      if (replaying)
      {
        // If we're recurrent, then check to see if we had any intermeidate
        // ops for which we still need to perform the fence analysis
        // If there were no intermediate dependences then we can just
        // record a dependence on the previous fence
        const ApEvent fence_completion = (recurrent &&
          !trace->has_intermediate_operations()) ?
            physical_trace->get_previous_template_completion()
                    : get_completion_event();
        if (recurrent && trace->has_intermediate_operations())
        {
          parent_ctx->perform_fence_analysis(this, execution_preconditions,
                                       true/*mapping*/, true/*execution*/);
          trace->reset_intermediate_operations();
        }
        if (!fence_registered)
          execution_preconditions.insert(
              parent_ctx->get_current_execution_fence_event());
        physical_trace->initialize_template(fence_completion, recurrent);
        trace->set_state_replay();
#ifdef LEGION_SPY
        physical_trace->get_current_template()->set_fence_uid(unique_op_id);
#endif
      }
      else if (!fence_registered)
      {
        parent_ctx->perform_fence_analysis(this, execution_preconditions,
                                           true/*mapping*/, true/*execution*/);
        physical_trace->set_current_execution_fence_event(
            get_completion_event());
      }

      // Now update the parent context with this fence before we can complete
      // the dependence analysis and possibly be deactivated
      parent_ctx->update_current_fence(this, true, true);
    }

    //--------------------------------------------------------------------------
    void ReplTraceReplayOp::pack_remote_operation(Serializer &rez,
                 AddressSpaceID target, std::set<RtEvent> &applied_events) const
    //--------------------------------------------------------------------------
    {
      pack_local_remote_operation(rez);
    }

    /////////////////////////////////////////////////////////////
    // ReplTraceBeginOp
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplTraceBeginOp::ReplTraceBeginOp(Runtime *rt)
      : ReplTraceOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTraceBeginOp::ReplTraceBeginOp(const ReplTraceBeginOp &rhs)
      : ReplTraceOp(NULL)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplTraceBeginOp::~ReplTraceBeginOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTraceBeginOp& ReplTraceBeginOp::operator=(const ReplTraceBeginOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplTraceBeginOp::initialize_begin(ReplicateContext *ctx, 
                                        LegionTrace *tr, Provenance *provenance)
    //--------------------------------------------------------------------------
    {
      initialize(ctx, MAPPING_FENCE, false/*need future*/, provenance);
      trace = tr;
      tracing = false;
    }

    //--------------------------------------------------------------------------
    void ReplTraceBeginOp::activate(void)
    //--------------------------------------------------------------------------
    {
      ReplTraceOp::activate();
    }

    //--------------------------------------------------------------------------
    void ReplTraceBeginOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      ReplTraceOp::deactivate(false/*free*/);
      if (freeop)
        runtime->free_repl_begin_op(this);
    }

    //--------------------------------------------------------------------------
    const char* ReplTraceBeginOp::get_logging_name(void) const
    //--------------------------------------------------------------------------
    {
      return op_names[TRACE_BEGIN_OP_KIND];
    }

    //--------------------------------------------------------------------------
    Operation::OpKind ReplTraceBeginOp::get_operation_kind(void) const
    //--------------------------------------------------------------------------
    {
      return TRACE_BEGIN_OP_KIND;
    }

    /////////////////////////////////////////////////////////////
    // ReplTraceSummaryOp
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ReplTraceSummaryOp::ReplTraceSummaryOp(Runtime *rt)
      : ReplTraceOp(rt)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTraceSummaryOp::ReplTraceSummaryOp(const ReplTraceSummaryOp &rhs)
      : ReplTraceOp(NULL)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ReplTraceSummaryOp::~ReplTraceSummaryOp(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ReplTraceSummaryOp& ReplTraceSummaryOp::operator=(
                                                  const ReplTraceSummaryOp &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ReplTraceSummaryOp::initialize_summary(ReplicateContext *ctx,
                                                ShardedPhysicalTemplate *tpl,
                                                Operation *invalidator,
                                                Provenance *provenance)
    //--------------------------------------------------------------------------
    {
      // Do NOT call 'initialize' here, we're in the dependence
      // analysis stage of the pipeline and we need to get our mapping
      // fence from a different location to avoid racing with the application
      initialize(ctx, MAPPING_FENCE, false/*need future*/,
                 provenance, false/*track*/);
      context_index = invalidator->get_ctx_index();
      current_template = tpl;
      // The summary could have been marked as being traced,
      // so here we forcibly clear them out.
      trace = NULL;
      tracing = false;
    }

    //--------------------------------------------------------------------------
    void ReplTraceSummaryOp::activate(void)
    //--------------------------------------------------------------------------
    {
      ReplTraceOp::activate();
      current_template = NULL;
    }

    //--------------------------------------------------------------------------
    void ReplTraceSummaryOp::deactivate(bool freeop)
    //--------------------------------------------------------------------------
    {
      ReplTraceOp::deactivate(false/*free*/);
      if (freeop)
        runtime->free_repl_summary_op(this);
    }

    //--------------------------------------------------------------------------
    const char* ReplTraceSummaryOp::get_logging_name(void) const
    //--------------------------------------------------------------------------
    {
      return op_names[TRACE_SUMMARY_OP_KIND];
    }

    //--------------------------------------------------------------------------
    Operation::OpKind ReplTraceSummaryOp::get_operation_kind(void) const
    //--------------------------------------------------------------------------
    {
      return TRACE_SUMMARY_OP_KIND;
    }

    //--------------------------------------------------------------------------
    void ReplTraceSummaryOp::trigger_dependence_analysis(void)
    //--------------------------------------------------------------------------
    {
      initialize_fence_barriers();
      perform_fence_analysis(true/*register fence also*/);
    }

    //--------------------------------------------------------------------------
    void ReplTraceSummaryOp::trigger_ready(void)
    //--------------------------------------------------------------------------
    {
      enqueue_ready_operation();
    }

    //--------------------------------------------------------------------------
    void ReplTraceSummaryOp::trigger_mapping(void)
    //--------------------------------------------------------------------------
    {
      if (current_template->is_replayable())
        current_template->apply_postcondition(this, map_applied_conditions);
      ReplFenceOp::trigger_mapping();
    }

    //--------------------------------------------------------------------------
    void ReplTraceSummaryOp::pack_remote_operation(Serializer &rez,
                 AddressSpaceID target, std::set<RtEvent> &applied_events) const
    //--------------------------------------------------------------------------
    {
      pack_local_remote_operation(rez);
    }

    /////////////////////////////////////////////////////////////
    // Shard Mapping
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ShardMapping::ShardMapping(void)
      : Collectable()
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ShardMapping::ShardMapping(const ShardMapping &rhs)
      : Collectable()
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ShardMapping::ShardMapping(const std::vector<AddressSpaceID> &spaces)
      : Collectable(), address_spaces(spaces)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ShardMapping::~ShardMapping(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ShardMapping& ShardMapping::operator=(const ShardMapping &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    AddressSpaceID ShardMapping::operator[](unsigned idx) const
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(idx < address_spaces.size());
#endif
      return address_spaces[idx];
    }

    //--------------------------------------------------------------------------
    AddressSpaceID& ShardMapping::operator[](unsigned idx)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(idx < address_spaces.size());
#endif
      return address_spaces[idx];
    }

    //--------------------------------------------------------------------------
    void ShardMapping::pack_mapping(Serializer &rez) const
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(address_spaces.size());
      for (std::vector<AddressSpaceID>::const_iterator it = 
            address_spaces.begin(); it != address_spaces.end(); it++)
        rez.serialize(*it);
    }

    //--------------------------------------------------------------------------
    void ShardMapping::unpack_mapping(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      size_t num_spaces;
      derez.deserialize(num_spaces);
      address_spaces.resize(num_spaces);
      for (unsigned idx = 0; idx < num_spaces; idx++)
        derez.deserialize(address_spaces[idx]);
    } 

    /////////////////////////////////////////////////////////////
    // Shard Manager 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ShardManager::ShardManager(Runtime *rt, ReplicationID id, bool control, 
                               bool top, bool iso, const Domain &dom,
                               std::vector<DomainPoint> &&shards,
                               std::vector<DomainPoint> &&sorted,
                               std::vector<ShardID> &&lookup,
                               AddressSpaceID owner, 
                               SingleTask *original/*= NULL*/, RtBarrier bar)
      : runtime(rt), repl_id(id), owner_space(owner), shard_points(shards),
        sorted_points(sorted), shard_lookup(lookup), shard_domain(dom),
        total_shards(shard_points.size()), original_task(original),
        control_replicated(control), top_level_task(top),
        isomorphic_points(iso), address_spaces(NULL), collective_mapping(NULL),
        local_mapping_complete(0), remote_mapping_complete(0),
        local_execution_complete(0), remote_execution_complete(0),
        trigger_local_complete(0), trigger_remote_complete(0),
        trigger_local_commit(0), trigger_remote_commit(0), 
        remote_constituents(0), semantic_attach_counter(0), 
        local_future_result(NULL), shard_task_barrier(bar),
        attach_deduplication(NULL)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(total_shards > 0);
      assert(shard_points.size() == sorted_points.size());
      assert(shard_points.size() == shard_lookup.size());
#endif
      // Add an extra reference if we're not the owner manager
      if (owner_space != runtime->address_space)
        add_reference();
      runtime->register_shard_manager(repl_id, this);
      if (control_replicated && (owner_space == runtime->address_space))
      {
#ifdef DEBUG_LEGION
        assert(!shard_task_barrier.exists());
#endif
        shard_task_barrier = 
          RtBarrier(Realm::Barrier::create_barrier(total_shards));
        // callback barrier can't be made until we know how many
        // unique address spaces we'll actually have so see
        // ShardManager::launch
      }
#ifdef DEBUG_LEGION
      else if (control_replicated)
        assert(shard_task_barrier.exists());
#endif
    }

    //--------------------------------------------------------------------------
    ShardManager::~ShardManager(void)
    //--------------------------------------------------------------------------
    { 
      // We can delete our shard tasks
      for (std::vector<ShardTask*>::const_iterator it = 
            local_shards.begin(); it != local_shards.end(); it++)
        delete (*it);
      local_shards.clear();
      for (std::map<ShardingID,ShardingFunction*>::const_iterator it =
            sharding_functions.begin(); it != sharding_functions.end(); it++)
        delete it->second;
      sharding_functions.clear();
      // Finally unregister ourselves with the runtime
      const bool owner_manager = (owner_space == runtime->address_space);
      runtime->unregister_shard_manager(repl_id, owner_manager);
      if (owner_manager)
      {
        if (control_replicated)
        {
          shard_task_barrier.destroy_barrier();
          callback_barrier.destroy_barrier();
        }
        // Send messages to all the remote spaces to remove the manager
        std::set<AddressSpaceID> sent_spaces;
        for (unsigned idx = 0; idx < address_spaces->size(); idx++)
        {
          AddressSpaceID target = (*address_spaces)[idx];
          if (sent_spaces.find(target) != sent_spaces.end())
            continue;
          if (target == runtime->address_space)
            continue;
          Serializer rez;
          {
            RezCheck z(rez);
            rez.serialize(repl_id);
          }
          runtime->send_replicate_delete(target, rez);
          sent_spaces.insert(target);
        }
      }
      if ((address_spaces != NULL) && address_spaces->remove_reference())
        delete address_spaces;
      if ((collective_mapping != NULL) && 
          collective_mapping->remove_reference())
        delete collective_mapping;
#ifdef DEBUG_LEGION
      assert(local_future_result == NULL);
      assert(created_equivalence_sets.empty());
#endif
    }

    //--------------------------------------------------------------------------
    void ShardManager::set_shard_mapping(const std::vector<Processor> &mapping)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(mapping.size() == total_shards);
#endif
      shard_mapping = mapping;
    }

    //--------------------------------------------------------------------------
    void ShardManager::set_address_spaces(
                                      const std::vector<AddressSpaceID> &spaces)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(address_spaces == NULL);
      assert(collective_mapping == NULL);
#endif
      address_spaces = new ShardMapping(spaces);
      address_spaces->add_reference();
      // We just need the collective radix, but use the existing routine
      int collective_radix = runtime->legion_collective_radix;
      int collective_log_radix, collective_stages;
      int participating_spaces, collective_last_radix;
      configure_collective_settings(spaces.size(), runtime->address_space,
          collective_radix, collective_log_radix, collective_stages,
          participating_spaces, collective_last_radix);
      collective_mapping = new CollectiveMapping(spaces, collective_radix);
      collective_mapping->add_reference();
    }

    //--------------------------------------------------------------------------
    void ShardManager::create_callback_barrier(size_t arrival_count)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!callback_barrier.exists());
      assert(owner_space == runtime->address_space);
      assert(arrival_count == runtime->total_address_spaces);
#endif
      callback_barrier = 
        RtBarrier(Realm::Barrier::create_barrier(arrival_count));
    }

    //--------------------------------------------------------------------------
    ShardTask* ShardManager::create_shard(ShardID id, Processor target)
    //--------------------------------------------------------------------------
    {
      ShardTask *shard = new ShardTask(runtime, this, id, target);
      local_shards.push_back(shard);
      return shard;
    }

    //--------------------------------------------------------------------------
    void ShardManager::launch(const std::vector<bool> &virtual_mapped)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!local_shards.empty());
      assert(address_spaces == NULL);
      assert(collective_mapping == NULL);
      assert(original_task->regions.size() == virtual_mapped.size());
#endif
      address_spaces = new ShardMapping();
      address_spaces->add_reference();
      address_spaces->resize(local_shards.size());
      // Sort the shards into their target address space
      std::map<AddressSpaceID,std::vector<ShardTask*> > shard_groups;
      for (std::vector<ShardTask*>::const_iterator it = 
            local_shards.begin(); it != local_shards.end(); it++)
      {
        const AddressSpaceID target = 
          runtime->find_address_space((*it)->target_proc);
        shard_groups[target].push_back(*it); 
#ifdef DEBUG_LEGION
        assert((*it)->shard_id < address_spaces->size());
#endif
        (*address_spaces)[(*it)->shard_id] = target;
      }
      local_shards.clear();
      {
        // We just need the collective radix, but use the existing routine
        int collective_radix = runtime->legion_collective_radix;
        int collective_log_radix, collective_stages;
        int participating_spaces, collective_last_radix;
        configure_collective_settings(address_spaces->size(), 
            runtime->address_space, collective_radix, collective_log_radix,
            collective_stages, participating_spaces, collective_last_radix);
        collective_mapping = 
          new CollectiveMapping(*address_spaces, collective_radix);
        collective_mapping->add_reference();
      }
      // Compute the unique shard spaces and make callback barrier
      // which has as many arrivers as unique shard spaces
      callback_barrier = 
        RtBarrier(Realm::Barrier::create_barrier(shard_groups.size()));
      // Make initial equivalence sets for each of the mapped regions
      mapped_equivalence_dids.resize(virtual_mapped.size());
      for (unsigned idx = 0; idx < mapped_equivalence_dids.size(); idx++)
        mapped_equivalence_dids[idx] = runtime->get_available_distributed_id();
      // Now either send the shards to the remote nodes or record them locally
      for (std::map<AddressSpaceID,std::vector<ShardTask*> >::const_iterator 
            it = shard_groups.begin(); it != shard_groups.end(); it++)
      {
        if (it->first != runtime->address_space)
        {
          distribute_shards(it->first, it->second); 
          // Clean up the shards that are now sent remotely
          for (unsigned idx = 0; idx < it->second.size(); idx++)
            delete it->second[idx];
        }
        else
          local_shards = it->second;
      }
      if (!local_shards.empty())
      {
        for (std::vector<ShardTask*>::const_iterator it = 
              local_shards.begin(); it != local_shards.end(); it++)
          launch_shard(*it);
      }
    }

    //--------------------------------------------------------------------------
    void ShardManager::distribute_shards(AddressSpaceID target,
                                         const std::vector<ShardTask*> &shards)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(address_spaces != NULL);
#endif
      Serializer rez;
      {
        RezCheck z(rez);
        rez.serialize(repl_id);
        rez.serialize(shard_domain);
        rez.serialize(total_shards);
        rez.serialize(isomorphic_points);
        if (isomorphic_points)
        {
          for (unsigned idx = 0; idx < total_shards; idx++)
            rez.serialize(shard_points[idx]);
        }
        else
        {
          for (unsigned idx = 0; idx < total_shards; idx++)
          {
            rez.serialize(sorted_points[idx]);
            rez.serialize(shard_lookup[idx]);
          }
        }
        rez.serialize(control_replicated);
        rez.serialize(top_level_task);
        rez.serialize(shard_task_barrier);
        address_spaces->pack_mapping(rez);
        if (control_replicated)
        {
#ifdef DEBUG_LEGION
          assert(callback_barrier.exists());
          assert(shard_mapping.size() == total_shards);
#endif
          rez.serialize(callback_barrier);
          for (std::vector<Processor>::const_iterator it = 
                shard_mapping.begin(); it != shard_mapping.end(); it++)
            rez.serialize(*it);
        }
        rez.serialize<size_t>(shards.size());
        rez.serialize<size_t>(mapped_equivalence_dids.size());
        for (std::vector<DistributedID>::const_iterator it = 
              mapped_equivalence_dids.begin(); it != 
              mapped_equivalence_dids.end(); it++)
          rez.serialize(*it);
        for (std::vector<ShardTask*>::const_iterator it = 
              shards.begin(); it != shards.end(); it++)
        {
          rez.serialize((*it)->shard_id);
          rez.serialize((*it)->target_proc);
          (*it)->pack_task(rez, target);
        }
      }
      runtime->send_replicate_launch(target, rez);
      // Update the remote constituents count
      remote_constituents++;
    }

    //--------------------------------------------------------------------------
    void ShardManager::unpack_shards_and_launch(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(owner_space != runtime->address_space);
      assert(local_shards.empty());
      assert(address_spaces == NULL);
      assert(collective_mapping == NULL);
#endif
      address_spaces = new ShardMapping();
      address_spaces->add_reference();
      address_spaces->unpack_mapping(derez);
      {
        // We just need the collective radix, but use the existing routine
        int collective_radix = runtime->legion_collective_radix;
        int collective_log_radix, collective_stages;
        int participating_spaces, collective_last_radix;
        configure_collective_settings(address_spaces->size(), 
            runtime->address_space, collective_radix, collective_log_radix, 
            collective_stages, participating_spaces, collective_last_radix);
        collective_mapping = 
          new CollectiveMapping(*address_spaces, collective_radix);
        collective_mapping->add_reference();
      }
      if (control_replicated)
      {
        derez.deserialize(callback_barrier);
        shard_mapping.resize(total_shards);
        for (unsigned idx = 0; idx < total_shards; idx++)
          derez.deserialize(shard_mapping[idx]);
      }
      size_t num_shards;
      derez.deserialize(num_shards);
      size_t num_equivalence_dids;
      derez.deserialize(num_equivalence_dids);
      mapped_equivalence_dids.resize(num_equivalence_dids);
      for (unsigned idx = 0; idx < num_equivalence_dids; idx++)
        derez.deserialize(mapped_equivalence_dids[idx]);
      local_shards.resize(num_shards);
      for (unsigned idx = 0; idx < num_shards; idx++)
      {
        ShardID shard_id;
        derez.deserialize(shard_id);
        Processor target;
        derez.deserialize(target);
        ShardTask *shard = new ShardTask(runtime, this, shard_id, target);
        std::set<RtEvent> ready_preconditions;
        shard->unpack_task(derez, target, ready_preconditions);
        local_shards[idx] = shard;
        if (!ready_preconditions.empty())
          launch_shard(shard, Runtime::merge_events(ready_preconditions));
        else
          launch_shard(shard);
      }
    }

    //--------------------------------------------------------------------------
    void ShardManager::launch_shard(ShardTask *task, RtEvent precondition) const
    //--------------------------------------------------------------------------
    {
      ShardManagerLaunchArgs args(task);
      runtime->issue_runtime_meta_task(args, LG_LATENCY_WORK_PRIORITY, 
                                       precondition);
    }

    //--------------------------------------------------------------------------
    EquivalenceSet* ShardManager::get_initial_equivalence_set(unsigned idx,
                                    LogicalRegion handle, InnerContext *context)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(idx < mapped_equivalence_dids.size());
#endif
      RegionNode *region = runtime->forest->get_node(handle);
      bool dummy_first;
      return deduplicate_equivalence_set_creation(region, context,
                        mapped_equivalence_dids[idx], dummy_first);
    }

    //--------------------------------------------------------------------------
    EquivalenceSet* ShardManager::deduplicate_equivalence_set_creation(
                                RegionNode *region_node, InnerContext *context,
                                DistributedID did, bool &first)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(collective_mapping != NULL);
#endif
      const AddressSpaceID owner_space = runtime->determine_owner(did);
      EquivalenceSet *result = NULL;
      if (local_shards.size() > 1)
      {
        AutoLock m_lock(manager_lock);
        // See if we already have this here or not
        std::map<DistributedID,std::pair<EquivalenceSet*,size_t> >::iterator
          finder = created_equivalence_sets.find(did);
        if (finder != created_equivalence_sets.end())
        {
          result = finder->second.first;
#ifdef DEBUG_LEGION
          assert(finder->second.second > 0);
#endif
          if (--finder->second.second == 0)
            created_equivalence_sets.erase(finder);
          first = false;
          return result;
        }
        // Didn't find it so make it
        result = new EquivalenceSet(runtime, did, owner_space, owner_space,
              region_node, context, true/*register now*/, collective_mapping);
        // This adds as many context refs as there are shards
        result->initialize_collective_references(local_shards.size());
        // Record it for the shards that come later
        std::pair<EquivalenceSet*,size_t> &pending = 
          created_equivalence_sets[did];
        pending.first = result;
        pending.second = local_shards.size() - 1;
      }
      else // Only one shard here on this node so just make it
      {
        result = new EquivalenceSet(runtime, did, owner_space, owner_space,
              region_node, context, true/*register now*/, collective_mapping);
        // This adds as many context refs as there are shards
        result->initialize_collective_references(1/*local shard count*/);
      }
      first = true;
      return result;
    }

    //--------------------------------------------------------------------------
    FillView* ShardManager::deduplicate_fill_view_creation(DistributedID did,
                                                     FillOp *op, bool &set_view)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!set_view);
#endif
      if (local_shards.size() > 1)
      {
        FillView *result = NULL;
        AutoLock m_lock(manager_lock);
        // See if we already have this here or not
        std::map<DistributedID,std::pair<FillView*,size_t> >::iterator
          finder = created_fill_views.find(did);
        if (finder != created_fill_views.end())
        {
          result = finder->second.first;
#ifdef DEBUG_LEGION
          assert(finder->second.second > 0);
#endif
          if (--finder->second.second == 0)
          {
            created_fill_views.erase(finder);
            set_view = true;
          }
          return result;
        }
        const AddressSpaceID owner_space = runtime->determine_owner(did);
        result = new FillView(runtime->forest, did, owner_space,
#ifdef LEGION_SPY
                       op->get_unique_op_id(),
#endif
                       true/*register now*/, collective_mapping);
        // Record it for the shards that come later
        std::pair<FillView*,size_t> &pending = created_fill_views[did];
        pending.first = result;
        pending.second = local_shards.size() - 1;
        return result;
      }
      else
      {
        const AddressSpaceID owner_space = runtime->determine_owner(did);
        FillView *fill_view = 
          new FillView(runtime->forest, did, owner_space,
#ifdef LEGION_SPY
                       op->get_unique_op_id(),
#endif
                       true/*register now*/, collective_mapping);
        // Only one shard so do the setting
        set_view = true;
        return fill_view;
      }
    }

    //--------------------------------------------------------------------------
    void ShardManager::deduplicate_attaches(const IndexAttachLauncher &launcher,
                                            std::vector<unsigned> &indexes)
    //--------------------------------------------------------------------------
    {
      // If we only have one shard then there is no need to deduplicate
      if (local_shards.size() == 1)
      {
        indexes.resize(launcher.handles.size());
        for (unsigned idx = 0; idx < indexes.size(); idx++)
          indexes[idx] = idx;
        return;
      }
      // If we have multiple local shards then try to deduplicate across them
      RtEvent wait_on;
      RtUserEvent to_trigger;
      {
        AutoLock m_lock(manager_lock);
        if (attach_deduplication == NULL)
          attach_deduplication = new AttachDeduplication();
        if (attach_deduplication->launchers.empty())
        {
#ifdef DEBUG_LEGION
          assert(!attach_deduplication->pending.exists());
#endif
          attach_deduplication->pending = Runtime::create_rt_user_event();
        }
        attach_deduplication->launchers.push_back(&launcher);
        if (attach_deduplication->launchers.size() == local_shards.size())
        {
#ifdef DEBUG_LEGION
          assert(attach_deduplication->pending.exists());
#endif
          to_trigger = attach_deduplication->pending;
          // Make a new event for signaling when we are done
          attach_deduplication->pending = Runtime::create_rt_user_event();
        }
        else
          wait_on = attach_deduplication->pending;
      }
      if (to_trigger.exists())
      {
        // Before triggering, do the compuation to figure out which shard
        // is going to own any duplicates, do this by cutting across using
        // snake order of the shards to try and balance them
        bool done = false;
        unsigned index = 0;
        while (!done)
        {
          done = true;
          if ((index % 2) == 0)
          {
            for (unsigned idx = 0; 
                  idx < attach_deduplication->launchers.size(); idx++)
            {
              const IndexAttachLauncher *next = 
                (attach_deduplication->launchers[idx]); 
              if (index >= next->handles.size())
                continue;
              done = false;
              const LogicalRegion handle = next->handles[index];
              if (attach_deduplication->owners.find(handle) ==
                  attach_deduplication->owners.end())
                attach_deduplication->owners.insert(
                    std::make_pair(handle, next));
            }
          }
          else
          {
            for (int idx = (attach_deduplication->launchers.size() - 1);
                  idx >= 0; idx--)
            {
              const IndexAttachLauncher *next = 
                (attach_deduplication->launchers[idx]); 
              if (index >= next->handles.size())
                continue;
              done = false;
              const LogicalRegion handle = next->handles[index];
              if (attach_deduplication->owners.find(handle) ==
                  attach_deduplication->owners.end())
                attach_deduplication->owners.insert(
                    std::make_pair(handle, next));
            }
          }
          index++;
        }
        Runtime::trigger_event(to_trigger);
        to_trigger = RtUserEvent::NO_RT_USER_EVENT;
      }
      if (wait_on.exists() && !wait_on.has_triggered())
        wait_on.wait();
      // Once we're here, all the launchers can be accessed read-only 
      // Figure out which of our handles we still own
      for (unsigned idx = 0; idx < launcher.handles.size(); idx++)
      {
        const LogicalRegion handle = launcher.handles[idx];
        std::map<LogicalRegion,const IndexAttachLauncher*>::const_iterator
          finder = attach_deduplication->owners.find(handle);
#ifdef DEBUG_LEGION
        assert(finder != attach_deduplication->owners.end());
#endif
        // Only add it if we own it
        if (finder->second == &launcher)
          indexes.push_back(idx);
      }
      // When we're done we need to sync on the way out too to make sure
      // everyone is done accessing our launcher before we leave
      {
        AutoLock m_lock(manager_lock);
#ifdef DEBUG_LEGION
        assert(attach_deduplication->done_count < local_shards.size());
#endif
        attach_deduplication->done_count++;
        if (attach_deduplication->done_count == local_shards.size())
          to_trigger = attach_deduplication->pending;
        else
          wait_on = attach_deduplication->pending;
      }
      if (to_trigger.exists())
      {
        // Need to clean up first
        delete attach_deduplication;
        attach_deduplication = NULL;
        __sync_synchronize();
        Runtime::trigger_event(to_trigger);
      }
      if (wait_on.exists() && !wait_on.has_triggered())
        wait_on.wait();
    }

    //--------------------------------------------------------------------------
    bool ShardManager::is_total_sharding(void)
    //--------------------------------------------------------------------------
    {
      AutoLock m_lock(manager_lock);
      if (unique_shard_spaces.empty())
        for (unsigned shard = 0; shard < total_shards; shard++)
              unique_shard_spaces.insert((*address_spaces)[shard]);
      return (unique_shard_spaces.size() == runtime->total_address_spaces);
    }

    //--------------------------------------------------------------------------
    void ShardManager::exchange_shard_local_op_data(size_t context_index,
                                                    size_t exchange_index,
                                                    const void *data,
                                                    size_t size)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!local_shards.empty());
#endif
      if (local_shards.size() == 1)
        return;
      RtUserEvent to_trigger;
      const std::pair<size_t,size_t> key(context_index, exchange_index);
      {
        AutoLock m_lock(manager_lock);
        ShardLocalData &result = shard_local_data[key];
        result.buffer = malloc(size);
        memcpy(result.buffer, data, size);
        result.size = size;
        result.remaining = local_shards.size() - 1;
        to_trigger = result.pending;
      }
      if (to_trigger.exists())
        Runtime::trigger_event(to_trigger);
    }

    //--------------------------------------------------------------------------
    void ShardManager::find_shard_local_op_data(size_t context_index,
                                                size_t exchange_index,
                                                void *result, size_t size)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(local_shards.size() > 1);
#endif
      RtEvent wait_on;
      const std::pair<size_t,size_t> key(context_index, exchange_index);
      {
        AutoLock m_lock(manager_lock);
        ShardLocalData &data = shard_local_data[key];
        if (data.remaining == 0)
        {
          // here before the sender
          if (!data.pending.exists())
            data.pending = Runtime::create_rt_user_event();
          wait_on = data.pending;
        }
        else
        {
#ifdef DEBUG_LEGION
          assert(size == data.size);
#endif
          memcpy(result, data.buffer, data.size);
          if ((--data.remaining) == 0)
          {
            free(data.buffer);
            shard_local_data.erase(key);
          }
          return;
        }
      }
      if (!wait_on.has_triggered())
        wait_on.wait();
      AutoLock m_lock(manager_lock);
      std::map<std::pair<size_t,size_t>,ShardLocalData>::iterator finder =
        shard_local_data.find(key);
#ifdef DEBUG_LEGION
      assert(finder != shard_local_data.end());
      assert(finder->second.remaining > 0);
      assert(size == finder->second.size);
#endif
      memcpy(result, finder->second.buffer, finder->second.size);
      if ((--finder->second.remaining) == 0)
      {
        free(finder->second.buffer);
        shard_local_data.erase(finder);
      }
    }

    //--------------------------------------------------------------------------
    void ShardManager::barrier_shard_local(size_t context_index,
                                           size_t exchange_index)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!local_shards.empty());
#endif
      if (local_shards.size() == 1)
        return;
      RtEvent wait_on;
      const std::pair<size_t,size_t> key(context_index, exchange_index);
      {
        AutoLock m_lock(manager_lock);
        std::map<std::pair<size_t,size_t>,ShardLocalData>::iterator finder =
          shard_local_data.find(key);
        if (finder != shard_local_data.end())
        {
#ifdef DEBUG_LEGION
          assert(finder->second.remaining > 0);
          assert(finder->second.pending.exists());
#endif
          finder->second.remaining--;
          if (finder->second.remaining == 0)
          {
            Runtime::trigger_event(finder->second.pending);
            shard_local_data.erase(finder);
            return;
          }
          else
            wait_on = finder->second.pending;
        }
        else
        {
          ShardLocalData &data = shard_local_data[key];
          data.pending = Runtime::create_rt_user_event();
          data.remaining = local_shards.size() - 1;
          wait_on = data.pending;
        }
      }
      if (!wait_on.has_triggered())
        wait_on.wait();
    }

    //--------------------------------------------------------------------------
    void ShardManager::handle_post_mapped(bool local, RtEvent precondition)
    //--------------------------------------------------------------------------
    {
      bool notify = false;   
      {
        AutoLock m_lock(manager_lock);
        if (precondition.exists())
          mapping_preconditions.insert(precondition);
        if (local)
        {
          local_mapping_complete++;
#ifdef DEBUG_LEGION
          assert(local_mapping_complete <= local_shards.size());
#endif
        }
        else
        {
          remote_mapping_complete++;
#ifdef DEBUG_LEGION
          assert(remote_mapping_complete <= remote_constituents);
#endif
        }
        notify = (local_mapping_complete == local_shards.size()) &&
                 (remote_mapping_complete == remote_constituents);
      }
      if (notify)
      {
        RtEvent mapped_precondition;
        if (!mapping_preconditions.empty())
          mapped_precondition = Runtime::merge_events(mapping_preconditions);
        if (original_task == NULL)
        {
          Serializer rez;
          rez.serialize(repl_id);
          rez.serialize(mapped_precondition);
          runtime->send_replicate_post_mapped(owner_space, rez);
        }
        else
          original_task->handle_post_mapped(false/*deferral*/, 
                                            mapped_precondition);
      }
    }

    //--------------------------------------------------------------------------
    void ShardManager::handle_post_execution(FutureInstance *inst, 
                                    void *metadata, size_t metasize, bool local)
    //--------------------------------------------------------------------------
    {
      bool notify = false;
      {
        AutoLock m_lock(manager_lock);
        if (local)
        {
          local_execution_complete++;
#ifdef DEBUG_LEGION
          assert(local_execution_complete <= local_shards.size());
#endif
        }
        else
        {
          remote_execution_complete++;
#ifdef DEBUG_LEGION
          assert(remote_execution_complete <= remote_constituents);
#endif
        }
        notify = (local_execution_complete == local_shards.size()) &&
                 (remote_execution_complete == remote_constituents);
        // See if we need to save the future or compare it
        if (inst != NULL)
        {
          if (local_future_result == NULL)
          {
            local_future_result = inst;
            inst = NULL;
          }
#ifdef DEBUG_LEGION
          // In debug mode we'll do a comparison to see if the futures
          // are bit-wise the same or not and issue a warning if not
          else if (local_future_result->size != inst->size)
            REPORT_LEGION_WARNING(LEGION_WARNING_MISMATCHED_REPLICATED_FUTURES,
                                  "WARNING: futures returned from control "
                                  "replicated task %s have different sizes!",
                                  local_shards[0]->get_task_name())
#endif
        }
      }
      if (notify)
      {
        FutureInstance *result = local_future_result;
        local_future_result = NULL;
        if (original_task == NULL)
        {
          Serializer rez;
          rez.serialize(repl_id);
          if (result != NULL)
            result->pack_instance(rez, true/*ownership*/);
          else
            rez.serialize<size_t>(0);
          rez.serialize(metasize);
          if (metasize > 0)
            rez.serialize(metadata, metasize);
          runtime->send_replicate_post_execution(owner_space, rez);
          if (result != NULL)
            delete result;
        }
        else
        {
          original_task->handle_future(result, metadata, metasize,
              NULL/*functor*/, Processor::NO_PROC, false/*own functor*/);
          // we no longer own this, it got passed through
          metadata = NULL;
        }
      }
      if (inst != NULL)
        delete inst;
      if (metadata != NULL)
        free(metadata);
    }

    //--------------------------------------------------------------------------
    RtEvent ShardManager::trigger_task_complete(bool local, ApEvent effects) 
    //--------------------------------------------------------------------------
    {
      bool notify = false;
      {
        AutoLock m_lock(manager_lock);
        if (local)
        {
          trigger_local_complete++;
#ifdef DEBUG_LEGION
          assert(trigger_local_complete <= local_shards.size());
#endif
        }
        else
        {
          trigger_remote_complete++;
#ifdef DEBUG_LEGION
          assert(trigger_remote_complete <= remote_constituents);
#endif
        }
        if (effects.exists())
          shard_effects.insert(effects);
        notify = (trigger_local_complete == local_shards.size()) &&
                 (trigger_remote_complete == remote_constituents);
      }
      if (notify)
      {
        const ApEvent all_shard_effects =
          Runtime::merge_events(NULL, shard_effects);
        if (original_task == NULL)
        {
          const RtUserEvent done_event = Runtime::create_rt_user_event();
          Serializer rez;
          rez.serialize(repl_id);
          rez.serialize(all_shard_effects);
          rez.serialize(done_event);
          runtime->send_replicate_trigger_complete(owner_space, rez);
          return done_event;
        }
        else
        {
#ifdef DEBUG_LEGION
          assert(!local_shards.empty());
#endif
          // For one of the shards we either need to return resources up
          // the tree or report leaks and duplicates of resources.
          // All the shards have the same set so we only have to do this
          // for one of the shards.
          std::set<RtEvent> applied_events;
          if (original_task->is_top_level_task())
            local_shards[0]->report_leaks_and_duplicates(applied_events);
          else
            local_shards[0]->return_resources(
                original_task->get_context(), applied_events); 
          RtEvent applied_event;
          if (!applied_events.empty())
            applied_event = Runtime::merge_events(applied_events);
          original_task->complete_execution(applied_event);
          original_task->trigger_children_complete(all_shard_effects);
          return applied_event;
        }
      }
      return RtEvent::NO_RT_EVENT;
    }

    //--------------------------------------------------------------------------
    void ShardManager::trigger_task_commit(bool local)
    //--------------------------------------------------------------------------
    {
      bool notify = false;
      {
        AutoLock m_lock(manager_lock);
        if (local)
        {
          trigger_local_commit++;
#ifdef DEBUG_LEGION
          assert(trigger_local_commit <= local_shards.size());
#endif
        }
        else
        {
          trigger_remote_commit++;
#ifdef DEBUG_LEGION
          assert(trigger_remote_commit <= remote_constituents);
#endif
        }
        notify = (trigger_local_commit == local_shards.size()) &&
                 (trigger_remote_commit == remote_constituents);
      }
      if (notify)
      {
        if (original_task == NULL)
        {
          Serializer rez;
          rez.serialize(repl_id);
          runtime->send_replicate_trigger_commit(owner_space, rez);
        }
        else
          original_task->trigger_children_committed();
      }
    }

    //--------------------------------------------------------------------------
    void ShardManager::send_collective_message(ShardID target, Serializer &rez)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(target < address_spaces->size());
#endif
      AddressSpaceID target_space = (*address_spaces)[target];
      // Check to see if this is a local shard
      if (target_space == runtime->address_space)
      {
        Deserializer derez(rez.get_buffer(), rez.get_used_bytes());
        // Have to unpack the preample we already know
        ReplicationID local_repl;
        derez.deserialize(local_repl);
        handle_collective_message(derez);
      }
      else
        runtime->send_control_replicate_collective_message(target_space, rez);
    }

    //--------------------------------------------------------------------------
    void ShardManager::handle_collective_message(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      // Figure out which shard we are going to
      ShardID target;
      derez.deserialize(target);
      for (std::vector<ShardTask*>::const_iterator it = 
            local_shards.begin(); it != local_shards.end(); it++)
      {
        if ((*it)->shard_id == target)
        {
          (*it)->handle_collective_message(derez);
          return;
        }
      }
      // Should never get here
      assert(false);
    }

    //--------------------------------------------------------------------------
    void ShardManager::send_future_map_request(ShardID target, Serializer &rez)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(target < address_spaces->size());
#endif
      AddressSpaceID target_space = (*address_spaces)[target];
      // Check to see if this is a local shard
      if (target_space == runtime->address_space)
      {
        Deserializer derez(rez.get_buffer(), rez.get_used_bytes());
        // Have to unpack the preample we already know
        ReplicationID local_repl;
        derez.deserialize(local_repl);     
        handle_future_map_request(derez);
      }
      else
        runtime->send_control_replicate_future_map_request(target_space, rez);
    }

    //--------------------------------------------------------------------------
    void ShardManager::handle_future_map_request(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      // Figure out which shard we are going to
      ShardID target;
      derez.deserialize(target);
      for (std::vector<ShardTask*>::const_iterator it = 
            local_shards.begin(); it != local_shards.end(); it++)
      {
        if ((*it)->shard_id == target)
        {
          (*it)->handle_future_map_request(derez);
          return;
        }
      }
      // Should never get here
      assert(false);
    }

    //--------------------------------------------------------------------------
    void ShardManager::send_disjoint_complete_request(ShardID target, 
                                                      Serializer &rez)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(target < address_spaces->size());
#endif
      AddressSpaceID target_space = (*address_spaces)[target];
      // Check to see if this is a local shard
      if (target_space == runtime->address_space)
      {
        Deserializer derez(rez.get_buffer(), rez.get_used_bytes());
        // Have to unpack the preample we already know
        ReplicationID local_repl;
        derez.deserialize(local_repl);
        handle_disjoint_complete_request(derez);
      }
      else
        runtime->send_control_replicate_disjoint_complete_request(target_space,
                                                                  rez);
    }

    //--------------------------------------------------------------------------
    void ShardManager::handle_disjoint_complete_request(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      // Figure out which shard we are going to
      ShardID target;
      derez.deserialize(target);
      for (std::vector<ShardTask*>::const_iterator it = 
            local_shards.begin(); it != local_shards.end(); it++)
      {
        if ((*it)->shard_id == target)
        {
          (*it)->handle_disjoint_complete_request(derez);
          return;
        }
      }
      // Should never get here
      assert(false);
    }

    //--------------------------------------------------------------------------
    void ShardManager::send_intra_space_dependence(ShardID target, 
                                                   Serializer &rez)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(target < address_spaces->size());
#endif
      AddressSpaceID target_space = (*address_spaces)[target];
      // Check to see if this is a local shard
      if (target_space == runtime->address_space)
      {
        Deserializer derez(rez.get_buffer(), rez.get_used_bytes());
        // Have to unpack the preample we already know
        ReplicationID local_repl;
        derez.deserialize(local_repl);     
        handle_intra_space_dependence(derez);
      }
      else
        runtime->send_control_replicate_intra_space_dependence(target_space,
                                                               rez);
    }

    //--------------------------------------------------------------------------
    void ShardManager::handle_intra_space_dependence(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      // Figure out which shard we are going to
      ShardID target;
      derez.deserialize(target);
      for (std::vector<ShardTask*>::const_iterator it = 
            local_shards.begin(); it != local_shards.end(); it++)
      {
        if ((*it)->shard_id == target)
        {
          (*it)->handle_intra_space_dependence(derez);
          return;
        }
      }
      // Should never get here
      assert(false);
    }

#ifdef NO_EXPLICIT_COLLECTIVES
    //--------------------------------------------------------------------------
    void ShardManager::send_collective_instance_message(ShardID target,
                                                        Serializer &rez)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(target < address_spaces->size());
#endif
      AddressSpaceID target_space = (*address_spaces)[target];
      // Check to see if this is a local shard
      if (target_space == runtime->address_space)
      {
        Deserializer derez(rez.get_buffer(), rez.get_used_bytes());
        // Have to unpack the preample we already know
        ReplicationID local_repl;
        derez.deserialize(local_repl);     
        handle_collective_instance_message(derez);
      }
      else
        runtime->send_control_replicate_collective_instance_message(
                                                  target_space, rez);
    }

    //--------------------------------------------------------------------------
    void ShardManager::handle_collective_instance_message(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      // Figure out which shard we are going to
      ShardID target;
      derez.deserialize(target);
      for (std::vector<ShardTask*>::const_iterator it = 
            local_shards.begin(); it != local_shards.end(); it++)
      {
        if ((*it)->shard_id == target)
        {
          (*it)->handle_collective_instance_message(derez);
          return;
        }
      }
      // Should never get here
      assert(false);
    }
#endif

    //--------------------------------------------------------------------------
    void ShardManager::broadcast_resource_update(ShardTask *source, 
                             Serializer &rez, std::set<RtEvent> &applied_events)
    //--------------------------------------------------------------------------
    {
      broadcast_message(source, rez, RESOURCE_UPDATE_KIND, applied_events); 
    }

    //--------------------------------------------------------------------------
    void ShardManager::broadcast_created_region_contexts(ShardTask *source, 
                             Serializer &rez, std::set<RtEvent> &applied_events)
    //--------------------------------------------------------------------------
    {
      broadcast_message(source, rez, CREATED_REGION_UPDATE_KIND,applied_events); 
    }

    //--------------------------------------------------------------------------
    void ShardManager::broadcast_message(ShardTask *source, Serializer &rez,
                   BroadcastMessageKind kind, std::set<RtEvent> &applied_events)
    //--------------------------------------------------------------------------
    {
      std::vector<AddressSpaceID> shard_spaces;
      {
        AutoLock m_lock(manager_lock);
        if (unique_shard_spaces.empty())
          for (unsigned shard = 0; shard < total_shards; shard++)
                unique_shard_spaces.insert((*address_spaces)[shard]);
        shard_spaces.insert(shard_spaces.end(), 
            unique_shard_spaces.begin(), unique_shard_spaces.end());
      }
      // First pack it out and send it out to any remote nodes 
      if (shard_spaces.size() > 1)
      {
        // Find the start index
        int start_idx = -1;
        for (unsigned idx = 0; idx < shard_spaces.size(); idx++)
        {
          if (shard_spaces[idx] != runtime->address_space)
            continue;
          start_idx = idx;
          break;
        }
#ifdef DEBUG_LEGION
        assert(start_idx >= 0);
#endif
        std::vector<unsigned> locals;
        std::vector<AddressSpaceID> targets;
        for (int idx = 0; idx < runtime->legion_collective_radix; idx++)
        {
          unsigned next = idx + 1;
          if (next >= shard_spaces.size())
            break;
          locals.push_back(next);
          // Convert from relative to actual address space
          const unsigned next_index = (start_idx + next) % shard_spaces.size();
          targets.push_back(shard_spaces[next_index]);
        }
        for (unsigned idx = 0; idx < locals.size(); idx++)
        {
          RtEvent next_done = Runtime::create_rt_user_event();
          Serializer rez2;
          rez2.serialize(repl_id);
          rez2.serialize<unsigned>(start_idx);
          rez2.serialize<unsigned>(locals[idx]);
          rez2.serialize(kind);
          rez2.serialize<size_t>(rez.get_used_bytes());
          rez2.serialize(rez.get_buffer(), rez.get_used_bytes());
          rez2.serialize(next_done);
          runtime->send_control_replicate_broadcast_update(targets[idx], rez2);
          applied_events.insert(next_done);
        }
      }
      // Then send it to any other local shards
      for (std::vector<ShardTask*>::const_iterator it =
            local_shards.begin(); it != local_shards.end(); it++)
      {
        // Skip the source since that's where it came from
        if ((*it) == source)
          continue;
        Deserializer derez(rez.get_buffer(), rez.get_used_bytes());
        switch (kind)
        {
          case RESOURCE_UPDATE_KIND:
            {
              (*it)->handle_resource_update(derez, applied_events);
              break;
            }
          case CREATED_REGION_UPDATE_KIND:
            {
              (*it)->handle_created_region_contexts(derez, applied_events);
              break;
            }
          default:
            assert(false);
        }
      }
    }

    //--------------------------------------------------------------------------
    void ShardManager::handle_broadcast(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      unsigned start_idx, local_idx;
      derez.deserialize(start_idx);
      derez.deserialize(local_idx);
      BroadcastMessageKind kind;
      derez.deserialize(kind);
      size_t message_size;
      derez.deserialize(message_size);
      const void *message = derez.get_current_pointer();
      derez.advance_pointer(message_size);
      RtUserEvent done_event;
      derez.deserialize(done_event);
      // Send out any remote updates first
      std::vector<AddressSpaceID> shard_spaces;
      {
        AutoLock m_lock(manager_lock);
        if (unique_shard_spaces.empty())
          for (unsigned shard = 0; shard < total_shards; shard++)
                unique_shard_spaces.insert((*address_spaces)[shard]);
        shard_spaces.insert(shard_spaces.end(), 
            unique_shard_spaces.begin(), unique_shard_spaces.end());
      }
      // First pack it out and send it out to any remote nodes 
      std::vector<unsigned> locals;
      std::vector<AddressSpaceID> targets;
      const unsigned start = local_idx * runtime->legion_collective_radix + 1;
      for (int idx = 0; idx < runtime->legion_collective_radix; idx++)
      {
        unsigned next = start + idx;
        if (next >= shard_spaces.size())
          break;
        locals.push_back(next);
        // Convert from relative to actual address space
        const unsigned next_index = (start_idx + next) % shard_spaces.size();
        targets.push_back(shard_spaces[next_index]);
      }
      std::set<RtEvent> remote_handled;
      if (!targets.empty())
      {
        for (unsigned idx = 0; idx < targets.size(); idx++)
        {
          RtEvent next_done = Runtime::create_rt_user_event();
          Serializer rez;
          rez.serialize(repl_id);
          rez.serialize<unsigned>(start_idx);
          rez.serialize<unsigned>(locals[idx]);
          rez.serialize(kind);
          rez.serialize<size_t>(message_size);
          rez.serialize(message, message_size);
          rez.serialize(next_done);
          runtime->send_control_replicate_broadcast_update(targets[idx], rez);
          remote_handled.insert(next_done);
        } 
      }
      // Handle it on all our local shards
      for (std::vector<ShardTask*>::const_iterator it =
            local_shards.begin(); it != local_shards.end(); it++)
      {
        Deserializer derez2(message, message_size);
        switch (kind)
        {
          case RESOURCE_UPDATE_KIND:
            {
              (*it)->handle_resource_update(derez2, remote_handled);
              break;
            }
          case CREATED_REGION_UPDATE_KIND:
            {
              (*it)->handle_created_region_contexts(derez2, remote_handled);
              break;
            }
          default:
            assert(false);
        }
      }
      if (!remote_handled.empty())
        Runtime::trigger_event(done_event, 
            Runtime::merge_events(remote_handled));
      else
        Runtime::trigger_event(done_event);
    }

    //--------------------------------------------------------------------------
    void ShardManager::send_trace_event_request(
        ShardedPhysicalTemplate *physical_template, ShardID shard_source, 
        AddressSpaceID template_source, size_t template_index, ApEvent event,
        AddressSpaceID event_space, RtUserEvent done_event)
    //--------------------------------------------------------------------------
    {
      // See whether we are on the right node to handle this request, if not
      // then forward the request onto the proper node
      if (event_space != runtime->address_space)
      {
#ifdef DEBUG_LEGION
        assert(template_source == runtime->address_space);
#endif
        // Check to see if we have a shard on that address space, if not
        // then we know that this event can't have come from there
        bool found = false;
        for (unsigned idx = 0; idx < address_spaces->size(); idx++)
        {
          if ((*address_spaces)[idx] != event_space)
            continue;
          found = true;
          break;
        }
        if (found)
        {
          Serializer rez;
          {
            RezCheck z(rez);
            rez.serialize(repl_id);
            rez.serialize(physical_template);
            rez.serialize(template_index);
            rez.serialize(shard_source);
            rez.serialize(event);
            rez.serialize(done_event);
          }
          runtime->send_control_replicate_trace_event_request(event_space, rez);
        }
        else
          send_trace_event_response(physical_template, template_source,
              event, ApBarrier::NO_AP_BARRIER, done_event);
      }
      else
      {
        // Ask each of our local shards to check for the event in the template
        for (std::vector<ShardTask*>::const_iterator it = 
              local_shards.begin(); it != local_shards.end(); it++)
        {
          const ApBarrier result = 
            (*it)->handle_find_trace_shard_event(template_index, 
                                                 event, shard_source);
          // If we found it then we are done
          if (result.exists())
          {
            send_trace_event_response(physical_template, template_source,
                event, result, done_event);
            return;
          }
        }
        // If we make it here then we didn't find it so return the result
        send_trace_event_response(physical_template, template_source,
            event, ApBarrier::NO_AP_BARRIER, done_event);
      }
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_trace_event_request(
                   Deserializer &derez, Runtime *runtime, AddressSpaceID source)
    //--------------------------------------------------------------------------
    {
      DerezCheck z(derez);
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ShardedPhysicalTemplate *physical_template;
      derez.deserialize(physical_template);
      size_t template_index;
      derez.deserialize(template_index);
      ShardID shard_source;
      derez.deserialize(shard_source);
      ApEvent event;
      derez.deserialize(event);
      RtUserEvent done_event;
      derez.deserialize(done_event);

      ShardManager *manager = runtime->find_shard_manager(repl_id);
      manager->send_trace_event_request(physical_template, shard_source, source,
          template_index, event, runtime->address_space, done_event);
    }

    //--------------------------------------------------------------------------
    void ShardManager::send_trace_event_response(
        ShardedPhysicalTemplate *physical_template, AddressSpaceID temp_source,
        ApEvent event, ApBarrier result, RtUserEvent done_event)
    //--------------------------------------------------------------------------
    {
      if (temp_source != runtime->address_space)
      {
        // Not local so send the response message
        Serializer rez;
        {
          RezCheck z(rez);
          rez.serialize(physical_template);
          rez.serialize(event);
          rez.serialize(result);
          rez.serialize(done_event);
        }
        runtime->send_control_replicate_trace_event_response(temp_source, rez);
      }
      else // This is local so handle it here
      {
        physical_template->record_trace_shard_event(event, result);
        Runtime::trigger_event(done_event);
      }
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_trace_event_response(
                                                            Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      DerezCheck z(derez);
      ShardedPhysicalTemplate *physical_template;
      derez.deserialize(physical_template);
      ApEvent event;
      derez.deserialize(event);
      ApBarrier result;
      derez.deserialize(result);
      RtUserEvent done_event;
      derez.deserialize(done_event);

      physical_template->record_trace_shard_event(event, result);
      Runtime::trigger_event(done_event);
    }

    //--------------------------------------------------------------------------
    void ShardManager::send_trace_frontier_request(
        ShardedPhysicalTemplate *physical_template, ShardID shard_source, 
        AddressSpaceID template_source, size_t template_index, ApEvent event,
        AddressSpaceID event_space, unsigned frontier, RtUserEvent done_event)
    //--------------------------------------------------------------------------
    {
      // See whether we are on the right node to handle this request, if not
      // then forward the request onto the proper node
      if (event_space != runtime->address_space)
      {
#ifdef DEBUG_LEGION
        assert(template_source == runtime->address_space);
#endif
        // Check to see if we have a shard on that address space, if not
        // then we know that this event can't have come from there
        bool found = false;
        for (unsigned idx = 0; idx < address_spaces->size(); idx++)
        {
          if ((*address_spaces)[idx] != event_space)
            continue;
          found = true;
          break;
        }
        if (found)
        {
          Serializer rez;
          {
            RezCheck z(rez);
            rez.serialize(repl_id);
            rez.serialize(physical_template);
            rez.serialize(template_index);
            rez.serialize(shard_source);
            rez.serialize(event);
            rez.serialize(frontier);
            rez.serialize(done_event);
          }
          runtime->send_control_replicate_trace_frontier_request(event_space,
                                                                 rez);
        }
        else
          send_trace_frontier_response(physical_template, template_source,
                          frontier, ApBarrier::NO_AP_BARRIER, done_event);
      }
      else
      {
        // Ask each of our local shards to check for the event in the template
        for (std::vector<ShardTask*>::const_iterator it = 
              local_shards.begin(); it != local_shards.end(); it++)
        {
          const ApBarrier result =
            (*it)->handle_find_trace_shard_frontier(template_index, 
                                                    event, shard_source);
          // If we found it then we are done
          if (result.exists())
          {
            send_trace_frontier_response(physical_template, template_source,
                                         frontier, result, done_event);
            return;
          }
        }
        // If we couldn't find it then send back a NO_BARRIER
        send_trace_frontier_response(physical_template, template_source,
                        frontier, ApBarrier::NO_AP_BARRIER, done_event);
      }
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_trace_frontier_request(
                   Deserializer &derez, Runtime *runtime, AddressSpaceID source)
    //--------------------------------------------------------------------------
    {
      DerezCheck z(derez);
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ShardedPhysicalTemplate *physical_template;
      derez.deserialize(physical_template);
      size_t template_index;
      derez.deserialize(template_index);
      ShardID shard_source;
      derez.deserialize(shard_source);
      ApEvent event;
      derez.deserialize(event);
      unsigned frontier;
      derez.deserialize(frontier);
      RtUserEvent done_event;
      derez.deserialize(done_event);

      ShardManager *manager = runtime->find_shard_manager(repl_id);
      manager->send_trace_frontier_request(physical_template, shard_source,
          source, template_index, event, runtime->address_space, frontier,
          done_event);
    }

    //--------------------------------------------------------------------------
    void ShardManager::send_trace_frontier_response(
        ShardedPhysicalTemplate *physical_template, AddressSpaceID temp_source,
        unsigned frontier, ApBarrier result, RtUserEvent done_event)
    //--------------------------------------------------------------------------
    {
      if (temp_source != runtime->address_space)
      {
        // Not local so send the response message
        Serializer rez;
        {
          RezCheck z(rez);
          rez.serialize(physical_template);
          rez.serialize(frontier);
          rez.serialize(result);
          rez.serialize(done_event);
        }
        runtime->send_control_replicate_trace_frontier_response(temp_source,
                                                                rez);
      }
      else // This is local so handle it here
      {
        physical_template->record_trace_shard_frontier(frontier, result);
        Runtime::trigger_event(done_event);
      }
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_trace_frontier_response(
                                                            Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      DerezCheck z(derez);
      ShardedPhysicalTemplate *physical_template;
      derez.deserialize(physical_template);
      unsigned frontier;
      derez.deserialize(frontier);
      ApBarrier result;
      derez.deserialize(result);
      RtUserEvent done_event;
      derez.deserialize(done_event);

      physical_template->record_trace_shard_frontier(frontier, result);
      Runtime::trigger_event(done_event);
    }

    //--------------------------------------------------------------------------
    void ShardManager::send_trace_update(ShardID target, Serializer &rez)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(target < address_spaces->size());
#endif
      AddressSpaceID target_space = (*address_spaces)[target];
      // Check to see if this is a local shard
      if (target_space == runtime->address_space)
      {
        Deserializer derez(rez.get_buffer(), rez.get_used_bytes());
        // Have to unpack the preample we already know
        ReplicationID local_repl;
        derez.deserialize(local_repl);     
        handle_trace_update(derez, target_space);
      }
      else
        runtime->send_control_replicate_trace_update(target_space, rez);
    }

    //--------------------------------------------------------------------------
    void ShardManager::handle_trace_update(Deserializer &derez,
                                           AddressSpaceID source)
    //--------------------------------------------------------------------------
    {
      // Figure out which shard we are going to
      ShardID target;
      derez.deserialize(target);
      for (std::vector<ShardTask*>::const_iterator it = 
            local_shards.begin(); it != local_shards.end(); it++)
      {
        if ((*it)->shard_id == target)
        {
          (*it)->handle_trace_update(derez, source);
          return;
        }
      }
      // Should never get here
      assert(false);
    }

    //--------------------------------------------------------------------------
    ShardID ShardManager::find_collective_owner(RegionTreeID tid) const
    //--------------------------------------------------------------------------
    {
      // This is the node that made the logical region tree
      const AddressSpaceID tree_owner = 
        RegionTreeNode::get_owner_space(tid, runtime);
      // This is the node in the replicate context that will handle
      // all the collective instance creations for this region tree
      const AddressSpaceID target_owner =
        collective_mapping->contains(tree_owner) ? tree_owner :
        collective_mapping->find_nearest(tree_owner);
      // We'll just assign all view creation to the first shard on this node
      const ShardMapping &mapping = get_mapping();
      for (ShardID shard = 0; shard < mapping.size(); shard++)
        if (mapping[shard] == target_owner)
          return shard;
      assert(false);
      return 0;
    }

    //--------------------------------------------------------------------------
    void ShardManager::send_find_or_create_collective_view(ShardID target,
                                                           Serializer &rez)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(target < address_spaces->size());
#endif
      AddressSpaceID target_space = (*address_spaces)[target];
      // Check to see if this is a local shard
      if (target_space == runtime->address_space)
      {
        Deserializer derez(rez.get_buffer(), rez.get_used_bytes());
        // Have to unpack the preample we already know
        ReplicationID local_repl;
        derez.deserialize(local_repl);     
        handle_find_or_create_collective_view(derez);
      }
      else
        runtime->send_control_replicate_find_collective_view(target_space, rez);
    }

    //--------------------------------------------------------------------------
    void ShardManager::handle_find_or_create_collective_view(
                                                            Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      ShardID target;
      derez.deserialize(target);
      for (std::vector<ShardTask*>::const_iterator it = 
            local_shards.begin(); it != local_shards.end(); it++)
      {
        if ((*it)->shard_id == target)
        {
          InnerContext *context = (*it)->get_shard_execution_context();
          RegionTreeID tid;
          derez.deserialize(tid);
          size_t num_insts;
          derez.deserialize(num_insts);
          std::vector<DistributedID> instances(num_insts);
          for (unsigned idx = 0; idx < num_insts; idx++)
            derez.deserialize(instances[idx]);
          InnerContext::CollectiveResult *target;
          derez.deserialize(target);
          AddressSpaceID source;
          derez.deserialize(source);
          RtUserEvent to_trigger;
          derez.deserialize(to_trigger);
          RtEvent ready;
          InnerContext::CollectiveResult *result =
            context->find_or_create_collective_view(tid, instances, ready);
          if (ready.exists() && !ready.has_triggered())
            ready.wait();
          Serializer rez;
          {
            RezCheck z(rez);
            rez.serialize(target);
            rez.serialize(result->collective_did);
            rez.serialize(result->ready_event);
            rez.serialize(to_trigger);
          }
          runtime->send_remote_context_find_collective_view_response(source,
                                                                     rez);
          if (result->remove_reference())
            delete result;
          return;
        }
      }
      // Should never get here
      assert(false);  
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_find_collective_view(
                                          Deserializer &derez, Runtime *runtime)
    //--------------------------------------------------------------------------
    {
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ShardManager *manager = runtime->find_shard_manager(repl_id);
      manager->handle_find_or_create_collective_view(derez);
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_launch(const void *args)
    //--------------------------------------------------------------------------
    {
      const ShardManagerLaunchArgs *largs = (const ShardManagerLaunchArgs*)args;
      largs->shard->launch_shard();
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_delete(const void *args)
    //--------------------------------------------------------------------------
    {
      const ShardManagerDeleteArgs *dargs = (const ShardManagerDeleteArgs*)args;
      if (dargs->manager->remove_reference())
        delete dargs->manager;
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_launch(Deserializer &derez, 
                                        Runtime *runtime, AddressSpaceID source)
    //--------------------------------------------------------------------------
    {
      DerezCheck z(derez);
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      Domain shard_domain;
      derez.deserialize(shard_domain);
      size_t total_shards;
      derez.deserialize(total_shards);
      std::vector<DomainPoint> shard_points(total_shards);
      std::vector<DomainPoint> sorted_points(total_shards);
      std::vector<ShardID> shard_lookup(total_shards);
      bool isomorphic_points;
      derez.deserialize(isomorphic_points);
      if (isomorphic_points)
      {
        for (unsigned idx = 0; idx < total_shards; idx++)
        {
          derez.deserialize(shard_points[idx]);
          sorted_points[idx] = shard_points[idx];
          shard_lookup[idx] = idx;
        }
      }
      else
      {
        for (unsigned idx = 0; idx < total_shards; idx++)
        {
          derez.deserialize(sorted_points[idx]);
          derez.deserialize(shard_lookup[idx]);
          shard_points[shard_lookup[idx]] = sorted_points[idx];
        }
      }
      bool control_repl;
      derez.deserialize(control_repl);
      bool top_level_task;
      derez.deserialize(top_level_task);
      RtBarrier shard_task_barrier;
      derez.deserialize(shard_task_barrier);
      ShardManager *manager = 
        new ShardManager(runtime, repl_id, control_repl, top_level_task,
                isomorphic_points, shard_domain, std::move(shard_points),
                std::move(sorted_points), std::move(shard_lookup), 
                source, NULL/*original*/, shard_task_barrier);
      manager->unpack_shards_and_launch(derez);
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_delete(
                                          Deserializer &derez, Runtime *runtime)
    //--------------------------------------------------------------------------
    {
      DerezCheck z(derez);
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ShardManager *manager = runtime->find_shard_manager(repl_id);
      delete manager;
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_post_mapped(
                                          Deserializer &derez, Runtime *runtime)
    //--------------------------------------------------------------------------
    {
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      RtEvent precondition;
      derez.deserialize(precondition);
      ShardManager *manager = runtime->find_shard_manager(repl_id);
      manager->handle_post_mapped(false/*local*/, precondition);
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_post_execution(
                                          Deserializer &derez, Runtime *runtime)
    //--------------------------------------------------------------------------
    {
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ShardManager *manager = runtime->find_shard_manager(repl_id);
      FutureInstance *instance = FutureInstance::unpack_instance(derez,runtime);
      size_t metasize;
      derez.deserialize(metasize);
      void *metadata = NULL;
      if (metasize > 0)
      {
        metadata = malloc(metasize);
        memcpy(metadata, derez.get_current_pointer(), metasize);
        derez.advance_pointer(metasize);
      }
      manager->handle_post_execution(instance,metadata,metasize,false/*local*/);
    }

    /*static*/ void ShardManager::handle_trigger_complete(
                                          Deserializer &derez, Runtime *runtime)
    //--------------------------------------------------------------------------
    {
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ApEvent all_shards_done;
      derez.deserialize(all_shards_done);
      RtUserEvent done_event;
      derez.deserialize(done_event);
      ShardManager *manager = runtime->find_shard_manager(repl_id);
      Runtime::trigger_event(done_event,
          manager->trigger_task_complete(false/*local*/, all_shards_done));
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_trigger_commit(
                                          Deserializer &derez, Runtime *runtime)
    //--------------------------------------------------------------------------
    {
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ShardManager *manager = runtime->find_shard_manager(repl_id);
      manager->trigger_task_commit(false/*local*/);
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_collective_message(Deserializer &derez,
                                                            Runtime *runtime)
    //--------------------------------------------------------------------------
    {
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ShardManager *manager = runtime->find_shard_manager(repl_id);
      manager->handle_collective_message(derez);
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_future_map_request(Deserializer &derez,
                                                            Runtime *runtime)
    //--------------------------------------------------------------------------
    {
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ShardManager *manager = runtime->find_shard_manager(repl_id);
      manager->handle_future_map_request(derez);
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_trace_update(Deserializer &derez,
                                                      Runtime *runtime,
                                                      AddressSpaceID source)
    //--------------------------------------------------------------------------
    {
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ShardManager *manager = runtime->find_shard_manager(repl_id);
      manager->handle_trace_update(derez, source);
    }

#ifdef NO_EXPLICIT_COLLECTIVES
    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_collective_instance_message(
                                          Deserializer &derez, Runtime *runtime)
    //--------------------------------------------------------------------------
    {
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ShardManager *manager = runtime->find_shard_manager(repl_id);
      manager->handle_collective_instance_message(derez);
    }
#endif

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_disjoint_complete_request(
                                          Deserializer &derez, Runtime *runtime)
    //--------------------------------------------------------------------------
    {
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ShardManager *manager = runtime->find_shard_manager(repl_id);
      manager->handle_disjoint_complete_request(derez);
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_intra_space_dependence(
                                          Deserializer &derez, Runtime *runtime)
    //--------------------------------------------------------------------------
    {
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ShardManager *manager = runtime->find_shard_manager(repl_id);
      manager->handle_intra_space_dependence(derez);
    }

    //--------------------------------------------------------------------------
    /*static*/ void ShardManager::handle_broadcast_update(Deserializer &derez,
                                                          Runtime *runtime)
    //--------------------------------------------------------------------------
    {
      ReplicationID repl_id;
      derez.deserialize(repl_id);
      ShardManager *manager = runtime->find_shard_manager(repl_id);
      manager->handle_broadcast(derez);
    }

    //--------------------------------------------------------------------------
    ShardingFunction* ShardManager::find_sharding_function(ShardingID sid, 
                                                           bool skip_checks)
    //--------------------------------------------------------------------------
    {
      // Check to see if it is in the cache
      {
        AutoLock m_lock(manager_lock,1,false/*exclusive*/);
        std::map<ShardingID,ShardingFunction*>::const_iterator finder = 
          sharding_functions.find(sid);
        if (finder != sharding_functions.end())
          return finder->second;
      }
      // Get the functor from the runtime
      ShardingFunctor *functor = runtime->find_sharding_functor(sid);
      // Retake the lock
      AutoLock m_lock(manager_lock);
      // See if we lost the race
      std::map<ShardingID,ShardingFunction*>::const_iterator finder = 
        sharding_functions.find(sid);
      if (finder != sharding_functions.end())
        return finder->second;
      ShardingFunction *result = 
        new ShardingFunction(functor, runtime->forest, this, sid, skip_checks);
      // Save the result for the future
      sharding_functions[sid] = result;
      return result;
    }

#ifdef LEGION_USE_LIBDL
    //--------------------------------------------------------------------------
    void ShardManager::perform_global_registration_callbacks(
                     Realm::DSOReferenceImplementation *dso, const void *buffer,
                     size_t buffer_size, bool withargs, size_t dedup_tag,
                     RtEvent local_done, RtEvent global_done,
                     std::set<RtEvent> &preconditions)
    //--------------------------------------------------------------------------
    {
      // See if we're the first one to handle this DSO
      const Runtime::RegistrationKey key(dedup_tag, 
                                         dso->dso_name, dso->symbol_name);
      {
        AutoLock m_lock(manager_lock);
        // Check to see if we've already handled this
        std::set<Runtime::RegistrationKey>::const_iterator finder =
          unique_registration_callbacks.find(key);
        if (finder != unique_registration_callbacks.end())
          return;
        unique_registration_callbacks.insert(key);
        if (unique_shard_spaces.empty())
          for (unsigned shard = 0; shard < total_shards; shard++)
                unique_shard_spaces.insert((*address_spaces)[shard]);
      }
      // We're the first one so handle it
      if (!is_total_sharding())
      {
        std::set<RtEvent> local_preconditions;
        AddressSpaceID space = 0;
        for (std::set<AddressSpaceID>::const_iterator it = 
              unique_shard_spaces.begin(); it != 
              unique_shard_spaces.end(); it++, space++)
        {
          if ((*it) == runtime->address_space)
            break;
        }
#ifdef DEBUG_LEGION
        assert(space < unique_shard_spaces.size());
#endif
        for ( ; space < runtime->total_address_spaces; 
              space += unique_shard_spaces.size())
        {
          if (unique_shard_spaces.find(space) != unique_shard_spaces.end())
            continue;
          runtime->send_registration_callback(space, dso, global_done,
              local_preconditions, buffer, buffer_size, withargs, 
              true/*deduplicate*/, dedup_tag);
        }
        if (!local_preconditions.empty())
        {
          local_preconditions.insert(local_done);
          Runtime::phase_barrier_arrive(callback_barrier, 1/*count*/,
                          Runtime::merge_events(local_preconditions));
        }
        else
          Runtime::phase_barrier_arrive(callback_barrier,
                                        1/*count*/, local_done);
      }
      else // there will be a callback on every node anyway
        Runtime::phase_barrier_arrive(callback_barrier,1/*count*/,local_done);
      preconditions.insert(callback_barrier);
      Runtime::advance_barrier(callback_barrier);
      if (!callback_barrier.exists())
        REPORT_LEGION_FATAL(LEGION_FATAL_UNIMPLEMENTED_FEATURE,
            "Need support for refreshing exhausted callback phase "
            "barrier generations.")
    }
#endif // LEGION_USE_LIBDL

    //--------------------------------------------------------------------------
    bool ShardManager::perform_semantic_attach(void)
    //--------------------------------------------------------------------------
    {
      if (local_shards.size() == 1)
        return true;
      AutoLock m_lock(manager_lock);
#ifdef DEBUG_LEGION
      assert(semantic_attach_counter < local_shards.size());
#endif
      if (++semantic_attach_counter == local_shards.size())
      {
        semantic_attach_counter = 0;
        return true;
      }
      else
        return false;
    }

    /////////////////////////////////////////////////////////////
    // Shard Collective 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ShardCollective::ShardCollective(CollectiveIndexLocation loc,
                                     ReplicateContext *ctx)
      : manager(ctx->shard_manager), context(ctx), 
        local_shard(ctx->owner_shard->shard_id), 
        collective_index(ctx->get_next_collective_index(loc))
    //--------------------------------------------------------------------------
    {
      context->add_reference();
    }

    //--------------------------------------------------------------------------
    ShardCollective::ShardCollective(ReplicateContext *ctx, CollectiveID id)
      : manager(ctx->shard_manager), context(ctx), 
        local_shard(ctx->owner_shard->shard_id), collective_index(id)
    //--------------------------------------------------------------------------
    { 
      context->add_reference();
    }

    //--------------------------------------------------------------------------
    ShardCollective::~ShardCollective(void)
    //--------------------------------------------------------------------------
    {
      // Unregister this with the context 
      context->unregister_collective(this);
      if (context->remove_reference())
        delete context;
    }

    //--------------------------------------------------------------------------
    void ShardCollective::perform_collective_sync(RtEvent precondition)
    //--------------------------------------------------------------------------
    {
      perform_collective_async(precondition); 
      perform_collective_wait(true/*block*/);
    }

    //--------------------------------------------------------------------------
    /*static*/void ShardCollective::handle_deferred_collective(const void *args)
    //--------------------------------------------------------------------------
    {
      const DeferCollectiveArgs *dargs = (const DeferCollectiveArgs*)args;
      dargs->collective->perform_collective_async();
    }

    //--------------------------------------------------------------------------
    bool ShardCollective::defer_collective_async(RtEvent precondition)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(precondition.exists());
#endif
      DeferCollectiveArgs args(this);
      if (precondition.has_triggered())
        return false;
      context->runtime->issue_runtime_meta_task(args,
          LG_LATENCY_DEFERRED_PRIORITY, precondition);
      return true;
    }

    //--------------------------------------------------------------------------
    int ShardCollective::convert_to_index(ShardID id, ShardID origin) const
    //--------------------------------------------------------------------------
    {
      // shift everything so that the target shard is at index 0
      const int result = 
        ((id + (manager->total_shards - origin)) % manager->total_shards);
      return result;
    }

    //--------------------------------------------------------------------------
    ShardID ShardCollective::convert_to_shard(int index, ShardID origin) const
    //--------------------------------------------------------------------------
    {
      // Add target then take the modulus
      const ShardID result = (index + origin) % manager->total_shards; 
      return result;
    }

    /////////////////////////////////////////////////////////////
    // Gather Collective 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    BroadcastCollective::BroadcastCollective(CollectiveIndexLocation loc,
                                             ReplicateContext *ctx, ShardID o)
      : ShardCollective(loc, ctx), origin(o),
        shard_collective_radix(ctx->get_shard_collective_radix())
    //--------------------------------------------------------------------------
    {
      if (local_shard != origin)
        done_event = Runtime::create_rt_user_event();
    }

    //--------------------------------------------------------------------------
    BroadcastCollective::BroadcastCollective(ReplicateContext *ctx, 
                                             CollectiveID id, ShardID o)
      : ShardCollective(ctx, id), origin(o),
        shard_collective_radix(ctx->get_shard_collective_radix())
    //--------------------------------------------------------------------------
    {
      if (local_shard != origin)
        done_event = Runtime::create_rt_user_event();
    }

    //--------------------------------------------------------------------------
    BroadcastCollective::~BroadcastCollective(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    void BroadcastCollective::perform_collective_async(RtEvent precondition)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(local_shard == origin);
#endif
      if (precondition.exists() && defer_collective_async(precondition))
        return;
      // Register this with the context
      context->register_collective(this);
      send_messages(); 
    }

    //--------------------------------------------------------------------------
    RtEvent BroadcastCollective::perform_collective_wait(bool block/*=true*/)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(local_shard != origin);
#endif     
      // Register this with the context
      context->register_collective(this);
      if (!done_event.has_triggered())
      {
        if (block)
          done_event.wait();
        else
          return done_event;
      }
      return RtEvent::NO_RT_EVENT;
    }

    //--------------------------------------------------------------------------
    void BroadcastCollective::handle_collective_message(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(local_shard != origin);
#endif
      // No need for the lock since this is only written to once
      unpack_collective(derez);
      // Send our messages
      send_messages();
      // Then trigger our event to indicate that we are ready
      Runtime::trigger_event(done_event, post_broadcast());
    }

    //--------------------------------------------------------------------------
    RtEvent BroadcastCollective::get_done_event(void) const
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(local_shard != origin);
#endif
      return done_event;
    }

    //--------------------------------------------------------------------------
    void BroadcastCollective::send_messages(void) const
    //--------------------------------------------------------------------------
    {
      const int local_index = convert_to_index(local_shard, origin);
      for (int idx = 1; idx <= shard_collective_radix; idx++)
      {
        const int target_index = local_index * shard_collective_radix + idx; 
        if (target_index >= int(manager->total_shards))
          break;
        ShardID target = convert_to_shard(target_index, origin);
        Serializer rez;
        {
          rez.serialize(manager->repl_id);
          rez.serialize(target);
          rez.serialize(collective_index);
          pack_collective(rez);
        }
        manager->send_collective_message(target, rez);
      }
    }

    /////////////////////////////////////////////////////////////
    // Gather Collective 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    GatherCollective::GatherCollective(CollectiveIndexLocation loc,
                                       ReplicateContext *ctx, ShardID t)
      : ShardCollective(loc, ctx), target(t), 
        shard_collective_radix(ctx->get_shard_collective_radix()),
        expected_notifications(compute_expected_notifications()),
        received_notifications(0)
    //--------------------------------------------------------------------------
    {
      if (expected_notifications > 1)
        done_event = Runtime::create_rt_user_event();
    }

    //--------------------------------------------------------------------------
    GatherCollective::GatherCollective(ReplicateContext *ctx, 
                                       CollectiveID id, ShardID t)
      : ShardCollective(ctx, id), target(t), 
        shard_collective_radix(ctx->get_shard_collective_radix()),
        expected_notifications(compute_expected_notifications()),
        received_notifications(0)
    //--------------------------------------------------------------------------
    {
      if (expected_notifications > 1)
        done_event = Runtime::create_rt_user_event();
    }

    //--------------------------------------------------------------------------
    GatherCollective::~GatherCollective(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      if (done_event.exists())
        assert(done_event.has_triggered());
#endif
    }

    //--------------------------------------------------------------------------
    void GatherCollective::perform_collective_async(RtEvent precondition)
    //--------------------------------------------------------------------------
    {
      if (precondition.exists() && defer_collective_async(precondition))
        return;
      // Register this with the context
      context->register_collective(this);
      bool done = false;
      {
        AutoLock c_lock(collective_lock);
#ifdef DEBUG_LEGION
        assert(received_notifications < expected_notifications);
#endif
        done = (++received_notifications == expected_notifications);
      }
      if (done)
      {
        if (local_shard != target)
          send_message();
        RtEvent postcondition = post_gather();
        if (done_event.exists())
          Runtime::trigger_event(done_event, postcondition);
      }
    }

    //--------------------------------------------------------------------------
    RtEvent GatherCollective::perform_collective_wait(bool block/*=true*/)
    //--------------------------------------------------------------------------
    {
      if (done_event.exists() && !done_event.has_triggered())
      {
        if (block)
          done_event.wait();
        else
          return done_event;
      }
      return RtEvent::NO_RT_EVENT;
    }

    //--------------------------------------------------------------------------
    void GatherCollective::handle_collective_message(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      bool done = false;
      {
        // Hold the lock while doing these operations
        AutoLock c_lock(collective_lock);
        // Unpack the result
        unpack_collective(derez);
 #ifdef DEBUG_LEGION
        assert(received_notifications < expected_notifications);
#endif
        done = (++received_notifications == expected_notifications);       
      }
      if (done)
      {
        if (local_shard != target)
          send_message();
        RtEvent postcondition = post_gather();
        if (done_event.exists())
          Runtime::trigger_event(done_event, postcondition);
      }
    }

    //--------------------------------------------------------------------------
    void GatherCollective::elide_collective(void)
    //--------------------------------------------------------------------------
    {
      if (done_event.exists())
        Runtime::trigger_event(done_event);
    }

    //--------------------------------------------------------------------------
    void GatherCollective::send_message(void)
    //--------------------------------------------------------------------------
    {
      // Convert to our local index
      const int local_index = convert_to_index(local_shard, target);
#ifdef DEBUG_LEGION
      assert(local_index > 0); // should never be here for zero
#endif
      // Subtract by 1 and then divide to get the target (truncate)
      const int target_index = (local_index - 1) / shard_collective_radix;
      // Then convert back to the target
      ShardID next = convert_to_shard(target_index, target);
      Serializer rez;
      {
        rez.serialize(manager->repl_id);
        rez.serialize(next);
        rez.serialize(collective_index);
        AutoLock c_lock(collective_lock,1,false/*exclusive*/);
        pack_collective(rez);
      }
      manager->send_collective_message(next, rez);
    } 

    //--------------------------------------------------------------------------
    int GatherCollective::compute_expected_notifications(void) const
    //--------------------------------------------------------------------------
    {
      int result = 1; // always have one arriver for ourself
      const int index = convert_to_index(local_shard, target);
      for (int idx = 1; idx <= shard_collective_radix; idx++)
      {
        const int source_index = index * shard_collective_radix + idx;
        if (source_index >= int(manager->total_shards))
          break;
        result++;
      }
      return result;
    }

    /////////////////////////////////////////////////////////////
    // All Gather Collective 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    template<bool INORDER>
    AllGatherCollective<INORDER>::AllGatherCollective(
        CollectiveIndexLocation loc, ReplicateContext *ctx)
      : ShardCollective(loc, ctx),
        shard_collective_radix(ctx->get_shard_collective_radix()),
        shard_collective_log_radix(ctx->get_shard_collective_log_radix()),
        shard_collective_stages(ctx->get_shard_collective_stages()),
        shard_collective_participating_shards(
            ctx->get_shard_collective_participating_shards()),
        shard_collective_last_radix(ctx->get_shard_collective_last_radix()),
        participating(int(local_shard) < shard_collective_participating_shards),
        reorder_stages(NULL), pending_send_ready_stages(0)
#ifdef DEBUG_LEGION
        , done_triggered(false)
#endif
    //--------------------------------------------------------------------------
    { 
      initialize_collective(); 
    }

    //--------------------------------------------------------------------------
    template<bool INORDER>
    AllGatherCollective<INORDER>::AllGatherCollective(ReplicateContext *ctx,
                                                      CollectiveID id)
      : ShardCollective(ctx, id),
        shard_collective_radix(ctx->get_shard_collective_radix()),
        shard_collective_log_radix(ctx->get_shard_collective_log_radix()),
        shard_collective_stages(ctx->get_shard_collective_stages()),
        shard_collective_participating_shards(
            ctx->get_shard_collective_participating_shards()),
        shard_collective_last_radix(ctx->get_shard_collective_last_radix()),
        participating(int(local_shard) < shard_collective_participating_shards),
        reorder_stages(NULL), pending_send_ready_stages(0)
#ifdef DEBUG_LEGION
        , done_triggered(false)
#endif
    //--------------------------------------------------------------------------
    {
      initialize_collective();
    }

    //--------------------------------------------------------------------------
    template<bool INORDER>
    void AllGatherCollective<INORDER>::initialize_collective(void)
    //--------------------------------------------------------------------------
    {
      if (manager->total_shards > 1)
      {
        // We already have our contributions for each stage so
        // we can set the inditial participants to 1
        if (participating)
        {
#ifdef DEBUG_LEGION
          assert(shard_collective_stages > 0);
#endif
          sent_stages.resize(shard_collective_stages, false);
          stage_notifications.resize(shard_collective_stages, 1);
          // Stage 0 always starts with 0 notifications since we'll 
          // explictcly arrive on it
          stage_notifications[0] = 0;
        }
        done_event = Runtime::create_rt_user_event();
      }
    }

    //--------------------------------------------------------------------------
    template<bool INORDER>
    AllGatherCollective<INORDER>::~AllGatherCollective(void)
    //--------------------------------------------------------------------------
    {
      if (reorder_stages != NULL)
      {
#ifdef DEBUG_LEGION
        assert(reorder_stages->empty());
#endif
        delete reorder_stages;
      }
#ifdef DEBUG_LEGION
      if (participating)
      {
        // We should have sent all our stages before being deleted
        for (unsigned idx = 0; idx < sent_stages.size(); idx++)
          assert(sent_stages[idx]);
      }
      if (participating)
        assert(done_triggered);
      assert(done_event.has_triggered());
#endif
    } 

    //--------------------------------------------------------------------------
    template<bool INORDER>
    void AllGatherCollective<INORDER>::perform_collective_async(RtEvent pre)
    //--------------------------------------------------------------------------
    {
      if (pre.exists() && defer_collective_async(pre))
        return;
      // Register this with the context
      context->register_collective(this);
      if (manager->total_shards <= 1)
        return;
      // See if we are a participating shard or not
      if (participating)
      {
        // We are a participating shard 
        // See if we are waiting for an initial notification
        // if not we can just send our message now
        if ((int(manager->total_shards) == 
              shard_collective_participating_shards) || 
            (local_shard >= (manager->total_shards -
              shard_collective_participating_shards)))
        {
          const bool all_stages_done = initiate_collective();
          if (all_stages_done)
            complete_exchange();
        }
      }
      else
      {
        // We are not a participating shard
        // so we just have to send notification to one shard
        send_remainder_stage();
      }
    }

    //--------------------------------------------------------------------------
    template<bool INORDER>
    RtEvent AllGatherCollective<INORDER>::perform_collective_wait(
                                                            bool block/*=true*/)
    //--------------------------------------------------------------------------
    {
      if (manager->total_shards <= 1)
        return RtEvent::NO_RT_EVENT;
      if (!done_event.has_triggered())
      {
        if (block)
          done_event.wait();
        else
          return done_event;
      }
      return RtEvent::NO_RT_EVENT;
    }

    //--------------------------------------------------------------------------
    template<bool INORDER>
    void AllGatherCollective<INORDER>::handle_collective_message(
                                                            Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      int stage;
      derez.deserialize(stage);
#ifdef DEBUG_LEGION
      assert(participating || (stage == -1));
#endif
      unpack_stage(stage, derez);
      bool all_stages_done = false;
      if (stage == -1)
      {
        if (!participating)
          all_stages_done = true;
        else // we can now initiate the collective
          all_stages_done = initiate_collective(); 
      }
      else
        all_stages_done = send_ready_stages();
      if (all_stages_done)
        complete_exchange();
    } 

    //--------------------------------------------------------------------------
    template<bool INORDER>
    void AllGatherCollective<INORDER>::elide_collective(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      // make it look like we sent all the stages
      for (unsigned idx = 0; idx < sent_stages.size(); idx++)
        sent_stages[idx] = true;
      assert(!done_triggered);
      assert(!done_event.has_triggered());
#endif
      // Trigger the user event 
      Runtime::trigger_event(done_event);
#ifdef DEBUG_LEGION
      done_triggered = true;
#endif
    }

    //--------------------------------------------------------------------------
    template<bool INORDER>
    void AllGatherCollective<INORDER>::construct_message(ShardID target, 
                                                     int stage, Serializer &rez)
    //--------------------------------------------------------------------------
    {
      rez.serialize(manager->repl_id);
      rez.serialize(target);
      rez.serialize(collective_index);
      rez.serialize(stage);
      AutoLock c_lock(collective_lock, 1, false/*exclusive*/);
      pack_collective_stage(rez, stage);
    }

    //--------------------------------------------------------------------------
    template<bool INORDER>
    bool AllGatherCollective<INORDER>::initiate_collective(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(participating); // should only get this for participating shards
#endif
      {
        AutoLock c_lock(collective_lock);
#ifdef DEBUG_LEGION
        assert(!sent_stages.empty());
        assert(!sent_stages[0]); // stage 0 shouldn't be sent yet
        assert(!stage_notifications.empty());
        if (shard_collective_stages == 1)
          assert(stage_notifications[0] < shard_collective_last_radix); 
        else
          assert(stage_notifications[0] < shard_collective_radix);
#endif
        stage_notifications[0]++;
        // Increment our guard to prevent deletion of the collective
        // object while we are still traversing
        pending_send_ready_stages++;
      }
      return send_ready_stages(0/*start stage*/);
    }

    //--------------------------------------------------------------------------
    template<bool INORDER>
    void AllGatherCollective<INORDER>::send_remainder_stage(void)
    //--------------------------------------------------------------------------
    {
      if (participating)
      {
        // Send back to the shards that are not participating
        ShardID target = local_shard + shard_collective_participating_shards;
#ifdef DEBUG_LEGION
        assert(target < manager->total_shards);
#endif
        Serializer rez;
        construct_message(target, -1/*stage*/, rez);
        manager->send_collective_message(target, rez);
      }
      else
      {
        // Send to a node that is participating
        ShardID target = local_shard % shard_collective_participating_shards;
        Serializer rez;
        construct_message(target, -1/*stage*/, rez);
        manager->send_collective_message(target, rez);
      }
    }

    //--------------------------------------------------------------------------
    template<bool INORDER>
    bool AllGatherCollective<INORDER>::send_ready_stages(const int start_stage)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(participating);
#endif
      // Iterate through the stages and send any that are ready
      // Remember that stages have to be done in order
      bool sent_previous_stage = false;
      for (int stage = start_stage; stage < shard_collective_stages; stage++)
      {
        {
          AutoLock c_lock(collective_lock);
          if (sent_previous_stage)
          {
#ifdef DEBUG_LEGION
            assert(!sent_stages[stage-1]);
#endif
            sent_stages[stage-1] = true;
            sent_previous_stage = false;
          }
          // If this stage has already been sent then we can keep going
          if (sent_stages[stage])
            continue;
#ifdef DEBUG_LEGION
          assert(pending_send_ready_stages > 0);
#endif
          // Check to see if we're sending this stage
          // We need all the notifications from the previous stage before
          // we can send this stage
          if (stage > 0)
          {
            // We can't have multiple threads doing sends at the same time
            // so make sure that only the last one is going through doing work
            // but stage 0 is because it is always sent by the initiator so
            // don't check this until we're past the first stage
            if ((stage_notifications[stage-1] < shard_collective_radix) ||
                (pending_send_ready_stages > 1))
            {
              // Remove our guard before exiting early
              pending_send_ready_stages--;
              return false;
            }
            else if (INORDER && (reorder_stages != NULL))
            {
              // Check to see if we have any unhandled messages for 
              // the previous stage that we need to handle before sending
              std::map<int,std::vector<std::pair<void*,size_t> > >::iterator
                finder = reorder_stages->find(stage-1);
              if (finder != reorder_stages->end())
              {
                // Perform the handling for the buffered messages now
                for (std::vector<std::pair<void*,size_t> >::const_iterator it =
                      finder->second.begin(); it != finder->second.end(); it++)
                {
                  Deserializer derez(it->first, it->second);
                  unpack_collective_stage(derez, finder->first);
                  free(it->first);
                }
                reorder_stages->erase(finder);
              }
            }
          }
          // If we get here then we can send the stage
        }
        // Now we can do the send
        if (stage == (shard_collective_stages-1))
        {
          for (int r = 1; r < shard_collective_last_radix; r++)
          {
            const ShardID target = local_shard ^
              (r << (stage * shard_collective_log_radix));
#ifdef DEBUG_LEGION
            assert(int(target) < shard_collective_participating_shards);
#endif
            Serializer rez;
            construct_message(target, stage, rez);
            manager->send_collective_message(target, rez);
          }
        }
        else
        {
          for (int r = 1; r < shard_collective_radix; r++)
          {
            const ShardID target = local_shard ^
              (r << (stage * shard_collective_log_radix));
#ifdef DEBUG_LEGION
            assert(int(target) < shard_collective_participating_shards);
#endif
            Serializer rez;
            construct_message(target, stage, rez);
            manager->send_collective_message(target, rez);
          }
        }
        sent_previous_stage = true;
      }
      // If we make it here, then we sent the last stage, check to see
      // if we've seen all the notifications for it
      AutoLock c_lock(collective_lock);
      if (sent_previous_stage)
      {
#ifdef DEBUG_LEGION
        assert(!sent_stages[shard_collective_stages-1]);
#endif
        sent_stages[shard_collective_stages-1] = true;
      }
      // Remove our pending guard and then check to see if we are done
#ifdef DEBUG_LEGION
      assert(pending_send_ready_stages > 0);
#endif
      if (((--pending_send_ready_stages) == 0) &&
          (stage_notifications.back() == shard_collective_last_radix))
      {
#ifdef DEBUG_LEGION
        assert(!done_triggered);
        done_triggered = true;
#endif
        return true;
      }
      else
        return false;
    }

    //--------------------------------------------------------------------------
    template<bool INORDER>
    void AllGatherCollective<INORDER>::unpack_stage(int stage, 
                                                    Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      AutoLock c_lock(collective_lock);
      // Do the unpack first while holding the lock
      if (INORDER && (stage >= 0))
      {
        // Check to see if we can handle this message now or whether we
        // need to buffer it for the future because we have not finished
        // sending the current stage yet or not
        if (!sent_stages[stage])
        {
          // Buffer this message until the stage is sent as well 
          const size_t buffer_size = derez.get_remaining_bytes();
          void *buffer = malloc(buffer_size);
          memcpy(buffer, derez.get_current_pointer(), buffer_size);
          derez.advance_pointer(buffer_size);
          if (reorder_stages == NULL)
            reorder_stages = 
              new std::map<int,std::vector<std::pair<void*,size_t> > >();
          (*reorder_stages)[stage].push_back(
              std::pair<void*,size_t>(buffer, buffer_size));
        }
        else
          unpack_collective_stage(derez, stage);
      }
      else // Just do the unpack here immediately
        unpack_collective_stage(derez, stage);
      if (stage >= 0)
      {
#ifdef DEBUG_LEGION
	assert(stage < int(stage_notifications.size()));
        if (stage < (shard_collective_stages-1))
          assert(stage_notifications[stage] < shard_collective_radix);
        else
          assert(stage_notifications[stage] < shard_collective_last_radix);
#endif
        stage_notifications[stage]++;
        // Increment our guard to prevent deletion of the collective
        // object while we are still traversing
        pending_send_ready_stages++;
      }
    }

    //--------------------------------------------------------------------------
    template<bool INORDER>
    void AllGatherCollective<INORDER>::complete_exchange(void)
    //--------------------------------------------------------------------------
    {
      if ((reorder_stages != NULL) && !reorder_stages->empty())
      {
#ifdef DEBUG_LEGION
        assert(reorder_stages->size() == 1);
#endif
        std::map<int,std::vector<std::pair<void*,size_t> > >::iterator 
          remaining = reorder_stages->begin();
        for (std::vector<std::pair<void*,size_t> >::const_iterator it = 
              remaining->second.begin(); it != remaining->second.end(); it++)
        {
          Deserializer derez(it->first, it->second);
          unpack_collective_stage(derez, remaining->first);
          free(it->first);     
        }
        reorder_stages->erase(remaining);
      }
      // See if we have to send a message back to a non-participating shard 
      if ((int(manager->total_shards) > shard_collective_participating_shards)
          && (int(local_shard) < int(manager->total_shards -
                                     shard_collective_participating_shards)))
        send_remainder_stage();
      // Pull this onto the stack in case post_complete_exchange ends up
      // deleting the object
      const RtUserEvent to_trigger = done_event;
      const RtEvent precondition = post_complete_exchange();
      // Only after we send the message and do the post can we signal we're done
      Runtime::trigger_event(to_trigger, precondition);
    }

    template class AllGatherCollective<false>;

    /////////////////////////////////////////////////////////////
    // Future All Reduce Collective 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    FutureAllReduceCollective::FutureAllReduceCollective(Operation *o,
                         CollectiveIndexLocation loc, ReplicateContext *ctx, 
                         ReductionOpID id, const ReductionOp *op, bool determin)
      : AllGatherCollective(loc, ctx), op(o), redop(op), redop_id(id),
        deterministic(determin), finished(Runtime::create_ap_user_event(NULL)),
        instance(NULL), shadow_instance(NULL), last_stage_sends(0),
        current_stage(-1), pack_shadow(false)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    FutureAllReduceCollective::FutureAllReduceCollective(Operation *o,
        ReplicateContext *ctx, CollectiveID rid, ReductionOpID id,
        const ReductionOp* op, bool determin)
      : AllGatherCollective(ctx, id), op(o), redop(op), redop_id(rid),
        deterministic(determin), finished(Runtime::create_ap_user_event(NULL)),
        instance(NULL), shadow_instance(NULL), last_stage_sends(0),
        current_stage(-1), pack_shadow(false)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    FutureAllReduceCollective::~FutureAllReduceCollective(void)
    //--------------------------------------------------------------------------
    {
      if (shadow_instance != NULL)
      {
        ApEvent free_shadow;
        if (!shadow_postconditions.empty())
          free_shadow = Runtime::merge_events(NULL, shadow_postconditions);
        if (shadow_instance->deferred_delete(op, free_shadow))
          delete shadow_instance;
      }
    }

    //--------------------------------------------------------------------------
    void FutureAllReduceCollective::pack_collective_stage(
                                                     Serializer &rez, int stage)
    //--------------------------------------------------------------------------
    {
      // The first time we pack a stage we merge any values that we had
      // unpacked earlier as they are needed for sending this stage for
      // the first time.
      if (stage != current_stage)
      {
        bool check_for_shadow = true;
        if (!pending_reductions.empty())
        {
          std::map<int,std::map<ShardID,PendingReduce> >::iterator next =
            pending_reductions.begin();
          if (next->first == current_stage)
          {
            // Apply all of these to the destination instance
            ApEvent new_instance_ready = perform_reductions(next->second);
            // Check to see if we'll be able to pack up instance by value
            if (new_instance_ready.exists() || !instance->can_pack_by_value())
            {
              if (stage == -1)
              {
#ifdef DEBUG_LEGION
                assert(current_stage == (shard_collective_stages-1));
#endif
                instance_ready = new_instance_ready;
                // No need for packing the shadow on the way out
                pack_shadow = false;
              }
              else
              {
                // Have to copy this to the shadow instance because we can't
                // do this in-place without support from Realm
                if (shadow_instance == NULL)
                  create_shadow_instance();
                // Copy to the shadow instance, make sure to incorporate 
                // any of the shadow postconditions from the previous stage
                // so we know it's safe to write here
                if (!shadow_postconditions.empty())
                {
                  if (new_instance_ready.exists())
                    shadow_postconditions.insert(new_instance_ready);
                  shadow_ready = shadow_instance->copy_from(instance, op,
                      Runtime::merge_events(NULL, shadow_postconditions),
                      false/*check source ready*/);
                  shadow_postconditions.clear();
                }
                else
                  shadow_ready =
                    shadow_instance->copy_from(instance, op,
                        new_instance_ready, false/*check source ready*/);
                instance_ready = shadow_ready;
                pack_shadow = true;
              }
            }
            else
            {
              instance_ready = new_instance_ready;
              pack_shadow = false;
            }
            pending_reductions.erase(next);
            // No need for the check
            check_for_shadow = false;
          }
        }
        if (check_for_shadow)
        {
#ifdef DEBUG_LEGION
          // should be stage 0 (first stage) or final stage 0
          assert((stage == 0) || (stage == -1));
#endif
          if (stage == -1)
          {
#ifdef DEBUG_LEGION
            assert(current_stage == (shard_collective_stages-1));
#endif
            // No need for packing the shadow on the way out
            pack_shadow = false;
          }
          else if (instance_ready.exists() || !instance->can_pack_by_value())
          {
#ifdef DEBUG_LEGION
            assert(current_stage == -1);
#endif
            // Have to make a copy in this case
            if (shadow_instance == NULL)
              create_shadow_instance();
            shadow_ready = shadow_instance->copy_from(instance, op,
                          instance_ready, false/*check src ready*/);
            instance_ready = shadow_ready;
            pack_shadow = true;
          }
        }
        current_stage = stage;
      }
      rez.serialize(local_shard);
      if (pack_shadow)
      {
        if (!shadow_instance->pack_instance(rez, false/*pack ownership*/,
                                       true/*other ready*/, shadow_ready))
        {
          ApUserEvent applied = Runtime::create_ap_user_event(NULL);
          rez.serialize(applied);
          shadow_postconditions.insert(applied);
        }
        else
          rez.serialize(ApUserEvent::NO_AP_USER_EVENT);
      }
      else
      {
        if (!instance->pack_instance(rez, false/*pack owner*/, 
                                     true/*other ready*/, instance_ready))
        {
#ifdef DEBUG_LEGION
          assert(stage == -1);
#endif
          ApUserEvent copy_out = Runtime::create_ap_user_event(NULL);
          rez.serialize(copy_out);
          instance_ready = copy_out; 
        }
        else
          rez.serialize(ApUserEvent::NO_AP_USER_EVENT);
      }
      // See if this is the last stage, if so we need to check for finalization
      if (((participating && (stage == -1)) || 
            (stage == (shard_collective_stages-1))) &&
          (++last_stage_sends == (shard_collective_last_radix-1)))
      {
        if (stage != -1)
        {
          std::map<int,std::map<ShardID,PendingReduce> >::const_iterator 
            finder = pending_reductions.find(stage);
          if ((finder != pending_reductions.end()) &&
              (finder->second.size() == size_t(shard_collective_last_radix-1)))
            finalize();
        }
        else
          finalize();
      }
    }

    //--------------------------------------------------------------------------
    void FutureAllReduceCollective::unpack_collective_stage(
                                                 Deserializer &derez, int stage)
    //--------------------------------------------------------------------------
    {
      // We never eagerly do reductions as they can arrive out of order
      // and we can't apply them too early or we'll get duplicate 
      // applications of reductions
      ShardID shard;
      derez.deserialize(shard);
      FutureInstance *instance =
        FutureInstance::unpack_instance(derez, context->runtime);
      ApUserEvent postcondition;
      derez.deserialize(postcondition);
      std::map<ShardID,PendingReduce> &pending = pending_reductions[stage];
      pending[shard] = PendingReduce(instance, postcondition);
      if (participating && (stage == -1))
        last_stage_sends--;
      // Check to see if we need to do the finalization
      if ((!participating && (stage == -1)) ||
          ((stage == (shard_collective_stages-1)) &&
           (last_stage_sends == (shard_collective_last_radix-1)) &&
           (pending.size() == size_t(shard_collective_last_radix-1))))
        finalize();
    }

    //--------------------------------------------------------------------------
    void FutureAllReduceCollective::set_shadow_instance(FutureInstance *shadow)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(shadow != NULL);
      assert(shadow_instance == NULL);
#endif
      shadow_instance = shadow;
    }

    //--------------------------------------------------------------------------
    RtEvent FutureAllReduceCollective::async_reduce(FutureInstance *inst,
                                                    ApEvent &ready)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(instance == NULL);
      // We should either have a shadow instance at this point or the nature
      // of the instance is that it is small enough and on system memory so
      // we will be able to do everything ourselves locally.
      assert((shadow_instance != NULL) ||
          ((inst->is_meta_visible) && (inst->size <= LEGION_MAX_RETURN_SIZE)));
#endif
      instance = inst;
      instance_ready = ready;
      // Record that this is the event that will trigger when finished
      ready = finished;
      // This is a small, but important optimization:
      // For futures that are meta visible and less than the size of the
      // maximum pass-by-value size that are not ready yet, delay starting
      // the collective until they are ready so that we can do as much 
      // as possible passing the data by value rather than having to defer
      // to Realm too much.
      if (inst->is_meta_visible && (inst->size <= LEGION_MAX_RETURN_SIZE) &&
          instance_ready.exists() && 
          !instance_ready.has_triggered_faultignorant())
        perform_collective_async(Runtime::protect_event(instance_ready));
      else
        perform_collective_async();
      return perform_collective_wait(false/*block*/);
    }

    //--------------------------------------------------------------------------
    void FutureAllReduceCollective::create_shadow_instance(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(shadow_instance == NULL);
      assert(instance->is_meta_visible);
      assert(instance->size <= LEGION_MAX_RETURN_SIZE);
#endif
      // We're past the mapping stage of the pipeline at this point so
      // it is too late to be making instances the normal way through
      // eager allocation, so we need to just call malloc and make an
      // external allocation. This should only be happening for small 
      // instances in system memory so it should not be a problem.
#ifdef __GNUC__
#if __GNUC__ >= 11
          // GCC is dumb and thinks we need to initialize this buffer
          // before we pass it into the create local call, which we
          // obviously don't need to do, so tell the compiler to shut up
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#endif
      void *buffer = malloc(instance->size);
      shadow_instance = FutureInstance::create_local(buffer,
              instance->size, true/*own*/, context->runtime);
#ifdef __GNUC__
#if __GNUC__ >= 11
#pragma GCC diagnostic pop
#endif
#endif
    }
    
    //--------------------------------------------------------------------------
    void FutureAllReduceCollective::finalize(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      // Should be exactly one stage left
      assert((pending_reductions.size() == 1) || (current_stage == -1));
#endif
      if (!pending_reductions.empty())
      {
        std::map<int,std::map<ShardID,PendingReduce> >::iterator last =
          pending_reductions.begin();
        if (last->first == -1)
        {
          // Copy-in last stage which includes our value so we just overwrite
#ifdef DEBUG_LEGION
          assert(last->second.size() == 1);
#endif
          const PendingReduce &pending = last->second.begin()->second;
          instance_ready =
            instance->copy_from(pending.instance, op, instance_ready);
          if (pending.postcondition.exists())
            Runtime::trigger_event(NULL, pending.postcondition, instance_ready);
          if (pending.instance->deferred_delete(op, instance_ready))
            delete pending.instance;
        }
        else
          instance_ready = perform_reductions(last->second);
        pending_reductions.erase(last);
      }
#ifdef DEBUG_LEGION
      assert(finished.exists());
#endif
      // Trigger the finish event for the collective
      Runtime::trigger_event(NULL, finished, instance_ready);
    }

    //--------------------------------------------------------------------------
    ApEvent FutureAllReduceCollective::perform_reductions(
                      const std::map<ShardID,PendingReduce> &pending_reductions)
    //--------------------------------------------------------------------------
    {
      ApEvent new_instance_ready;
      if (deterministic)
      {
        new_instance_ready = instance_ready;
        for (std::map<ShardID,PendingReduce>::const_iterator it =
              pending_reductions.begin(); it != pending_reductions.end(); it++)
        {
          new_instance_ready = instance->reduce_from(it->second.instance,
              op, redop_id, redop, true/*exclusive*/, new_instance_ready);
          if (it->second.postcondition.exists())
            Runtime::trigger_event(NULL, it->second.postcondition,
                                    new_instance_ready);
          if (it->second.instance->deferred_delete(op, new_instance_ready))
            delete it->second.instance;
        }
      }
      else
      {
        std::set<ApEvent> postconditions;
        for (std::map<ShardID,PendingReduce>::const_iterator it =
              pending_reductions.begin(); it != pending_reductions.end(); it++)
        {
          ApEvent post;
          post = instance->reduce_from(it->second.instance,
            op, redop_id, redop, false/*exclusive*/, instance_ready);
          if (it->second.postcondition.exists())
            Runtime::trigger_event(NULL, it->second.postcondition, post);
          if (post.exists())
            postconditions.insert(post);
          if (it->second.instance->deferred_delete(op, post))
            delete it->second.instance;
        }
        if (!postconditions.empty())
          new_instance_ready = Runtime::merge_events(NULL,postconditions);
      }
      return new_instance_ready;
    }

    /////////////////////////////////////////////////////////////
    // All Reduce Collective 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    template<typename REDOP>
    AllReduceCollective<REDOP>::AllReduceCollective(CollectiveIndexLocation loc,
                                                    ReplicateContext *ctx)
      : AllGatherCollective(loc, ctx), current_stage(-1)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    template<typename REDOP>
    AllReduceCollective<REDOP>::AllReduceCollective(ReplicateContext *ctx,
                                                    CollectiveID id)
      : AllGatherCollective(ctx, id), current_stage(-1)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    template<typename REDOP>
    AllReduceCollective<REDOP>::~AllReduceCollective(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    template<typename REDOP>
    void AllReduceCollective<REDOP>::pack_collective_stage(Serializer &rez,
                                                           int stage)
    //--------------------------------------------------------------------------
    {
      // The first time we pack a stage we merge any values that we had
      // unpacked earlier as they are needed for sending this stage for
      // the first time.
      if (stage != current_stage)
      {
        if (!future_values.empty())
        {
          typename std::map<int,std::vector<typename REDOP::RHS> >::iterator 
            next = future_values.begin();
          if (next->first == current_stage)
          {
            for (typename std::vector<typename REDOP::RHS>::const_iterator it =
                  next->second.begin(); it != next->second.end(); it++)
              REDOP::template fold<true/*exclusive*/>(value, *it);
            future_values.erase(next);
          }
        }
        current_stage = stage;
      }
      rez.serialize(value);
    }

    //--------------------------------------------------------------------------
    template<typename REDOP>
    void AllReduceCollective<REDOP>::unpack_collective_stage(
                                                 Deserializer &derez, int stage)
    //--------------------------------------------------------------------------
    {
      // We never eagerly do reductions as they can arrive out of order
      // and we can't apply them too early or we'll get duplicate 
      // applications of reductions
      typename REDOP::RHS next;
      derez.deserialize(next);
      future_values[stage].push_back(next);
    }
    
    //--------------------------------------------------------------------------
    template<typename REDOP>
    void AllReduceCollective<REDOP>::async_all_reduce(typename REDOP::RHS val)
    //--------------------------------------------------------------------------
    {
      value = val;
      perform_collective_async();
    }

    //--------------------------------------------------------------------------
    template<typename REDOP>
    RtEvent AllReduceCollective<REDOP>::wait_all_reduce(bool block)
    //--------------------------------------------------------------------------
    {
      return perform_collective_wait(block);
    }

    //--------------------------------------------------------------------------
    template<typename REDOP>
    typename REDOP::RHS AllReduceCollective<REDOP>::sync_all_reduce(
                                                        typename REDOP::RHS val)
    //--------------------------------------------------------------------------
    {
      async_all_reduce(val);
      return get_result();
    }

    //--------------------------------------------------------------------------
    template<typename REDOP>
    typename REDOP::RHS AllReduceCollective<REDOP>::get_result(void)
    //--------------------------------------------------------------------------
    {
      // Wait for the results to be ready
      wait_all_reduce(true);
      // Need to avoid races here so we have to always recompute the last stage
      typename REDOP::RHS result = value;
      if (!future_values.empty())
      {
#ifdef DEBUG_LEGION
        // Should be at most one stage left
        assert(future_values.size() == 1);
#endif
        const typename std::map<int,std::vector<typename REDOP::RHS> >::
          const_iterator last = future_values.begin();
        if (last->first == -1)
        {
          // Special case for the last stage which already includes our
          // value so just do the overwrite
#ifdef DEBUG_LEGION
          assert(last->second.size() == 1);
#endif
          result = last->second.front();
        }
        else
        {
          // Do the reduction here
          for (typename std::vector<typename REDOP::RHS>::const_iterator it =
                last->second.begin(); it != last->second.end(); it++)
            REDOP::template fold<true/*exclusive*/>(result, *it);
        }
      }
      return result;
    }

    // Instantiate this for a common use case
    template class AllReduceCollective<ProdReduction<bool> >;

    /////////////////////////////////////////////////////////////
    // Buffer Broadcast
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    void BufferBroadcast::broadcast(void *b, size_t s, bool copy)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(buffer == NULL);
#endif
      if (copy)
      {
        size = s;
        buffer = malloc(size);
        memcpy(buffer, b, size);
        own = true;
      }
      else
      {
        buffer = b;
        size = s;
        own = false;
      }
      perform_collective_async();
    }

    //--------------------------------------------------------------------------
    const void* BufferBroadcast::get_buffer(size_t &s, bool wait)
    //--------------------------------------------------------------------------
    {
      if (wait) 
        perform_collective_wait();
      s = size;
      return buffer;
    }

    //--------------------------------------------------------------------------
    void BufferBroadcast::pack_collective(Serializer &rez) const
    //--------------------------------------------------------------------------
    {
      rez.serialize(size);
      if (size > 0)
        rez.serialize(buffer, size);
    }

    //--------------------------------------------------------------------------
    void BufferBroadcast::unpack_collective(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      derez.deserialize(size);
      if (size > 0)
      {
#ifdef DEBUG_LEGION
        assert(buffer == NULL);
#endif
        buffer = malloc(size);  
        derez.deserialize(buffer, size);
        own = true;
      }
    }

    /////////////////////////////////////////////////////////////
    // Shard Sync Tree 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ShardSyncTree::ShardSyncTree(ReplicateContext *ctx, ShardID origin,
                                 CollectiveIndexLocation loc)
      : GatherCollective(loc, ctx, origin) 
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ShardSyncTree::~ShardSyncTree(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    void ShardSyncTree::pack_collective(Serializer &rez) const
    //--------------------------------------------------------------------------
    {
      RtEvent precondition = get_done_event();
      rez.serialize(precondition);
    }

    //--------------------------------------------------------------------------
    void ShardSyncTree::unpack_collective(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      RtEvent postcondition;
      derez.deserialize(postcondition);
      postconditions.push_back(postcondition);
    }

    //--------------------------------------------------------------------------
    RtEvent ShardSyncTree::post_gather(void)
    //--------------------------------------------------------------------------
    {
      return Runtime::merge_events(postconditions);
    }

    /////////////////////////////////////////////////////////////
    // Shard Event Tree 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ShardEventTree::ShardEventTree(ReplicateContext *ctx, ShardID origin,
                                   CollectiveID id)
      : BroadcastCollective(ctx, id, origin)
    //--------------------------------------------------------------------------
    {
      if (!is_origin())
        precondition = get_done_event();
    }

    //--------------------------------------------------------------------------
    ShardEventTree::~ShardEventTree(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    void ShardEventTree::signal_tree(RtEvent pre)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(is_origin());
      assert(!precondition.exists());
#endif
      precondition = pre;
      perform_collective_async();
    }

    //--------------------------------------------------------------------------
    RtEvent ShardEventTree::get_local_event(void)
    //--------------------------------------------------------------------------
    {
      return perform_collective_wait(false/*block*/); 
    }

    //--------------------------------------------------------------------------
    void ShardEventTree::pack_collective(Serializer &rez) const
    //--------------------------------------------------------------------------
    {
      rez.serialize(precondition);
    }

    //--------------------------------------------------------------------------
    void ShardEventTree::unpack_collective(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      derez.deserialize(postcondition);
    }

    /////////////////////////////////////////////////////////////
    // Single Task Tree 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    SingleTaskTree::SingleTaskTree(ReplicateContext *ctx, ShardID origin, 
                                   CollectiveID id, FutureImpl *impl)
      : ShardEventTree(ctx, origin, id), future(impl), future_size(0),
        has_future_size(false)
    //--------------------------------------------------------------------------
    {
      if (future != NULL)
        future->add_base_gc_ref(PENDING_COLLECTIVE_REF);
    }

    //--------------------------------------------------------------------------
    SingleTaskTree::~SingleTaskTree(void)
    //--------------------------------------------------------------------------
    {
      if ((future != NULL) && 
          future->remove_base_gc_ref(PENDING_COLLECTIVE_REF))
        delete future;
    }
    
    //--------------------------------------------------------------------------
    void SingleTaskTree::broadcast_future_size(RtEvent precondition,
                                               size_t size, bool has_size)
    //--------------------------------------------------------------------------
    {
      future_size = size;
      has_future_size = has_size;
      signal_tree(precondition);
    }

    //--------------------------------------------------------------------------
    void SingleTaskTree::pack_collective(Serializer &rez) const
    //--------------------------------------------------------------------------
    {
      rez.serialize(future_size);
      rez.serialize<bool>(has_future_size);
      ShardEventTree::pack_collective(rez);
    }

    //--------------------------------------------------------------------------
    void SingleTaskTree::unpack_collective(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      derez.deserialize(future_size);
      derez.deserialize<bool>(has_future_size);
      ShardEventTree::unpack_collective(derez);
      if ((future != NULL) && has_future_size)
        future->set_future_result_size(future_size, 
                  context->runtime->address_space);
    }

    /////////////////////////////////////////////////////////////
    // Cross Product Collective 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    CrossProductCollective::CrossProductCollective(ReplicateContext *ctx,
                                                   CollectiveIndexLocation loc)
      : AllGatherCollective(loc, ctx)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    CrossProductCollective::CrossProductCollective(
                                              const CrossProductCollective &rhs)
      : AllGatherCollective(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    CrossProductCollective::~CrossProductCollective(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    CrossProductCollective& CrossProductCollective::operator=(
                                              const CrossProductCollective &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void CrossProductCollective::exchange_partitions(
                                   std::map<IndexSpace,IndexPartition> &handles)
    //--------------------------------------------------------------------------
    {
      // Need the lock in case we are unpacking other things here
      {
        AutoLock c_lock(collective_lock);
        // Only put the non-empty partitions into our local set
        for (std::map<IndexSpace,IndexPartition>::const_iterator it = 
              handles.begin(); it != handles.end(); it++)
        {
          if (!it->second.exists())
            continue;
          non_empty_handles.insert(*it);
        }
      }
      // Now we do the exchange
      perform_collective_sync();
      // When we wake up we should have all the handles and no need the lock
      // to access them
#ifdef DEBUG_LEGION
      assert(handles.size() == non_empty_handles.size());
#endif
      handles = non_empty_handles;
    }

    //--------------------------------------------------------------------------
    void CrossProductCollective::pack_collective_stage(Serializer &rez, 
                                                       int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(non_empty_handles.size());
      for (std::map<IndexSpace,IndexPartition>::const_iterator it = 
            non_empty_handles.begin(); it != non_empty_handles.end(); it++)
      {
        rez.serialize(it->first);
        rez.serialize(it->second);
      }
    }

    //--------------------------------------------------------------------------
    void CrossProductCollective::unpack_collective_stage(Deserializer &derez,
                                                         int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_handles;
      derez.deserialize(num_handles);
      for (unsigned idx = 0; idx < num_handles; idx++)
      {
        IndexSpace handle;
        derez.deserialize(handle);
        derez.deserialize(non_empty_handles[handle]);
      }
    }

    /////////////////////////////////////////////////////////////
    // Sharding Gather Collective 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ShardingGatherCollective::ShardingGatherCollective(ReplicateContext *ctx,
                                   ShardID target, CollectiveIndexLocation loc)
      : GatherCollective(loc, ctx, target)
    //--------------------------------------------------------------------------
    {
    }
    
    //--------------------------------------------------------------------------
    ShardingGatherCollective::ShardingGatherCollective(
                                            const ShardingGatherCollective &rhs)
      : GatherCollective(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ShardingGatherCollective::~ShardingGatherCollective(void)
    //--------------------------------------------------------------------------
    {
      // Make sure that we wait in case we still have messages to pass on
      perform_collective_wait();
    }

    //--------------------------------------------------------------------------
    ShardingGatherCollective& ShardingGatherCollective::operator=(
                                            const ShardingGatherCollective &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ShardingGatherCollective::pack_collective(Serializer &rez) const
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(results.size());
      for (std::map<ShardID,ShardingID>::const_iterator it = 
            results.begin(); it != results.end(); it++)
      {
        rez.serialize(it->first);
        rez.serialize(it->second);
      }
    }

    //--------------------------------------------------------------------------
    void ShardingGatherCollective::unpack_collective(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      size_t num_results;
      derez.deserialize(num_results);
      for (unsigned idx = 0; idx < num_results; idx++)
      {
        ShardID shard;
        derez.deserialize(shard);
        derez.deserialize(results[shard]);
      }
    }

    //--------------------------------------------------------------------------
    void ShardingGatherCollective::contribute(ShardingID value)
    //--------------------------------------------------------------------------
    {
      {
        AutoLock c_lock(collective_lock);
#ifdef DEBUG_LEGION
        assert(results.find(local_shard) == results.end());
#endif
        results[local_shard] = value;
      }
      perform_collective_async();
    }

    //--------------------------------------------------------------------------
    bool ShardingGatherCollective::validate(ShardingID value)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(is_target());
#endif
      // Wait for the results
      perform_collective_wait();
      for (std::map<ShardID,ShardingID>::const_iterator it = 
            results.begin(); it != results.end(); it++)
      {
        if (it->second != value)
          return false;
      }
      return true;
    }

    /////////////////////////////////////////////////////////////
    // Indirect Record Exchange
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    IndirectRecordExchange::IndirectRecordExchange(ReplicateContext *ctx,
                                                   CollectiveID id)
      : AllGatherCollective(ctx, id)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    IndirectRecordExchange::~IndirectRecordExchange(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    RtEvent IndirectRecordExchange::exchange_records(
                            std::vector<std::vector<IndirectRecord>*> &targets,
                            std::vector<IndirectRecord> &records)
    //--------------------------------------------------------------------------
    {
      local_targets.swap(targets);
      all_records.swap(records);
      perform_collective_async();
      return perform_collective_wait(false/*block*/);
    }

    //--------------------------------------------------------------------------
    void IndirectRecordExchange::pack_collective_stage(Serializer &rez,
                                                       int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize(all_records.size());
      for (unsigned idx = 0; idx < all_records.size(); idx++)
        all_records[idx].serialize(rez);
    }

    //--------------------------------------------------------------------------
    void IndirectRecordExchange::unpack_collective_stage(Deserializer &derez,
                                                         int stage)
    //--------------------------------------------------------------------------
    {
      // If we are not a participating stage then we already contributed our
      // data into the output so we clear ourself to avoid double counting
      if (!participating)
      {
#ifdef DEBUG_LEGION
        assert(stage == -1);
#endif
        all_records.clear();
      }
      const size_t offset = all_records.size();
      size_t num_records;
      derez.deserialize(num_records);
      all_records.resize(offset + num_records);
      for (unsigned idx = 0; idx < num_records; idx++)
        all_records[offset+idx].deserialize(derez);
    }

    //--------------------------------------------------------------------------
    RtEvent IndirectRecordExchange::post_complete_exchange(void)
    //--------------------------------------------------------------------------
    {
      for (unsigned idx = 0; idx < local_targets.size(); idx++)
        *local_targets[idx] = all_records;
      return RtEvent::NO_RT_EVENT;
    }

    /////////////////////////////////////////////////////////////
    // Field Descriptor Exchange 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    FieldDescriptorExchange::FieldDescriptorExchange(ReplicateContext *ctx,
                                                   CollectiveIndexLocation loc)
      : AllGatherCollective(loc, ctx)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    FieldDescriptorExchange::FieldDescriptorExchange(
                                             const FieldDescriptorExchange &rhs)
      : AllGatherCollective(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    FieldDescriptorExchange::~FieldDescriptorExchange(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    FieldDescriptorExchange& FieldDescriptorExchange::operator=(
                                             const FieldDescriptorExchange &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    ApEvent FieldDescriptorExchange::exchange_descriptors(ApEvent ready_event,
                                  const std::vector<FieldDataDescriptor> &descs)
    //--------------------------------------------------------------------------
    {
      {
        AutoLock c_lock(collective_lock);
        ready_events.insert(ready_event);
        descriptors.insert(descriptors.end(), descs.begin(), descs.end());
        if (participating)
        {
          remote_to_trigger.resize(shard_collective_stages + 1);
          local_preconditions.resize(shard_collective_stages + 1);
        }
        else
        {
          remote_to_trigger.resize(1);
          local_preconditions.resize(1);
        }
      }
      perform_collective_sync();
      return Runtime::merge_events(NULL, ready_events);
    }

    //--------------------------------------------------------------------------
    ApEvent FieldDescriptorExchange::exchange_completion(ApEvent complete)
    //--------------------------------------------------------------------------
    {
      if (participating)
      {
        // Might have a precondition from a remainder shard 
        if (!local_preconditions[0].empty())
        {
#ifdef DEBUG_LEGION
          assert(local_preconditions[0].size() == 1);
#endif
          complete = Runtime::merge_events(NULL, complete,
              *(local_preconditions[0].begin()));
        }
        const std::set<ApUserEvent> &to_trigger = remote_to_trigger[0];
        for (std::set<ApUserEvent>::const_iterator it = 
              to_trigger.begin(); it != to_trigger.end(); it++)
          Runtime::trigger_event(NULL, *it, complete);
        const ApEvent done = 
          Runtime::merge_events(NULL, local_preconditions.back());
        // If we have a remainder shard then we need to signal them too
        if (!remote_to_trigger[shard_collective_stages].empty())
        {
#ifdef DEBUG_LEGION
          assert(remote_to_trigger[shard_collective_stages].size() == 1);
#endif
          Runtime::trigger_event(NULL,
              *(remote_to_trigger[shard_collective_stages].begin()), done);     
        }
        return done;
      }
      else
      {
        // Not participating so we should have exactly one thing to 
        // trigger and one precondition for being done
#ifdef DEBUG_LEGION
        assert(remote_to_trigger[0].size() == 1);
        assert(local_preconditions[0].size() == 1);
#endif
        Runtime::trigger_event(NULL, *(remote_to_trigger[0].begin()), complete);
        return *(local_preconditions[0].begin());
      }
    }

    //--------------------------------------------------------------------------
    void FieldDescriptorExchange::pack_collective_stage(Serializer &rez,
                                                        int stage)
    //--------------------------------------------------------------------------
    {
      // Always make a stage precondition and send it back
      ApUserEvent stage_complete = Runtime::create_ap_user_event(NULL);
      rez.serialize(stage_complete);
      if (stage == -1)
      {
#ifdef DEBUG_LEGION
        assert(!local_preconditions.empty());
        assert(local_preconditions[0].empty());
#endif
        // Always save this as a precondition for later
        local_preconditions[0].insert(stage_complete);
      }
      else 
      {
#ifdef DEBUG_LEGION
        assert(participating);
        assert(stage < shard_collective_stages);
#endif
        std::set<ApEvent> &preconditions = 
          local_preconditions[shard_collective_stages - stage];
        preconditions.insert(stage_complete);
        // See if we've sent all our messages in which case we can 
        // trigger all the remote user events for any previous stages
        if (((stage == (shard_collective_stages-1)) && 
              (int(preconditions.size()) == shard_collective_last_radix)) ||
            ((stage < (shard_collective_stages-1)) &&
              (int(preconditions.size()) == shard_collective_radix)))
        {
          const std::set<ApUserEvent> &to_trigger = 
           remote_to_trigger[(stage > 0) ? (stage-1) : shard_collective_stages];
          // Check for empty which can happen with stage 0 if there
          // are no remainders
          if (!to_trigger.empty())
          {
            const ApEvent stage_pre = Runtime::merge_events(NULL,preconditions);
            for (std::set<ApUserEvent>::const_iterator it = 
                  to_trigger.begin(); it != to_trigger.end(); it++)
              Runtime::trigger_event(NULL, *it, stage_pre);
          }
        }
      }
      rez.serialize<size_t>(ready_events.size());
      for (std::set<ApEvent>::const_iterator it = ready_events.begin();
            it != ready_events.end(); it++)
        rez.serialize(*it);
      rez.serialize<size_t>(descriptors.size());
      for (std::vector<FieldDataDescriptor>::const_iterator it = 
            descriptors.begin(); it != descriptors.end(); it++)
        rez.serialize(*it);
    }

    //--------------------------------------------------------------------------
    void FieldDescriptorExchange::unpack_collective_stage(Deserializer &derez,
                                                          int stage)
    //--------------------------------------------------------------------------
    {
      ApUserEvent remote_complete;
      derez.deserialize(remote_complete);
      if (stage == -1)
      {
#ifdef DEBUG_LEGION
        assert(!remote_to_trigger.empty());
#endif
        if (participating)
        {
#ifdef DEBUG_LEGION
          assert(remote_to_trigger[shard_collective_stages].empty());
#endif
          remote_to_trigger[shard_collective_stages].insert(remote_complete);
        }
        else
        {
#ifdef DEBUG_LEGION
          assert(remote_to_trigger[0].empty());
#endif
          remote_to_trigger[0].insert(remote_complete);
        }
      }
      else
      {
#ifdef DEBUG_LEGION
        assert(participating);
        assert(stage < int(remote_to_trigger.size()));
#endif
        remote_to_trigger[stage].insert(remote_complete);
      }
      size_t num_events;
      derez.deserialize(num_events);
      for (unsigned idx = 0; idx < num_events; idx++)
      {
        ApEvent ready;
        derez.deserialize(ready);
        ready_events.insert(ready);
      }
      unsigned offset = descriptors.size();
      size_t num_descriptors;
      derez.deserialize(num_descriptors);
      descriptors.resize(offset + num_descriptors);
      for (unsigned idx = 0; idx < num_descriptors; idx++)
        derez.deserialize(descriptors[offset + idx]);
    }

    /////////////////////////////////////////////////////////////
    // Field Descriptor Gather 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    FieldDescriptorGather::FieldDescriptorGather(ReplicateContext *ctx,
                             ShardID target, CollectiveIndexLocation loc)
      : GatherCollective(loc, ctx, target), used(false)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    FieldDescriptorGather::FieldDescriptorGather(
                                               const FieldDescriptorGather &rhs)
      : GatherCollective(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    FieldDescriptorGather::~FieldDescriptorGather(void)
    //--------------------------------------------------------------------------
    {
      // Make sure that we wait in case we still have messages to pass on
      if (used)
        perform_collective_wait();
#ifdef DEBUG_LEGION
      assert(!complete_event.exists() || complete_event.has_triggered());
#endif
    }

    //--------------------------------------------------------------------------
    FieldDescriptorGather& FieldDescriptorGather::operator=(
                                               const FieldDescriptorGather &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void FieldDescriptorGather::pack_collective(Serializer &rez) const
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(complete_event.exists());
#endif
      // Trigger any remote complete events we have dependent on our event
      if (!remote_complete_events.empty())
      {
        for (std::set<ApUserEvent>::const_iterator it = 
              remote_complete_events.begin(); it != 
              remote_complete_events.end(); it++)
          Runtime::trigger_event(NULL, *it, complete_event); 
      }
      rez.serialize(complete_event);
      rez.serialize<size_t>(ready_events.size());
      for (std::set<ApEvent>::const_iterator it = ready_events.begin();
            it != ready_events.end(); it++)
        rez.serialize(*it);
      rez.serialize<size_t>(descriptors.size());
      for (std::vector<FieldDataDescriptor>::const_iterator it = 
            descriptors.begin(); it != descriptors.end(); it++)
        rez.serialize(*it);
    }
    
    //--------------------------------------------------------------------------
    void FieldDescriptorGather::unpack_collective(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      ApUserEvent remote_complete;
      derez.deserialize(remote_complete);
      remote_complete_events.insert(remote_complete);
      size_t num_events;
      derez.deserialize(num_events);
      for (unsigned idx = 0; idx < num_events; idx++)
      {
        ApEvent ready;
        derez.deserialize(ready);
        ready_events.insert(ready);
      }
      unsigned offset = descriptors.size();
      size_t num_descriptors;
      derez.deserialize(num_descriptors);
      descriptors.resize(offset + num_descriptors);
      for (unsigned idx = 0; idx < num_descriptors; idx++)
        derez.deserialize(descriptors[offset + idx]);
    }

    //--------------------------------------------------------------------------
    void FieldDescriptorGather::contribute(ApEvent ready_event,
                                  const std::vector<FieldDataDescriptor> &descs)
    //--------------------------------------------------------------------------
    {
      used = true;
      {
        AutoLock c_lock(collective_lock);
        ready_events.insert(ready_event);
        descriptors.insert(descriptors.end(), descs.begin(), descs.end());
        // If we're not the owner make our complete event
#ifdef DEBUG_LEGION
        assert(!complete_event.exists());
#endif
        if (!is_target())
          complete_event = Runtime::create_ap_user_event(NULL);
      }
      perform_collective_async();
    }

    //--------------------------------------------------------------------------
    const std::vector<FieldDataDescriptor>& 
                     FieldDescriptorGather::get_full_descriptors(ApEvent &ready)
    //--------------------------------------------------------------------------
    {
      perform_collective_wait();
      ready = Runtime::merge_events(NULL, ready_events);
      return descriptors;
    }

    //--------------------------------------------------------------------------
    void FieldDescriptorGather::notify_remote_complete(ApEvent precondition)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(is_target());
#endif
      if (!remote_complete_events.empty())
      {
        for (std::set<ApUserEvent>::const_iterator it = 
              remote_complete_events.begin(); it != 
              remote_complete_events.end(); it++)
          Runtime::trigger_event(NULL, *it, precondition);
      }
    }

    //--------------------------------------------------------------------------
    ApEvent FieldDescriptorGather::get_complete_event(void) const
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!is_target());
      assert(complete_event.exists());
#endif
      return complete_event;
    }

    /////////////////////////////////////////////////////////////
    // Future Broadcast 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    FutureBroadcast::FutureBroadcast(ReplicateContext *ctx, CollectiveID id,
                                     ShardID source, FutureImpl *i)
      : BroadcastCollective(ctx, id, source), impl(i)
    //--------------------------------------------------------------------------
    {
      if (source == ctx->owner_shard->shard_id)
        ready = impl->subscribe();
    }

    //--------------------------------------------------------------------------
    FutureBroadcast::FutureBroadcast(const FutureBroadcast &rhs)
      : BroadcastCollective(rhs), impl(rhs.impl)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    FutureBroadcast::~FutureBroadcast(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    FutureBroadcast& FutureBroadcast::operator=(const FutureBroadcast &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void FutureBroadcast::pack_collective(Serializer &rez) const
    //--------------------------------------------------------------------------
    {
      FutureInstance *instance = impl->get_canonical_instance();
      if (instance != NULL)
        instance->pack_instance(rez, false/*pack ownership*/);
      else
        rez.serialize<size_t>(0);
    }

    //--------------------------------------------------------------------------
    void FutureBroadcast::unpack_collective(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      FutureInstance *instance = 
        FutureInstance::unpack_instance(derez, context->runtime);
      impl->set_result(instance);
    }

    //--------------------------------------------------------------------------
    void FutureBroadcast::broadcast_future(void)
    //--------------------------------------------------------------------------
    {
      if (ready.exists() && !ready.has_triggered())
        ready.wait();
      perform_collective_async();
    }

    /////////////////////////////////////////////////////////////
    // Buffer Exchange 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    BufferExchange::BufferExchange(ReplicateContext *ctx,
                                   CollectiveIndexLocation loc)
      : AllGatherCollective(loc, ctx)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    BufferExchange::BufferExchange(const BufferExchange &rhs)
      : AllGatherCollective(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    BufferExchange::~BufferExchange(void)
    //--------------------------------------------------------------------------
    {
      for (std::map<ShardID,std::pair<void*,size_t> >::const_iterator it = 
            results.begin(); it != results.end(); it++)
        if (it->second.second > 0)
          free(it->second.first);
    }

    //--------------------------------------------------------------------------
    BufferExchange& BufferExchange::operator=(const BufferExchange &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void BufferExchange::pack_collective_stage(Serializer &rez, int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(results.size());
      for (std::map<ShardID,std::pair<void*,size_t> >::const_iterator it =
            results.begin(); it != results.end(); it++)
      {
        rez.serialize(it->first);
        rez.serialize(it->second.second);
        if (it->second.second > 0)
          rez.serialize(it->second.first, it->second.second);
      }
    }

    //--------------------------------------------------------------------------
    void BufferExchange::unpack_collective_stage(Deserializer &derez, int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_results;
      derez.deserialize(num_results);
      for (unsigned idx = 0; idx < num_results; idx++)
      {
        ShardID shard;
        derez.deserialize(shard);
        size_t size;
        derez.deserialize(size);
        if (results.find(shard) != results.end())
        {
          derez.advance_pointer(size);
          continue;
        }
        if (size > 0)
        {
          void *buffer = malloc(size);
          derez.deserialize(buffer, size);
          results[shard] = std::make_pair(buffer, size);
        }
        else
          results[shard] = std::make_pair<void*,size_t>(NULL,0);
      }
    }

    //--------------------------------------------------------------------------
    const std::map<ShardID,std::pair<void*,size_t> >& 
      BufferExchange::exchange_buffers(void *value, size_t size, bool keep_self)
    //--------------------------------------------------------------------------
    {
      // Can put this in without the lock since we haven't started yet
      results[local_shard] = std::make_pair(value, size);
      perform_collective_sync();
      // Remove ourselves after we're done
      if (!keep_self)
        results.erase(local_shard);
      return results;
    }

    //--------------------------------------------------------------------------
    RtEvent BufferExchange::exchange_buffers_async(void *value, size_t size,
                                                   bool keep_self)
    //--------------------------------------------------------------------------
    {
      // Can put this in without the lock since we haven't started yet
      results[local_shard] = std::make_pair(value, size);
      perform_collective_async();
      return perform_collective_wait(false/*block*/);
    }

    //--------------------------------------------------------------------------
    const std::map<ShardID,std::pair<void*,size_t> >& 
                                    BufferExchange::sync_buffers(bool keep_self)
    //--------------------------------------------------------------------------
    {
      perform_collective_wait(true/*block*/);
      if (!keep_self)
        results.erase(local_shard);
      return results;
    }

    /////////////////////////////////////////////////////////////
    // Future Name Exchange 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    FutureNameExchange::FutureNameExchange(ReplicateContext *ctx,
                   CollectiveID id, ReplFutureMapImpl *m, ReferenceMutator *mut)
      : AllGatherCollective(ctx, id), future_map(m), mutator(mut)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    FutureNameExchange::FutureNameExchange(const FutureNameExchange &rhs)
      : AllGatherCollective(rhs), future_map(rhs.future_map), 
        mutator(rhs.mutator)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    FutureNameExchange::~FutureNameExchange(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    FutureNameExchange& FutureNameExchange::operator=(
                                                  const FutureNameExchange &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void FutureNameExchange::pack_collective_stage(Serializer &rez, int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(results.size());
      for (std::map<DomainPoint,Future>::const_iterator it = 
            results.begin(); it != results.end(); it++)
      {
        rez.serialize(it->first);
        if (it->second.impl != NULL)
          it->second.impl->pack_future(rez);
        else
          rez.serialize<DistributedID>(0);
      }
    }

    //--------------------------------------------------------------------------
    void FutureNameExchange::unpack_collective_stage(Deserializer &derez,
                                                     int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_futures;
      derez.deserialize(num_futures);
      Runtime *runtime = context->runtime;
      for (unsigned idx = 0; idx < num_futures; idx++)
      {
        DomainPoint point;
        derez.deserialize(point);
        FutureImpl *impl = FutureImpl::unpack_future(runtime, derez, mutator);
        if (impl != NULL)
        {
          // Add the reference ourselves so we can capture the effects
          impl->add_base_gc_ref(FUTURE_HANDLE_REF, mutator);
          results[point] = Future(impl, false/*need referece*/);
        }
        else
          results[point] = Future();
      }
    }

    //--------------------------------------------------------------------------
    void FutureNameExchange::exchange_future_names(
                                          std::map<DomainPoint,Future> &futures)
    //--------------------------------------------------------------------------
    {
      {
        AutoLock c_lock(collective_lock);
        results.insert(futures.begin(), futures.end());
      }
      perform_collective_sync();
      futures = results;
    }

    /////////////////////////////////////////////////////////////
    // Must Epoch Processor Broadcast 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    MustEpochMappingBroadcast::MustEpochMappingBroadcast(
            ReplicateContext *ctx, ShardID origin, CollectiveID collective_id)
      : BroadcastCollective(ctx, collective_id, origin)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    MustEpochMappingBroadcast::MustEpochMappingBroadcast(
                                           const MustEpochMappingBroadcast &rhs)
      : BroadcastCollective(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    MustEpochMappingBroadcast::~MustEpochMappingBroadcast(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(local_done_event.exists());
#endif
      if (!done_events.empty())
        Runtime::trigger_event(local_done_event,
            Runtime::merge_events(done_events));
      else
        Runtime::trigger_event(local_done_event);
      // This should only happen on the owner node
      if (!held_references.empty())
      {
        // Wait for all the other shards to be done
        local_done_event.wait();
        // Now we can remove our held references
        for (std::set<PhysicalManager*>::const_iterator it = 
              held_references.begin(); it != held_references.end(); it++)
          if ((*it)->remove_base_valid_ref(REPLICATION_REF))
            delete (*it);
      }
    }
    
    //--------------------------------------------------------------------------
    MustEpochMappingBroadcast& MustEpochMappingBroadcast::operator=(
                                           const MustEpochMappingBroadcast &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void MustEpochMappingBroadcast::pack_collective(Serializer &rez) const
    //--------------------------------------------------------------------------
    {
      RtUserEvent next_done = Runtime::create_rt_user_event();
      done_events.insert(next_done);
      rez.serialize(next_done);
      rez.serialize<size_t>(processors.size());
      for (unsigned idx = 0; idx < processors.size(); idx++)
        rez.serialize(processors[idx]);
      rez.serialize<size_t>(instances.size());
      for (unsigned idx = 0; idx < instances.size(); idx++)
      {
        const std::vector<DistributedID> &dids = instances[idx];
        rez.serialize<size_t>(dids.size());
        for (std::vector<DistributedID>::const_iterator it = 
              dids.begin(); it != dids.end(); it++)
          rez.serialize(*it);
      }
    }

    //--------------------------------------------------------------------------
    void MustEpochMappingBroadcast::unpack_collective(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      derez.deserialize(local_done_event);
      size_t num_procs;
      derez.deserialize(num_procs);
      processors.resize(num_procs);
      for (unsigned idx = 0; idx < num_procs; idx++)
        derez.deserialize(processors[idx]);
      size_t num_constraints;
      derez.deserialize(num_constraints);
      instances.resize(num_constraints);
      for (unsigned idx1 = 0; idx1 < num_constraints; idx1++)
      {
        size_t num_dids;
        derez.deserialize(num_dids);
        std::vector<DistributedID> &dids = instances[idx1];
        dids.resize(num_dids);
        for (unsigned idx2 = 0; idx2 < num_dids; idx2++)
          derez.deserialize(dids[idx2]);
      }
    }

    //--------------------------------------------------------------------------
    void MustEpochMappingBroadcast::broadcast(
           const std::vector<Processor> &processor_mapping,
           const std::vector<std::vector<Mapping::PhysicalInstance> > &mappings)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(!local_done_event.exists());
#endif
      local_done_event = Runtime::create_rt_user_event();
      processors = processor_mapping;
      instances.resize(mappings.size());
      // Add valid references to all the physical instances that we will
      // hold until all the must epoch operations are done with the exchange
      WrapperReferenceMutator mutator(done_events);
      for (unsigned idx1 = 0; idx1 < mappings.size(); idx1++)
      {
        std::vector<DistributedID> &dids = instances[idx1];
        dids.resize(mappings[idx1].size());
        for (unsigned idx2 = 0; idx2 < dids.size(); idx2++)
        {
          const Mapping::PhysicalInstance &inst = mappings[idx1][idx2];
          PhysicalManager *manager = inst.impl->as_physical_manager();
          dids[idx2] = manager->did;
          if (held_references.find(manager) != held_references.end())
            continue;
          manager->add_base_valid_ref(REPLICATION_REF, &mutator);
          held_references.insert(manager);
        }
      }
      perform_collective_async();
    }

    //--------------------------------------------------------------------------
    void MustEpochMappingBroadcast::receive_results(
                std::vector<Processor> &processor_mapping,
                const std::vector<unsigned> &constraint_indexes,
                std::vector<std::vector<Mapping::PhysicalInstance> > &mappings,
                std::map<PhysicalManager*,unsigned> &acquired)
    //--------------------------------------------------------------------------
    {
      perform_collective_wait();
      // Just grab all the processors since we still need them
      processor_mapping = processors;
      // We are a little smarter with the mappings since we know exactly
      // which ones we are actually going to need for our local points
      std::set<RtEvent> ready_events;
      Runtime *runtime = manager->runtime;
      for (std::vector<unsigned>::const_iterator it = 
            constraint_indexes.begin(); it != constraint_indexes.end(); it++)
      {
#ifdef DEBUG_LEGION
        assert((*it) < instances.size());
        assert((*it) < mappings.size());
#endif
        const std::vector<DistributedID> &dids = instances[*it];
        std::vector<Mapping::PhysicalInstance> &mapping = mappings[*it];
        mapping.resize(dids.size());
        for (unsigned idx = 0; idx < dids.size(); idx++)
        {
          RtEvent ready;
          mapping[idx].impl = 
            runtime->find_or_request_instance_manager(dids[idx], ready);
          if (!ready.has_triggered())
            ready_events.insert(ready);   
        }
      }
      // Have to wait for the ready events to trigger before we can add
      // our references safely
      if (!ready_events.empty())
      {
        RtEvent ready = Runtime::merge_events(ready_events);
        if (!ready.has_triggered())
          ready.wait();
      }
      // Lastly we need to put acquire references on any of local instances
      WrapperReferenceMutator mutator(done_events);
      for (unsigned idx = 0; idx < constraint_indexes.size(); idx++)
      {
        const unsigned constraint_index = constraint_indexes[idx];
        const std::vector<Mapping::PhysicalInstance> &mapping = 
          mappings[constraint_index];
        // Also grab an acquired reference to these instances
        for (std::vector<Mapping::PhysicalInstance>::const_iterator it = 
              mapping.begin(); it != mapping.end(); it++)
        {
          PhysicalManager *manager = it->impl->as_physical_manager();
          // If we already had a reference to this instance
          // then we don't need to add any additional ones
          if (acquired.find(manager) != acquired.end())
            continue;
          manager->add_base_valid_ref(MAPPING_ACQUIRE_REF, &mutator);
          acquired[manager] = 1/*count*/; 
        }
      }
    }

    /////////////////////////////////////////////////////////////
    // Must Epoch Mapping Exchange
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    MustEpochMappingExchange::MustEpochMappingExchange(ReplicateContext *ctx,
                                                 CollectiveID collective_id)
      : AllGatherCollective(ctx, collective_id)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    MustEpochMappingExchange::MustEpochMappingExchange(
                                            const MustEpochMappingExchange &rhs)
      : AllGatherCollective(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    MustEpochMappingExchange::~MustEpochMappingExchange(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(local_done_event.exists()); // better have one of these
#endif
      Runtime::trigger_event(local_done_event);
      // See if we need to wait for others to be done before we can
      // remove our valid references
      if (!done_events.empty())
      {
        RtEvent done = Runtime::merge_events(done_events);
        if (!done.has_triggered())
          done.wait();
      }
      // Now we can remove our held references
      for (std::set<PhysicalManager*>::const_iterator it = 
            held_references.begin(); it != held_references.end(); it++)
        if ((*it)->remove_base_valid_ref(REPLICATION_REF))
          delete (*it);
    }
    
    //--------------------------------------------------------------------------
    MustEpochMappingExchange& MustEpochMappingExchange::operator=(
                                            const MustEpochMappingExchange &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void MustEpochMappingExchange::pack_collective_stage(Serializer &rez,
                                                         int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(processors.size());
      for (std::map<DomainPoint,Processor>::const_iterator it = 
            processors.begin(); it != processors.end(); it++)
      {
        rez.serialize(it->first);
        rez.serialize(it->second);
      }
      rez.serialize<size_t>(constraints.size());
      for (std::map<unsigned,ConstraintInfo>::const_iterator it = 
            constraints.begin(); it != constraints.end(); it++)
      {
        rez.serialize(it->first);
        rez.serialize<size_t>(it->second.instances.size());
        for (unsigned idx = 0; idx < it->second.instances.size(); idx++)
          rez.serialize(it->second.instances[idx]);
        rez.serialize(it->second.origin_shard);
        rez.serialize(it->second.weight);
      }
      rez.serialize<size_t>(done_events.size());
      for (std::set<RtEvent>::const_iterator it = 
            done_events.begin(); it != done_events.end(); it++)
        rez.serialize(*it);
    }

    //--------------------------------------------------------------------------
    void MustEpochMappingExchange::unpack_collective_stage(Deserializer &derez,
                                                           int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_procs;
      derez.deserialize(num_procs);
      for (unsigned idx = 0; idx < num_procs; idx++)
      {
        DomainPoint point;
        derez.deserialize(point);
        derez.deserialize(processors[point]);
      }
      size_t num_mappings;
      derez.deserialize(num_mappings);
      for (unsigned idx1 = 0; idx1 < num_mappings; idx1++)
      {
        unsigned constraint_index;
        derez.deserialize(constraint_index);
        std::map<unsigned,ConstraintInfo>::iterator
          finder = constraints.find(constraint_index);
        if (finder == constraints.end())
        {
          // Can unpack directly since we're first
          ConstraintInfo &info = constraints[constraint_index];
          size_t num_dids;
          derez.deserialize(num_dids);
          info.instances.resize(num_dids);
          for (unsigned idx2 = 0; idx2 < num_dids; idx2++)
            derez.deserialize(info.instances[idx2]);
          derez.deserialize(info.origin_shard);
          derez.deserialize(info.weight);
        }
        else
        {
          // Unpack into a temporary
          ConstraintInfo info;
          size_t num_dids;
          derez.deserialize(num_dids);
          info.instances.resize(num_dids);
          for (unsigned idx2 = 0; idx2 < num_dids; idx2++)
            derez.deserialize(info.instances[idx2]);
          derez.deserialize(info.origin_shard);
          derez.deserialize(info.weight);
          // Only keep the result if we have a larger weight
          // or we have the same weight and a smaller shard
          if ((info.weight > finder->second.weight) ||
              ((info.weight == finder->second.weight) &&
               (info.origin_shard < finder->second.origin_shard)))
            finder->second = info;
        }
      }
      size_t num_done;
      derez.deserialize(num_done);
      for (unsigned idx = 0; idx < num_done; idx++)
      {
        RtEvent done_event;
        derez.deserialize(done_event);
        done_events.insert(done_event);
      }
    }

    //--------------------------------------------------------------------------
    void MustEpochMappingExchange::exchange_must_epoch_mappings(
                ShardID shard_id, size_t total_shards, size_t total_constraints,
                const std::vector<const Task*> &local_tasks,
                const std::vector<const Task*> &all_tasks,
                      std::vector<Processor> &processor_mapping,
                const std::vector<unsigned> &constraint_indexes,
                std::vector<std::vector<Mapping::PhysicalInstance> > &mappings,
                const std::vector<int> &mapping_weights,
                std::map<PhysicalManager*,unsigned> &acquired)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(local_tasks.size() == processor_mapping.size());
      assert(constraint_indexes.size() == mappings.size());
#endif
      // Add valid references to all the physical instances that we will
      // hold until all the must epoch operations are done with the exchange
      WrapperReferenceMutator mutator(done_events);
      for (unsigned idx = 0; idx < mappings.size(); idx++)
      {
        for (std::vector<Mapping::PhysicalInstance>::const_iterator it = 
              mappings[idx].begin(); it != mappings[idx].end(); it++)
        {
          PhysicalManager *manager = it->impl->as_physical_manager();
          if (held_references.find(manager) != held_references.end())
            continue;
          manager->add_base_valid_ref(REPLICATION_REF, &mutator);
          held_references.insert(manager);
        }
      }
#ifdef DEBUG_LEGION
      assert(!local_done_event.exists());
#endif
      local_done_event = Runtime::create_rt_user_event();
      // Then we can add our instances to the set and do the exchange
      {
        AutoLock c_lock(collective_lock);
        for (unsigned idx = 0; idx < local_tasks.size(); idx++)
        {
          const Task *task = local_tasks[idx];
#ifdef DEBUG_LEGION
          assert(processors.find(task->index_point) == processors.end());
#endif
          processors[task->index_point] = processor_mapping[idx];
        }
        for (unsigned idx1 = 0; idx1 < mappings.size(); idx1++)
        {
          const unsigned constraint_index = constraint_indexes[idx1]; 
#ifdef DEBUG_LEGION
          assert(constraint_index < total_constraints);
#endif
          std::map<unsigned,ConstraintInfo>::iterator
            finder = constraints.find(constraint_index);
          // Only add it if it doesn't exist or it has a lower weight
          // or it has the same weight and is a lower shard
          if ((finder == constraints.end()) || 
              (mapping_weights[idx1] > finder->second.weight) ||
              ((mapping_weights[idx1] == finder->second.weight) &&
               (shard_id < finder->second.origin_shard)))
          {
            ConstraintInfo &info = constraints[constraint_index];
            info.instances.resize(mappings[idx1].size());
            for (unsigned idx2 = 0; idx2 < mappings[idx1].size(); idx2++)
              info.instances[idx2] = mappings[idx1][idx2].impl->did;
            info.origin_shard = shard_id;
            info.weight = mapping_weights[idx1];
          }
        }
        // Also update the local done events
        done_events.insert(local_done_event);
      }
      perform_collective_sync();
      // Start fetching the all the mapping results to get them in flight
      mappings.clear();
      mappings.resize(total_constraints);
      std::set<RtEvent> ready_events;
      Runtime *runtime = manager->runtime;
      // We only need to get the results for local constraints as we 
      // know that we aren't going to care about any of the rest
      for (unsigned idx1 = 0; idx1 < constraint_indexes.size(); idx1++)
      {
        const unsigned constraint_index = constraint_indexes[idx1];
        const std::vector<DistributedID> &dids = 
          constraints[constraint_index].instances;
        std::vector<Mapping::PhysicalInstance> &mapping = 
          mappings[constraint_index];
        mapping.resize(dids.size());
        for (unsigned idx2 = 0; idx2 < dids.size(); idx2++)
        {
          RtEvent ready;
          mapping[idx2].impl = 
            runtime->find_or_request_instance_manager(dids[idx2], ready);
          if (!ready.has_triggered())
            ready_events.insert(ready);   
        }
      }
      // Update the processor mapping
      processor_mapping.resize(all_tasks.size());
      for (unsigned idx = 0; idx < all_tasks.size(); idx++)
      {
        const Task *task = all_tasks[idx];
        std::map<DomainPoint,Processor>::const_iterator finder = 
          processors.find(task->index_point);
#ifdef DEBUG_LEGION
        assert(finder != processors.end());
#endif
        processor_mapping[idx] = finder->second;
      }
      // Wait for all the instances to be ready
      if (!ready_events.empty())
      {
        RtEvent ready = Runtime::merge_events(ready_events);
        if (!ready.has_triggered())
          ready.wait();
      }
      // Lastly we need to put acquire references on any of local instances
      for (unsigned idx = 0; idx < constraint_indexes.size(); idx++)
      {
        const unsigned constraint_index = constraint_indexes[idx];
        const std::vector<Mapping::PhysicalInstance> &mapping = 
          mappings[constraint_index];
        // Also grab an acquired reference to these instances
        for (std::vector<Mapping::PhysicalInstance>::const_iterator it = 
              mapping.begin(); it != mapping.end(); it++)
        {
          PhysicalManager *manager = it->impl->as_physical_manager();
          // If we already had a reference to this instance
          // then we don't need to add any additional ones
          if (acquired.find(manager) != acquired.end())
            continue;
          manager->add_base_valid_ref(MAPPING_ACQUIRE_REF, &mutator);
          acquired[manager] = 1/*count*/;
        }
      }
    }

    /////////////////////////////////////////////////////////////
    // Must Epoch Dependence Exchange
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    MustEpochDependenceExchange::MustEpochDependenceExchange(
                             ReplicateContext *ctx, CollectiveIndexLocation loc)
      : AllGatherCollective(loc, ctx)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    MustEpochDependenceExchange::MustEpochDependenceExchange(
                                         const MustEpochDependenceExchange &rhs)
      : AllGatherCollective(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    MustEpochDependenceExchange::~MustEpochDependenceExchange(void)
    //--------------------------------------------------------------------------
    {
    }
    
    //--------------------------------------------------------------------------
    MustEpochDependenceExchange& MustEpochDependenceExchange::operator=(
                                         const MustEpochDependenceExchange &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void MustEpochDependenceExchange::pack_collective_stage(Serializer &rez,
                                                            int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(mapping_dependences.size());
      for (std::map<DomainPoint,RtUserEvent>::const_iterator it = 
            mapping_dependences.begin(); it != mapping_dependences.end(); it++)
      {
        rez.serialize(it->first);
        rez.serialize(it->second);
      }
    }

    //--------------------------------------------------------------------------
    void MustEpochDependenceExchange::unpack_collective_stage(
                                                 Deserializer &derez, int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_deps;
      derez.deserialize(num_deps);
      for (unsigned idx = 0; idx < num_deps; idx++)
      {
        DomainPoint point;
        derez.deserialize(point);
        derez.deserialize(mapping_dependences[point]);
      }
    }

    //--------------------------------------------------------------------------
    void MustEpochDependenceExchange::exchange_must_epoch_dependences(
                               std::map<DomainPoint,RtUserEvent> &mapped_events)
    //--------------------------------------------------------------------------
    {
      {
        AutoLock c_lock(collective_lock);
        for (std::map<DomainPoint,RtUserEvent>::const_iterator it = 
              mapped_events.begin(); it != mapped_events.end(); it++)
          mapping_dependences.insert(*it);
      }
      perform_collective_sync();
      // No need to hold the lock after the collective is complete
      mapped_events.swap(mapping_dependences);
    }

    /////////////////////////////////////////////////////////////
    // Must Epoch Completion Exchange
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    MustEpochCompletionExchange::MustEpochCompletionExchange(
                             ReplicateContext *ctx, CollectiveIndexLocation loc)
      : AllGatherCollective(loc, ctx)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    MustEpochCompletionExchange::MustEpochCompletionExchange(
                                         const MustEpochCompletionExchange &rhs)
      : AllGatherCollective(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    MustEpochCompletionExchange::~MustEpochCompletionExchange(void)
    //--------------------------------------------------------------------------
    {
    }
    
    //--------------------------------------------------------------------------
    MustEpochCompletionExchange& MustEpochCompletionExchange::operator=(
                                         const MustEpochCompletionExchange &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void MustEpochCompletionExchange::pack_collective_stage(Serializer &rez,
                                                            int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(tasks_mapped.size());
      for (std::set<RtEvent>::const_iterator it = 
            tasks_mapped.begin(); it != tasks_mapped.end(); it++)
        rez.serialize(*it);
      rez.serialize<size_t>(tasks_complete.size());
      for (std::set<ApEvent>::const_iterator it = 
            tasks_complete.begin(); it != tasks_complete.end(); it++)
        rez.serialize(*it);
    }

    //--------------------------------------------------------------------------
    void MustEpochCompletionExchange::unpack_collective_stage(
                                                 Deserializer &derez, int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_mapped;
      derez.deserialize(num_mapped);
      for (unsigned idx = 0; idx < num_mapped; idx++)
      {
        RtEvent mapped;
        derez.deserialize(mapped);
        tasks_mapped.insert(mapped);
      }
      size_t num_complete;
      derez.deserialize(num_complete);
      for (unsigned idx = 0; idx < num_complete; idx++)
      {
        ApEvent complete;
        derez.deserialize(complete);
        tasks_complete.insert(complete);
      }
    }

    //--------------------------------------------------------------------------
    void MustEpochCompletionExchange::exchange_must_epoch_completion(
                                RtEvent mapped, ApEvent complete,
                                std::set<RtEvent> &all_mapped,
                                std::set<ApEvent> &all_complete)
    //--------------------------------------------------------------------------
    {
      {
        AutoLock c_lock(collective_lock);
        tasks_mapped.insert(mapped);
        tasks_complete.insert(complete);
      }
      perform_collective_sync();
      // No need to hold the lock after the collective is complete
      all_mapped.swap(tasks_mapped);
      all_complete.swap(tasks_complete);
    }

    /////////////////////////////////////////////////////////////
    // Check Collective Mapping
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    CheckCollectiveMapping::CheckCollectiveMapping(ReplicateContext *ctx,
                                                   CollectiveID id)
      : AllGatherCollective<true>(ctx, id)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    CheckCollectiveMapping::~CheckCollectiveMapping(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    void CheckCollectiveMapping::pack_collective_stage(Serializer &rez,
                                                       int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(mapped_instances.size());
      for (std::map<PhysicalInstance,ShardFields>::const_iterator mit =
            mapped_instances.begin(); mit != mapped_instances.end(); mit++)
      {
        rez.serialize(mit->first);
        rez.serialize<size_t>(mit->second.size());
        for (ShardFields::const_iterator it = mit->second.begin();
              it != mit->second.end(); it++)
        {
          rez.serialize(it->first);
          rez.serialize(it->second);
        }
      }
    }

    //--------------------------------------------------------------------------
    void CheckCollectiveMapping::unpack_collective_stage(Deserializer &derez,
                                                         int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_instances;
      derez.deserialize(num_instances);
      for (unsigned idx1 = 0; idx1 < num_instances; idx1++)
      {
        PhysicalInstance inst;
        derez.deserialize(inst);
        ShardFields &shard_fields = mapped_instances[inst];
        size_t offset = shard_fields.size();
        size_t num_copies;
        derez.deserialize(num_copies);
        shard_fields.resize(offset+num_copies);
        for (unsigned idx2 = 0; idx2 < num_copies; idx2++)
        {
          derez.deserialize(shard_fields[offset+idx2].first);
          derez.deserialize(shard_fields[offset+idx2].second);
        }
      }
    }

    //--------------------------------------------------------------------------
    void CheckCollectiveMapping::verify(const InstanceSet &instances,
                                        MapperManager *mapper)
    //--------------------------------------------------------------------------
    {
      for (unsigned idx = 0; idx < instances.size(); idx++) 
      {
        const InstanceRef &ref = instances[idx];
        PhysicalManager *manager = ref.get_physical_manager();
        PhysicalInstance inst = manager->get_instance();
        mapped_instances[inst].emplace_back(
            std::make_pair(local_shard, ref.get_valid_fields()));
      }
      perform_collective_sync();
      for (unsigned idx = 0; idx < instances.size(); idx++) 
      {
        const InstanceRef &ref = instances[idx];
        PhysicalManager *manager = ref.get_physical_manager();
        PhysicalInstance inst = manager->get_instance();
        ShardFields &shard_fields = mapped_instances[inst];
#ifdef DEBUG_LEGION
        assert(!shard_fields.empty());
#endif
        for (ShardFields::const_iterator it =
              shard_fields.begin(); it != shard_fields.end(); it++)
        {
          if (it->first == local_shard)
            continue;
          if (it->second * ref.get_valid_fields())
            continue;
          REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
                  "Invalid mapper output from invocation of 'map_inline' "
                  "by mapper %s. Mapper selected the same physical instance "
                  IDFMT " on both shards %d and %d with write privileges for "
                  "inline mapping in control-replicated parent task %s "
                  "(UID %lld). Each inline mapping with write privileges in a "
                  "control-replicated parent task must map to a different "
                  "physical instance to avoid races.",
                  mapper->get_mapper_name(), inst.id, local_shard, it->first,
                  context->get_task_name(), context->get_unique_id())
        }
      }
    }

    /////////////////////////////////////////////////////////////
    // Check Collective Sources
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    CheckCollectiveSources::CheckCollectiveSources(ReplicateContext *ctx,
                                                   CollectiveID id)
      : BroadcastCollective(ctx, id, 0/*origin shard*/)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    CheckCollectiveSources::~CheckCollectiveSources(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    void CheckCollectiveSources::pack_collective(Serializer &rez) const
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(source_instances.size());
      for (std::vector<DistributedID>::const_iterator it =
            source_instances.begin(); it != source_instances.end(); it++)
        rez.serialize(*it);
    }

    //--------------------------------------------------------------------------
    void CheckCollectiveSources::unpack_collective(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      size_t num_instances;
      derez.deserialize(num_instances);
      source_instances.resize(num_instances);
      for (unsigned idx = 0; idx < num_instances; idx++)
        derez.deserialize(source_instances[idx]);
    }

    //--------------------------------------------------------------------------
    bool CheckCollectiveSources::verify(
                                 const std::vector<PhysicalManager*> &instances)
    //--------------------------------------------------------------------------
    {
      if (local_shard == 0)
      {
        source_instances.resize(instances.size());
        for (unsigned idx = 0; idx < instances.size(); idx++)
          source_instances[idx] = instances[idx]->did;
        perform_collective_async();
      }
      else
      {
        perform_collective_wait();
        if (instances.size() != source_instances.size())
          return false;
        for (unsigned idx = 0; idx < instances.size(); idx++)
          if (source_instances[idx] != instances[idx]->did)
            return false;
      }
      return true;
    }

    /////////////////////////////////////////////////////////////
    // Template Index Exchange 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    TemplateIndexExchange::TemplateIndexExchange(ReplicateContext *ctx,
                                                 CollectiveID id)
      : AllGatherCollective(ctx, id), current_stage(-1)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    TemplateIndexExchange::TemplateIndexExchange(
                                               const TemplateIndexExchange &rhs)
      : AllGatherCollective(rhs), current_stage(-1)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    TemplateIndexExchange::~TemplateIndexExchange(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    TemplateIndexExchange& TemplateIndexExchange::operator=(
                                               const TemplateIndexExchange &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void TemplateIndexExchange::pack_collective_stage(Serializer &rez,int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(index_counts.size());
      for (std::map<int,unsigned>::const_iterator it = 
            index_counts.begin(); it != index_counts.end(); it++)
      {
        rez.serialize(it->first);
        rez.serialize(it->second);
      }
    }
    
    //--------------------------------------------------------------------------
    void TemplateIndexExchange::unpack_collective_stage(Deserializer &derez,
                                                        int stage)
    //--------------------------------------------------------------------------
    {
      // If we are not a participating stage then we already contributed our
      // data into the output so we clear ourself to avoid double counting
      if (!participating)
      {
#ifdef DEBUG_LEGION
        assert(stage == -1);
#endif
        index_counts.clear();
      }
      size_t num_counts;
      derez.deserialize(num_counts);
      for (unsigned idx = 0; idx < num_counts; idx++)
      {
        int index;
        derez.deserialize(index);
        unsigned count;
        derez.deserialize(count);
        std::map<int,unsigned>::iterator finder = index_counts.find(index);
        if (finder == index_counts.end())
          index_counts[index] = count;
        else
          finder->second += count;
      }
    }

    //--------------------------------------------------------------------------
    void TemplateIndexExchange::initiate_exchange(
                                                const std::vector<int> &indexes)
    //--------------------------------------------------------------------------
    {
      for (std::vector<int>::const_iterator it = indexes.begin();
            it != indexes.end(); it++)
        index_counts[*it] = 1;
      perform_collective_async();
    }

    //--------------------------------------------------------------------------
    void TemplateIndexExchange::complete_exchange(
                                          std::map<int,unsigned> &result_counts)
    //--------------------------------------------------------------------------
    {
      perform_collective_wait(true/*block*/);
      result_counts.swap(index_counts);
    }

    /////////////////////////////////////////////////////////////
    // Unordered Exchange 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    UnorderedExchange::UnorderedExchange(ReplicateContext *ctx, 
                                         CollectiveIndexLocation loc)
      : AllGatherCollective(loc, ctx)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    UnorderedExchange::UnorderedExchange(const UnorderedExchange &rhs)
      : AllGatherCollective(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    UnorderedExchange::~UnorderedExchange(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    UnorderedExchange& UnorderedExchange::operator=(
                                                   const UnorderedExchange &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    template<typename T>
    void UnorderedExchange::update_future_counts(const int stage,
                            std::map<int,std::map<T,unsigned> > &future_counts,
                            std::map<T,unsigned> &counts)
    //--------------------------------------------------------------------------
    {
      typename std::map<int,std::map<T,unsigned> >::iterator next =
        future_counts.find(stage-1);
      if (next != future_counts.end())
      {
        for (typename std::map<T,unsigned>::const_iterator it = 
              next->second.begin(); it != next->second.end(); it++)
        {
          typename std::map<T,unsigned>::iterator finder = 
            counts.find(it->first);
          if (finder == counts.end())
            counts.insert(*it);
          else
            finder->second += it->second;
        }
        future_counts.erase(next);
      }
    }

    //--------------------------------------------------------------------------
    template<typename T>
    void UnorderedExchange::pack_counts(Serializer &rez,   
                                        const std::map<T,unsigned> &counts)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(counts.size());
      for (typename std::map<T,unsigned>::const_iterator it = 
            counts.begin(); it != counts.end(); it++)
      {
        rez.serialize(it->first);
        rez.serialize(it->second);
      }
    }

    //--------------------------------------------------------------------------
    template<typename T>
    void UnorderedExchange::unpack_counts(const int stage, Deserializer &derez,   
                                          std::map<T,unsigned> &counts)
    //--------------------------------------------------------------------------
    {
      size_t num_counts;
      derez.deserialize(num_counts);
      if (num_counts == 0)
        return;
      for (unsigned idx = 0; idx < num_counts; idx++)
      {
        T key;
        derez.deserialize(key);
        typename std::map<T,unsigned>::iterator finder = counts.find(key);
        if (finder != counts.end())
        {
          unsigned count;
          derez.deserialize(count);
          finder->second += count;
        }
        else
          derez.deserialize(counts[key]);
      }
    }

    //--------------------------------------------------------------------------
    template<typename T>
    void UnorderedExchange::pack_field_counts(Serializer &rez,
                          const std::map<std::pair<T,FieldID>,unsigned> &counts)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(counts.size());
      for (typename std::map<std::pair<T,FieldID>,unsigned>::const_iterator it =
            counts.begin(); it != counts.end(); it++)
      {
        rez.serialize(it->first.first);
        rez.serialize(it->first.second);
        rez.serialize(it->second);
      }
    }

    //--------------------------------------------------------------------------
    template<typename T>
    void UnorderedExchange::unpack_field_counts(const int stage,
        Deserializer &derez, std::map<std::pair<T,FieldID>,unsigned> &counts)
    //--------------------------------------------------------------------------
    {
      size_t num_counts;
      derez.deserialize(num_counts);
      if (num_counts == 0)
        return;
      for (unsigned idx = 0; idx < num_counts; idx++)
      {
        std::pair<T,FieldID> key;
        derez.deserialize(key.first);
        derez.deserialize(key.second);
        typename std::map<std::pair<T,FieldID>,unsigned>::iterator finder =
          counts.find(key);
        if (finder != counts.end())
        {
          unsigned count;
          derez.deserialize(count);
          finder->second += count;
        }
        else
          derez.deserialize(counts[key]);
      }
    }

    //--------------------------------------------------------------------------
    template<typename T, typename OP>
    void UnorderedExchange::initialize_counts(const std::map<T,OP*> &ops,
                                              std::map<T,unsigned> &counts)
    //--------------------------------------------------------------------------
    {
      for (typename std::map<T,OP*>::const_iterator it = 
            ops.begin(); it != ops.end(); it++) 
        counts[it->first] = 1;
    }

    //--------------------------------------------------------------------------
    template<typename T, typename OP>
    void UnorderedExchange::find_ready_ops(const size_t total_shards,
                const std::map<T,unsigned> &final_counts,
                const std::map<T,OP*> &ops, std::vector<Operation*> &ready_ops)
    //--------------------------------------------------------------------------
    {
      for (typename std::map<T,unsigned>::const_iterator it = 
            final_counts.begin(); it != final_counts.end(); it++)
      {
#ifdef DEBUG_LEGION
        assert(it->second <= total_shards);
#endif
        if (it->second == total_shards)
        {
          typename std::map<T,OP*>::const_iterator finder = ops.find(it->first);
#ifdef DEBUG_LEGION
          assert(finder != ops.end());
#endif
          ready_ops.push_back(finder->second);
        }
      }
    }

    //--------------------------------------------------------------------------
    void UnorderedExchange::pack_collective_stage(Serializer &rez, int stage)
    //--------------------------------------------------------------------------
    {
      pack_counts(rez, index_space_counts);
      pack_counts(rez, index_partition_counts);
      pack_counts(rez, field_space_counts);
      pack_field_counts(rez, field_counts);
      pack_counts(rez, logical_region_counts);
      pack_field_counts(rez, detach_counts);
    }

    //--------------------------------------------------------------------------
    void UnorderedExchange::unpack_collective_stage(Deserializer &derez, 
                                                    int stage)
    //--------------------------------------------------------------------------
    {
      // If we are not a participating stage then we already contributed our
      // data into the output so we clear ourself to avoid double counting
      if (!participating)
      {
#ifdef DEBUG_LEGION
        assert(stage == -1);
#endif
        index_space_counts.clear();
        index_partition_counts.clear();
        field_space_counts.clear();
        field_counts.clear();
        logical_region_counts.clear();
        detach_counts.clear();
      }
      unpack_counts(stage, derez, index_space_counts);
      unpack_counts(stage, derez, index_partition_counts);
      unpack_counts(stage, derez, field_space_counts);
      unpack_field_counts(stage, derez, field_counts);
      unpack_counts(stage, derez, logical_region_counts);
      unpack_field_counts(stage, derez, detach_counts);
    }

    //--------------------------------------------------------------------------
    bool UnorderedExchange::exchange_unordered_ops(
                                    const std::list<Operation*> &unordered_ops,
                                          std::vector<Operation*> &ready_ops)
    //--------------------------------------------------------------------------
    {
      // Sort our operations
      if (!unordered_ops.empty())
      {
        for (std::list<Operation*>::const_iterator it = 
              unordered_ops.begin(); it != unordered_ops.end(); it++)
        {
          switch ((*it)->get_operation_kind())
          {
            case Operation::DELETION_OP_KIND:
              {
#ifdef DEBUG_LEGION
                ReplDeletionOp *op = dynamic_cast<ReplDeletionOp*>(*it);
                assert(op != NULL);
#else
                ReplDeletionOp *op = static_cast<ReplDeletionOp*>(*it);
#endif
                op->record_unordered_kind(index_space_deletions,
                    index_partition_deletions, field_space_deletions,
                    field_deletions, logical_region_deletions); 
                break; 
              }
            case Operation::DETACH_OP_KIND:
              {
#ifdef DEBUG_LEGION
                ReplDetachOp *op = dynamic_cast<ReplDetachOp*>(*it);
                assert(op != NULL);
#else
                ReplDetachOp *op = static_cast<ReplDetachOp*>(*it);
#endif
                op->record_unordered_kind(detachments);
                break;
              }
            default: // Unimplemented operation kind
              assert(false);
          }
        }
        // Set the initial counts to one for all our unordered ops
        initialize_counts(index_space_deletions, index_space_counts);
        initialize_counts(index_partition_deletions, index_partition_counts);
        initialize_counts(field_space_deletions, field_space_counts);
        initialize_counts(field_deletions, field_counts);
        initialize_counts(logical_region_deletions, logical_region_counts);
        initialize_counts(detachments, detach_counts);
      }
      // Perform the exchange
      perform_collective_sync();
      // Now look and see which operations have keys for all shards 
      // Only need to do this if we have ops, if we didn't have ops then
      // it's impossible for anyone else to have them all too
      if (!unordered_ops.empty())
      {
        const size_t total_shards = manager->total_shards;
        find_ready_ops(total_shards, index_space_counts,
                       index_space_deletions, ready_ops);
        find_ready_ops(total_shards, index_partition_counts,
                       index_partition_deletions, ready_ops);
        find_ready_ops(total_shards, field_space_counts,
                       field_space_deletions, ready_ops);
        find_ready_ops(total_shards, field_counts,
                       field_deletions, ready_ops);
        find_ready_ops(total_shards, logical_region_counts,
                       logical_region_deletions, ready_ops);
        find_ready_ops(total_shards, detach_counts,
                       detachments, ready_ops);
      }
      // Return true if anybody anywhere had a non-zero count
      return (!index_space_counts.empty() || !index_partition_counts.empty() ||
          !field_space_counts.empty() || !field_counts.empty() || 
          !logical_region_counts.empty() || !detach_counts.empty());
    }

    /////////////////////////////////////////////////////////////
    // Consensus Match Base 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ConsensusMatchBase::ConsensusMatchBase(ReplicateContext *ctx,
                                           CollectiveIndexLocation loc)
      : AllGatherCollective(loc, ctx)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ConsensusMatchBase::ConsensusMatchBase(const ConsensusMatchBase &rhs)
      : AllGatherCollective(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ConsensusMatchBase::~ConsensusMatchBase(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    /*static*/ void ConsensusMatchBase::handle_consensus_match(const void *args)
    //--------------------------------------------------------------------------
    {
      const ConsensusMatchArgs *margs = (const ConsensusMatchArgs*)args;
      margs->base->complete_exchange();
      delete margs->base;
    }

    /////////////////////////////////////////////////////////////
    // Consensus Match Exchange 
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    template<typename T>
    ConsensusMatchExchange<T>::ConsensusMatchExchange(ReplicateContext *ctx,
             CollectiveIndexLocation loc, Future f, void *out, ApUserEvent trig)
      : ConsensusMatchBase(ctx, loc), to_complete(f),
        output(static_cast<T*>(out)), to_trigger(trig)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    template<typename T>
    ConsensusMatchExchange<T>::ConsensusMatchExchange(
                                              const ConsensusMatchExchange &rhs)
      : ConsensusMatchBase(rhs), to_complete(rhs.to_complete),
        output(rhs.output), to_trigger(rhs.to_trigger)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    template<typename T>
    ConsensusMatchExchange<T>::~ConsensusMatchExchange(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    template<typename T>
    ConsensusMatchExchange<T>& ConsensusMatchExchange<T>::operator=(
                                              const ConsensusMatchExchange &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    template<typename T>
    void ConsensusMatchExchange<T>::pack_collective_stage(Serializer &rez, 
                                                          int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(element_counts.size());
      for (typename std::map<T,size_t>::const_iterator it = 
            element_counts.begin(); it != element_counts.end(); it++)
      {
        rez.serialize(it->first);
        rez.serialize(it->second);
      }
    }

    //--------------------------------------------------------------------------
    template<typename T>
    void ConsensusMatchExchange<T>::unpack_collective_stage(
                                                 Deserializer &derez, int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_elements;
      derez.deserialize(num_elements);
      if (!participating)
      {
#ifdef DEBUG_LEGION
        assert(stage == -1);
#endif
        // Edge case at the end of a match
        // Just overwrite since our data comes back
        for (unsigned idx = 0; idx < num_elements; idx++)
        {
          T element;
          derez.deserialize(element);
          derez.deserialize(element_counts[element]);
        }
      }
      else
      {
        // Common case
        for (unsigned idx = 0; idx < num_elements; idx++)
        {
          T element;
          derez.deserialize(element);
          typename std::map<T,size_t>::iterator finder = 
            element_counts.find(element);
          if (finder != element_counts.end())
          {
            size_t count;
            derez.deserialize(count);
            finder->second += count;
          }
          else
            derez.deserialize(element_counts[element]);
        }
      }
    }

    //--------------------------------------------------------------------------
    template<typename T>
    bool ConsensusMatchExchange<T>::match_elements_async(const void *input,
                                                         size_t num_elements)
    //--------------------------------------------------------------------------
    {
      const T *inputs = static_cast<const T*>(input);
      for (unsigned idx = 0; idx < num_elements; idx++)
        element_counts[inputs[idx]] = 1;
#ifdef DEBUG_LEGION
      max_elements = num_elements;
#endif
      perform_collective_async(); 
      const RtEvent precondition = perform_collective_wait(false/*block*/);
      if (precondition.exists() && !precondition.has_triggered())
      {
        ConsensusMatchArgs args(this, context->get_unique_id());
        context->runtime->issue_runtime_meta_task(args,
            LG_LATENCY_DEFERRED_PRIORITY, precondition);
        return false;
      }
      else
      {
        complete_exchange();
        return true;
      }
    }

    //--------------------------------------------------------------------------
    template<typename T>
    void ConsensusMatchExchange<T>::complete_exchange(void)
    //--------------------------------------------------------------------------
    {
      const size_t total_shards = manager->total_shards; 
      size_t next_index = 0;
      for (typename std::map<T,size_t>::const_iterator it = 
            element_counts.begin(); it != element_counts.end(); it++)
      {
#ifdef DEBUG_LEGION
        assert(it->second <= total_shards);
#endif
        if (it->second < total_shards)
          continue;
#ifdef DEBUG_LEGION
        assert(next_index < max_elements);
#endif
        output[next_index++] = it->first;
      }
      // A little bit of help from the replicate context to complete the future
      context->help_complete_future(to_complete, &next_index, 
                        sizeof(next_index), false/*own*/);
      Runtime::trigger_event(NULL, to_trigger);
    }

    template class ConsensusMatchExchange<uint8_t>;
    template class ConsensusMatchExchange<uint16_t>;
    template class ConsensusMatchExchange<uint32_t>;
    template class ConsensusMatchExchange<uint64_t>;

    /////////////////////////////////////////////////////////////
    // VerifyReplicableExchange
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    VerifyReplicableExchange::VerifyReplicableExchange(
                             CollectiveIndexLocation loc, ReplicateContext *ctx)
      : AllGatherCollective<false>(loc, ctx)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    VerifyReplicableExchange::VerifyReplicableExchange(
                                            const VerifyReplicableExchange &rhs)
      : AllGatherCollective<false>(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    VerifyReplicableExchange::~VerifyReplicableExchange(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    VerifyReplicableExchange& VerifyReplicableExchange::operator=(
                                            const VerifyReplicableExchange &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void VerifyReplicableExchange::pack_collective_stage(Serializer &rez, 
                                                         int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(unique_hashes.size());
      for (ShardHashes::const_iterator it = unique_hashes.begin();
            it != unique_hashes.end(); it++)
      {
        rez.serialize(it->first.first);
        rez.serialize(it->first.second);
        rez.serialize(it->second);
      }
    }

    //--------------------------------------------------------------------------
    void VerifyReplicableExchange::unpack_collective_stage(Deserializer &derez,
                                                           int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_hashes;
      derez.deserialize(num_hashes);
      for (unsigned idx = 0; idx < num_hashes; idx++)
      {
        std::pair<uint64_t,uint64_t> key;
        derez.deserialize(key.first);
        derez.deserialize(key.second);
        ShardHashes::iterator finder = unique_hashes.find(key);
        if (finder != unique_hashes.end())
        {
          ShardID sid;
          derez.deserialize(sid);
          if (sid < finder->second)
            finder->second = sid;
        }
        else
          derez.deserialize(unique_hashes[key]);
      }
    }

    //--------------------------------------------------------------------------
    const VerifyReplicableExchange::ShardHashes& 
                      VerifyReplicableExchange::exchange(const uint64_t hash[2])
    //--------------------------------------------------------------------------
    {
      const std::pair<uint64_t,uint64_t> key(hash[0],hash[1]);
      unique_hashes[key] = local_shard;
      perform_collective_sync();
      return unique_hashes;
    }

    /////////////////////////////////////////////////////////////
    // OutputSizeExchange
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    OutputSizeExchange::OutputSizeExchange(ReplicateContext *ctx,
                                           CollectiveIndexLocation loc,
                                          std::map<unsigned,SizeMap> &all_sizes)
      : AllGatherCollective<false>(loc, ctx), all_output_sizes(all_sizes)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    OutputSizeExchange::OutputSizeExchange(const OutputSizeExchange &rhs)
      : AllGatherCollective<false>(rhs), all_output_sizes(rhs.all_output_sizes)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    OutputSizeExchange::~OutputSizeExchange(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    OutputSizeExchange& OutputSizeExchange::operator=(
                                                  const OutputSizeExchange &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void OutputSizeExchange::pack_collective_stage(Serializer &rez, int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize(all_output_sizes.size());
      for (std::map<unsigned,SizeMap>::iterator it = all_output_sizes.begin();
           it != all_output_sizes.end(); ++it)
      {
        rez.serialize(it->first);
        rez.serialize(it->second.size());
        for (SizeMap::iterator sit = it->second.begin();
             sit != it->second.end(); ++sit)
        {
          rez.serialize(sit->first);
          rez.serialize(sit->second);
        }
      }
    }

    //--------------------------------------------------------------------------
    void OutputSizeExchange::unpack_collective_stage(
                                                 Deserializer &derez, int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_sizes;
      derez.deserialize(num_sizes);
      if (num_sizes == 0) return;
      for (unsigned idx = 0; idx < num_sizes; ++idx)
      {
        unsigned out_idx;
        derez.deserialize(out_idx);
        SizeMap &sizes = all_output_sizes[out_idx];

        size_t num_entries;
        derez.deserialize(num_entries);
        for (unsigned eidx = 0; eidx < num_entries; eidx++)
        {
          DomainPoint point;
          derez.deserialize(point);
#ifdef DEBUG_LEGION
          DomainPoint size;
          derez.deserialize(size);
          assert(sizes.find(point) == sizes.end() ||
                 sizes.find(point)->second == size);
          sizes[point] = size;
#else
          derez.deserialize(sizes[point]);
#endif
        }
      }
    }

    //--------------------------------------------------------------------------
    RtEvent OutputSizeExchange::exchange_output_sizes(void)
    //--------------------------------------------------------------------------
    {
      perform_collective_async();
      return perform_collective_wait(false/*block*/);
    }

    /////////////////////////////////////////////////////////////
    // Index Attach Launch Space
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    IndexAttachLaunchSpace::IndexAttachLaunchSpace(ReplicateContext *ctx,
                                                   CollectiveIndexLocation loc)
      : AllGatherCollective<false>(loc, ctx), nonzeros(0)
    //--------------------------------------------------------------------------
    {
      sizes.resize(manager->total_shards, 0);
    }

    //--------------------------------------------------------------------------
    IndexAttachLaunchSpace::IndexAttachLaunchSpace(
                                              const IndexAttachLaunchSpace &rhs)
      : AllGatherCollective<false>(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    IndexAttachLaunchSpace::~IndexAttachLaunchSpace(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    IndexAttachLaunchSpace& IndexAttachLaunchSpace::operator=(
                                              const IndexAttachLaunchSpace &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void IndexAttachLaunchSpace::pack_collective_stage(Serializer &rez,
                                                       int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize(nonzeros);
      for (unsigned idx = 0; idx < sizes.size(); idx++)
      {
        size_t size = sizes[idx];
        if (size == 0)
          continue;
        rez.serialize(idx);
        rez.serialize(size);
      }
    }

    //--------------------------------------------------------------------------
    void IndexAttachLaunchSpace::unpack_collective_stage(Deserializer &derez,
                                                         int stage)
    //--------------------------------------------------------------------------
    {
      unsigned num_nonzeros;
      derez.deserialize(num_nonzeros);
      for (unsigned idx = 0; idx < num_nonzeros; idx++)
      {
        unsigned index;
        derez.deserialize(index);
        if (sizes[index] == 0)
          nonzeros++;
        derez.deserialize(sizes[index]);
      }
    }

    //--------------------------------------------------------------------------
    void IndexAttachLaunchSpace::exchange_counts(size_t count)
    //--------------------------------------------------------------------------
    {
      if (count > 0)
      {
#ifdef DEBUG_LEGION
        assert(local_shard < sizes.size());
#endif
        sizes[local_shard] = count;
        nonzeros++;
      }
      perform_collective_async();
    }

    //--------------------------------------------------------------------------
    IndexSpaceNode* IndexAttachLaunchSpace::get_launch_space(
                                                         Provenance *provenance)
    //--------------------------------------------------------------------------
    {
      perform_collective_wait();
      return context->compute_index_attach_launch_spaces(sizes, provenance);
    }

    /////////////////////////////////////////////////////////////
    // Index Attach Upper Bound
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    IndexAttachUpperBound::IndexAttachUpperBound(ReplicateContext *ctx,
                               CollectiveIndexLocation loc, RegionTreeForest *f)
      : AllGatherCollective<false>(loc, ctx), forest(f), node(NULL)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    IndexAttachUpperBound::IndexAttachUpperBound(
                                               const IndexAttachUpperBound &rhs)
      : AllGatherCollective<false>(rhs), forest(rhs.forest)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    IndexAttachUpperBound::~IndexAttachUpperBound(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    IndexAttachUpperBound& IndexAttachUpperBound::operator=(
                                               const IndexAttachUpperBound &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void IndexAttachUpperBound::pack_collective_stage(Serializer &rez,int stage)
    //--------------------------------------------------------------------------
    {
      if (node != NULL)
      {
        if (node->is_region())
        {
          rez.serialize<bool>(true); // is region
          rez.serialize(node->as_region_node()->handle);
        }
        else
        {
          rez.serialize<bool>(false); // is_region
          rez.serialize(node->as_partition_node()->handle);
        }
      }
      else
      {
        rez.serialize<bool>(true); // is region
        rez.serialize(LogicalRegion::NO_REGION);
      }
    }

    //--------------------------------------------------------------------------
    void IndexAttachUpperBound::unpack_collective_stage(Deserializer &derez,
                                                        int stage)
    //--------------------------------------------------------------------------
    {
      bool is_region;
      derez.deserialize(is_region);
      RegionTreeNode *next = NULL;
      if (is_region)
      {
        LogicalRegion handle;
        derez.deserialize(handle);
        if (!handle.exists())
          return;
        next = forest->get_node(handle);
      }
      else
      {
        LogicalPartition handle;
        derez.deserialize(handle);
        next = forest->get_node(handle);
      }
      if (node == NULL)
      {
        node = next;
        return;
      }
      if (next == node)
        return;
      // Bring them to the same depth
      unsigned next_depth = next->get_depth();
      unsigned node_depth = node->get_depth();
      while (next_depth < node_depth)
      {
#ifdef DEBUG_LEGION
        assert(node_depth > 0);
#endif
        node = node->get_parent();
        node_depth--;
      }
      while (node_depth < next_depth)
      {
#ifdef DEBUG_LEGION
        assert(next_depth > 0);
#endif
        next = next->get_parent();
        next_depth--;
      }
      while (node != next)
      {
        node = node->get_parent();
        next = next->get_parent();
      }
    }

    //--------------------------------------------------------------------------
    RegionTreeNode* IndexAttachUpperBound::find_upper_bound(RegionTreeNode *n)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(node == NULL);
#endif
      node = n;
      perform_collective_sync();
      return node;
    }

    /////////////////////////////////////////////////////////////
    // Index Attach Exchange
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    IndexAttachExchange::IndexAttachExchange(ReplicateContext *ctx,
                                             CollectiveIndexLocation loc)
      : AllGatherCollective<false>(loc, ctx)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    IndexAttachExchange::IndexAttachExchange(const IndexAttachExchange &rhs)
      : AllGatherCollective<false>(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    IndexAttachExchange::~IndexAttachExchange(void)
    //--------------------------------------------------------------------------
    {
    }
    
    //--------------------------------------------------------------------------
    IndexAttachExchange& IndexAttachExchange::operator=(
                                                 const IndexAttachExchange &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void IndexAttachExchange::pack_collective_stage(Serializer &rez,
                                                    int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(shard_spaces.size());
      for (std::map<ShardID,std::vector<IndexSpace> >::const_iterator sit =
            shard_spaces.begin(); sit != shard_spaces.end(); sit++)
      {
        rez.serialize(sit->first);
        rez.serialize<size_t>(sit->second.size());
        for (std::vector<IndexSpace>::const_iterator it =
              sit->second.begin(); it != sit->second.end(); it++)
          rez.serialize(*it);
      }
    }

    //--------------------------------------------------------------------------
    void IndexAttachExchange::unpack_collective_stage(Deserializer &derez,
                                                      int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_shards;
      derez.deserialize(num_shards);
      for (unsigned idx1 = 0; idx1 < num_shards; idx1++)
      {
        ShardID sid;
        derez.deserialize(sid);
        size_t num_spaces;
        derez.deserialize(num_spaces);
        std::vector<IndexSpace> &spaces = shard_spaces[sid];
        spaces.resize(num_spaces);
        for (unsigned idx2 = 0; idx2 < num_spaces; idx2++)
          derez.deserialize(spaces[idx2]);
      }
    }

    //--------------------------------------------------------------------------
    void IndexAttachExchange::exchange_spaces(std::vector<IndexSpace> &spaces)
    //--------------------------------------------------------------------------
    {
      shard_spaces[local_shard].swap(spaces);
      perform_collective_async();
    }

    //--------------------------------------------------------------------------
    size_t IndexAttachExchange::get_spaces(std::vector<IndexSpace> &spaces,
                                           unsigned &local_start)
    //--------------------------------------------------------------------------
    {
      perform_collective_wait();
      size_t total_spaces = 0;
      for (std::map<ShardID,std::vector<IndexSpace> >::const_iterator it =
            shard_spaces.begin(); it != shard_spaces.end(); it++)
        total_spaces += it->second.size();
      spaces.reserve(total_spaces);
      size_t local_size = 0;
      for (std::map<ShardID,std::vector<IndexSpace> >::const_iterator it =
            shard_spaces.begin(); it != shard_spaces.end(); it++)
      {
        if (it->first == local_shard)
        {
          local_start = spaces.size();
          local_size = it->second.size();
        }
        spaces.insert(spaces.end(), it->second.begin(), it->second.end());
      }
      return local_size;
    }

    /////////////////////////////////////////////////////////////
    // Shard Participants Exchange
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ShardParticipantsExchange::ShardParticipantsExchange(
                             ReplicateContext *ctx, CollectiveIndexLocation loc)
      : AllGatherCollective<false>(loc, ctx)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ShardParticipantsExchange::~ShardParticipantsExchange(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    void ShardParticipantsExchange::pack_collective_stage(Serializer &rez,
                                                          int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(participants.size());
      for (std::set<ShardID>::const_iterator it =
            participants.begin(); it != participants.end(); it++)
        rez.serialize(*it);
    }

    //--------------------------------------------------------------------------
    void ShardParticipantsExchange::unpack_collective_stage(Deserializer &derez,
                                                            int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_participants;
      derez.deserialize(num_participants);
      for (unsigned idx = 0; idx < num_participants; idx++)
      {
        ShardID shard;
        derez.deserialize(shard);
        participants.insert(shard);
      }
    }

    //--------------------------------------------------------------------------
    void ShardParticipantsExchange::exchange(bool participating)
    //--------------------------------------------------------------------------
    {
      if (participating)
        participants.insert(local_shard);
      perform_collective_async();
    }

    //--------------------------------------------------------------------------
    bool ShardParticipantsExchange::find_shard_participants(
                                                   std::vector<ShardID> &shards)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(shards.empty());
#endif
      perform_collective_wait();
      if (participants.size() < manager->total_shards)
      {
        shards.insert(shards.end(), participants.begin(), participants.end());
        return false;
      }
      else
        return true;
    }

    /////////////////////////////////////////////////////////////
    // Implicit Sharding Functor
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ImplicitShardingFunctor::ImplicitShardingFunctor(ReplicateContext *ctx,
                              CollectiveIndexLocation loc, ReplFutureMapImpl *m)
      : AllGatherCollective<false>(loc, ctx), ShardingFunctor(), map(m)
    //--------------------------------------------------------------------------
    {
      // Add this reference here, it will be removed after the exchange is
      // complete and that will break the cycle on deleting things since
      // technically the future map will have a reference to this as well
      map->add_base_resource_ref(PENDING_UNBOUND_REF);
    }

    //--------------------------------------------------------------------------
    ImplicitShardingFunctor::ImplicitShardingFunctor(
                                             const ImplicitShardingFunctor &rhs)
      : AllGatherCollective<false>(rhs), map(NULL)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    ImplicitShardingFunctor::~ImplicitShardingFunctor(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    ImplicitShardingFunctor& ImplicitShardingFunctor::operator=(
                                             const ImplicitShardingFunctor &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

    //--------------------------------------------------------------------------
    void ImplicitShardingFunctor::pack_collective_stage(Serializer &rez, 
                                                        int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(implicit_sharding.size());
      for (std::map<DomainPoint,ShardID>::const_iterator it =
            implicit_sharding.begin(); it != implicit_sharding.end(); it++)
      {
        rez.serialize(it->first);
        rez.serialize(it->second);
      }
    }

    //--------------------------------------------------------------------------
    void ImplicitShardingFunctor::unpack_collective_stage(Deserializer &derez,
                                                          int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_points;
      derez.deserialize(num_points);
      for (unsigned idx = 0; idx < num_points; idx++)
      {
        DomainPoint point;
        derez.deserialize(point);
        derez.deserialize(implicit_sharding[point]);
      }
    }

    //--------------------------------------------------------------------------
    ShardID ImplicitShardingFunctor::shard(const DomainPoint &point,
                                           const Domain &full_space,
                                           const size_t total_shards)
    //--------------------------------------------------------------------------
    {
      perform_collective_wait();
      std::map<DomainPoint,ShardID>::const_iterator finder =
        implicit_sharding.find(point);
#ifdef DEBUG_LEGION
      assert(finder != implicit_sharding.end());
#endif
      return finder->second;
    }

    //--------------------------------------------------------------------------
    RtEvent ImplicitShardingFunctor::post_complete_exchange(void)
    //--------------------------------------------------------------------------
    {
      // Remove our reference on the map
      if (map->remove_base_resource_ref(PENDING_UNBOUND_REF))
        delete map;
      return RtEvent::NO_RT_EVENT;
    }

    /////////////////////////////////////////////////////////////
    // Create Collective Fill View
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    CreateCollectiveFillView::CreateCollectiveFillView(ReplicateContext *ctx, 
                                  CollectiveID id, FillOp *op,
                                  DistributedID did, ShardID allocator_shard)
      : AllGatherCollective<false>(ctx, id), fill_op(op), fresh_did(0)
    //--------------------------------------------------------------------------
    {
      selected_views.insert(did);
      if (local_shard == allocator_shard)
        fresh_did = manager->runtime->get_available_distributed_id();
    }

    //--------------------------------------------------------------------------
    void CreateCollectiveFillView::pack_collective_stage(Serializer &rez,
                                                         int stage)
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(selected_views.size());
      for (std::set<DistributedID>::const_iterator it =
            selected_views.begin(); it != selected_views.end(); it++)
        rez.serialize(*it);
      rez.serialize(fresh_did);
    }

    //--------------------------------------------------------------------------
    void CreateCollectiveFillView::unpack_collective_stage(Deserializer &derez,
                                                           int stage)
    //--------------------------------------------------------------------------
    {
      size_t num_views;
      derez.deserialize(num_views);
      for (unsigned idx = 0; idx < num_views; idx++)
      {
        DistributedID did;
        derez.deserialize(did);
        selected_views.insert(did);
      }
      DistributedID did;
      derez.deserialize(did);
      if (did > 0)
      {
#ifdef DEBUG_LEGION
        assert((fresh_did == 0) || (fresh_did == did));
#endif
        fresh_did = did;
      }
    }

    //--------------------------------------------------------------------------
    RtEvent CreateCollectiveFillView::post_complete_exchange(void)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(fresh_did > 0);
      assert(!selected_views.empty());
#endif
      if ((selected_views.size() > 1) || ((*selected_views.begin()) == 0))
      {
        bool set_view = false;
        FillView *fill_view =
          manager->deduplicate_fill_view_creation(fresh_did, fill_op, set_view);
#ifdef DEBUG_LEGION
        assert(fill_view != NULL);
#endif
        return fill_op->register_fill_view_creation(fill_view, set_view);
      }
      else
        return RtEvent::NO_RT_EVENT;
    }

    /////////////////////////////////////////////////////////////
    // Collective View Rendezvous
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    CollectiveViewRendezvous::CollectiveViewRendezvous(CollectiveID id, 
        ReplicateContext *ctx, Operation *o, Finalizer *f, 
        const RendezvousKey &k, RegionTreeID tid)
      : GatherCollective(ctx, id, 
          ctx->shard_manager->find_collective_owner(tid)), 
        key(k), op(o), finalizer(f)
    //--------------------------------------------------------------------------
    {
#ifdef DEBUG_LEGION
      assert(op != NULL);
      assert(finalizer != NULL);
#endif
    }

    //--------------------------------------------------------------------------
    CollectiveViewRendezvous::~CollectiveViewRendezvous(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    void CollectiveViewRendezvous::pack_collective(Serializer &rez) const
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(rendezvous.size());
      for (std::map<LogicalRegion,CollectiveRendezvous>::const_iterator
            rit = rendezvous.begin(); rit != rendezvous.end(); rit++)
      {
        rez.serialize(rit->first);
        rez.serialize(rit->second.results.size());
        for (std::vector<std::pair<AddressSpaceID,RendezvousResult*> >::
              const_iterator it = rit->second.results.begin(); it !=
              rit->second.results.end(); it++)
        {
          rez.serialize(it->first);
          rez.serialize(it->second);
        }
        rez.serialize<size_t>(rit->second.groups.size());
        for (LegionMap<DistributedID,FieldMask>::const_iterator it =
              rit->second.groups.begin(); it != rit->second.groups.end(); it++)
        {
          rez.serialize(it->first);
          rez.serialize(it->second);
        }
        rez.serialize<size_t>(rit->second.counts.size());
        for (std::map<DistributedID,size_t>::const_iterator it =
              rit->second.counts.begin(); it != rit->second.counts.end(); it++)
        {
          rez.serialize(it->first);
          rez.serialize(it->second);
        }
      }
    }

    //--------------------------------------------------------------------------
    void CollectiveViewRendezvous::unpack_collective(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      size_t num_regions;
      derez.deserialize(num_regions);
      for (unsigned idx1 = 0; idx1 < num_regions; idx1++)
      {
        LogicalRegion region;
        derez.deserialize(region);
        std::map<LogicalRegion,CollectiveRendezvous>::iterator region_finder =
          rendezvous.find(region);
        if (region_finder != rendezvous.end())
        {
          // need to unpack out of place to do the merge
          size_t num_results;
          derez.deserialize(num_results);
          const unsigned offset = region_finder->second.results.size(); 
          region_finder->second.results.resize(offset + num_results);
          for (unsigned idx2 = 0; idx2 < num_results; idx2++)
          {
            derez.deserialize(
                region_finder->second.results[offset + idx2].first);
            derez.deserialize(
                region_finder->second.results[offset + idx2].second);
          }
          // unpack these and then do the merge
          LegionMap<DistributedID,FieldMask> groups;
          std::map<DistributedID,size_t> counts;
          size_t num_groups;
          derez.deserialize(num_groups);
          for (unsigned idx2 = 0; idx2 < num_groups; idx2++)
          {
            DistributedID did;
            derez.deserialize(did);
            derez.deserialize(groups[did]);
          }
          size_t num_counts;
          derez.deserialize(num_counts);
          for (unsigned idx2 = 0; idx2 < num_counts; idx2++)
          {
            DistributedID did;
            derez.deserialize(did);
            derez.deserialize(counts[did]);
          }
          // merge the groups and counts into the existing case
          for (LegionMap<DistributedID,FieldMask>::const_iterator it =
                groups.begin(); it != groups.end(); it++)
          {
            LegionMap<DistributedID,FieldMask>::iterator group_finder =
              region_finder->second.groups.find(it->first);
            std::map<DistributedID,size_t>::iterator count_finder =
              counts.find(it->first);
            if (group_finder != region_finder->second.groups.end())
            {
              if (group_finder->second == it->second)
              {
                std::map<DistributedID,size_t>::iterator local_finder =
                    region_finder->second.counts.find(it->first);
                if (count_finder != counts.end())
                {
                  if (local_finder == region_finder->second.counts.end())
                    region_finder->second.counts[it->first] = 
                      count_finder->second + 1;
                  else
                    local_finder->second += count_finder->second;
                }
                else
                {
                  if (local_finder == region_finder->second.counts.end())
                    region_finder->second.counts[it->first] = 2;
                  else
                    local_finder->second++;
                }
              }
              else
              {
                // If you ever hit this then heaven help you
                // The user has done something really out there and
                // is using the same instance with different sets of
                // fields for multiple point ops/tasks in the same 
                // index space operation. All the tricks we do to 
                // compute the collective arrivals are not going to
                // work in this case so the arrival counts will need 
                // to look something like:
                //   std::map<InstanceView*,LegionMap<size_t,FieldMask> >
                REPORT_LEGION_FATAL(
                    LEGION_FATAL_COLLECTIVE_PARTIAL_FIELD_OVERLAP,
                    "Operation %s (UID %lld) in context %s (UID %lld) "
                    "requested a very strange pattern for collective "
                    "instance rendezvous with different points asking to "
                    "rendezvous with different field sets on the same "
                    "physical instance. This isn't currently supported. "
                    "Please report your use case to the Legion "
                    "developer's mailing list.", op->get_logging_name(),
                    op->get_unique_op_id(), context->get_task_name(),
                    context->get_unique_id())
              }
            }
            else
            {
              // New instance, just insert it
              region_finder->second.groups.insert(*it);
              // See if we have any counts to move over
              if (count_finder != counts.end())
                region_finder->second.counts.insert(*count_finder);
            }
          }
        }
        else
        {
          // unpack in place since we know it doesn't exist yet
          CollectiveRendezvous &new_rendezvous = rendezvous[region];
          size_t num_results;
          derez.deserialize(num_results);
          new_rendezvous.results.resize(num_results);
          for (unsigned idx2 = 0; idx2 < num_results; idx2++)
          {
            derez.deserialize(new_rendezvous.results[idx2].first);
            derez.deserialize(new_rendezvous.results[idx2].second);
          }
          size_t num_groups;
          derez.deserialize(num_groups);
          for (unsigned idx2 = 0; idx2 < num_groups; idx2++)
          {
            DistributedID did;
            derez.deserialize(did);
            derez.deserialize(new_rendezvous.groups[did]);
          }
          size_t num_counts;
          derez.deserialize(num_counts);
          for (unsigned idx2 = 0; idx2 < num_counts; idx2++)
          {
            DistributedID did;
            derez.deserialize(did);
            derez.deserialize(new_rendezvous.counts[did]);
          }
        }
      }
    }

    //--------------------------------------------------------------------------
    RtEvent CollectiveViewRendezvous::post_gather(void)
    //--------------------------------------------------------------------------
    {
      if (local_shard == target)
        finalizer->finalize_collective_mapping(key, rendezvous);
      return RtEvent::NO_RT_EVENT;
    }

    //--------------------------------------------------------------------------
    void CollectiveViewRendezvous::perform_rendezvous(
                    std::map<LogicalRegion,CollectiveRendezvous> &to_rendezvous)
    //--------------------------------------------------------------------------
    {
      rendezvous.swap(to_rendezvous); 
      perform_collective_async(); 
    }

    /////////////////////////////////////////////////////////////
    // Concurrent Execution Validator
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    ConcurrentExecutionValidator::ConcurrentExecutionValidator(
        ReplIndexTask *own, CollectiveIndexLocation loc,
        ReplicateContext *ctx, ShardID target)
      : GatherCollective(loc, ctx, target), owner(own)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    void ConcurrentExecutionValidator::pack_collective(Serializer &rez) const
    //--------------------------------------------------------------------------
    {
      rez.serialize<size_t>(concurrent_processors.size());
      for (std::map<DomainPoint,Processor>::const_iterator it =
            concurrent_processors.begin(); it != 
            concurrent_processors.end(); it++)
      {
        rez.serialize(it->first);
        rez.serialize(it->second);
      }
    }

    //--------------------------------------------------------------------------
    void ConcurrentExecutionValidator::unpack_collective(Deserializer &derez)
    //--------------------------------------------------------------------------
    {
      size_t num_points;
      derez.deserialize(num_points);
      for (unsigned idx = 0; idx < num_points; idx++)
      {
        DomainPoint point;
        derez.deserialize(point);
#ifdef DEBUG_LEGION
        assert(concurrent_processors.find(point) == 
            concurrent_processors.end());
#endif
        derez.deserialize(concurrent_processors[point]);
      }
    }

    //--------------------------------------------------------------------------
    RtEvent ConcurrentExecutionValidator::post_gather(void)
    //--------------------------------------------------------------------------
    {
      std::map<Processor,DomainPoint> inverted;
      for (std::map<DomainPoint,Processor>::const_iterator it =
            concurrent_processors.begin(); it != 
            concurrent_processors.end(); it++)
      {
        std::map<Processor,DomainPoint>::const_iterator finder = 
          inverted.find(it->second);
        if (finder != inverted.end())
        {
          MapperManager *mapper = 
            owner->runtime->find_mapper(owner->current_proc, owner->map_id);
          // TODO: update this error message to name the bad points
          REPORT_LEGION_ERROR(ERROR_INVALID_MAPPER_OUTPUT,
              "Mapper %s performed illegal mapping of concurrent index "
              "space task %s (UID %lld) by mapping multiple points to "
              "the same processor " IDFMT ". All point tasks must be "
              "mapped to different processors for concurrent execution "
              "of index space tasks.", mapper->get_mapper_name(),
              owner->get_task_name(), owner->get_unique_id(), it->second.id)
        }
        inverted[it->second] = it->first;
      }
      return GatherCollective::post_gather();
    }

    //--------------------------------------------------------------------------
    void ConcurrentExecutionValidator::perform_validation(
                                    std::map<DomainPoint,Processor> &processors)
    //--------------------------------------------------------------------------
    {
      concurrent_processors.swap(processors);
      perform_collective_async();
    }

    /////////////////////////////////////////////////////////////
    // Slow Barrier
    /////////////////////////////////////////////////////////////

    //--------------------------------------------------------------------------
    SlowBarrier::SlowBarrier(ReplicateContext *ctx, CollectiveID id)
      : AllGatherCollective<false>(ctx, id)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    SlowBarrier::SlowBarrier(const SlowBarrier &rhs)
      : AllGatherCollective<false>(rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
    }

    //--------------------------------------------------------------------------
    SlowBarrier::~SlowBarrier(void)
    //--------------------------------------------------------------------------
    {
    }

    //--------------------------------------------------------------------------
    SlowBarrier& SlowBarrier::operator=(const SlowBarrier &rhs)
    //--------------------------------------------------------------------------
    {
      // should never be called
      assert(false);
      return *this;
    }

  }; // namespace Internal
}; // namespace Legion

