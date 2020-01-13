/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <graphene/chain/database.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/witness_schedule_object.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/son_info.hpp>

namespace graphene { namespace chain {

using boost::container::flat_set;

witness_id_type database::get_scheduled_witness( uint32_t slot_num )const
{
   witness_id_type wid;
   const global_property_object& gpo = get_global_properties();
   if (gpo.parameters.witness_schedule_algorithm == GRAPHENE_WITNESS_SHUFFLED_ALGORITHM)
   {
       const dynamic_global_property_object& dpo = get_dynamic_global_properties();
       const witness_schedule_object& wso = witness_schedule_id_type()(*this);
       uint64_t current_aslot = dpo.current_aslot + slot_num;
       return wso.current_shuffled_witnesses[ current_aslot % wso.current_shuffled_witnesses.size() ];
   }
   if (gpo.parameters.witness_schedule_algorithm == GRAPHENE_WITNESS_SCHEDULED_ALGORITHM &&
       slot_num != 0 )
   {
       const witness_schedule_object& wso = witness_schedule_id_type()(*this);
       // ask the near scheduler who goes in the given slot
       bool slot_is_near = wso.scheduler.get_slot(slot_num-1, wid);
       if(! slot_is_near)
       {
          // if the near scheduler doesn't know, we have to extend it to
          //   a far scheduler.
          // n.b. instantiating it is slow, but block gaps long enough to
          //   need it are likely pretty rare.

          witness_scheduler_rng far_rng(wso.rng_seed.begin(), GRAPHENE_FAR_SCHEDULE_CTR_IV);

          far_future_witness_scheduler far_scheduler =
             far_future_witness_scheduler(wso.scheduler, far_rng);
          if(!far_scheduler.get_slot(slot_num-1, wid))
          {
             // no scheduled witness -- somebody set up us the bomb
             // n.b. this code path is impossible, the present
             // implementation of far_future_witness_scheduler
             // returns true unconditionally
             assert( false );
          }
       }
   }
   return wid;
}

son_id_type database::get_scheduled_son( uint32_t slot_num )const
{
   son_id_type sid;
   const global_property_object& gpo = get_global_properties();
   if (gpo.parameters.witness_schedule_algorithm == GRAPHENE_WITNESS_SHUFFLED_ALGORITHM)
   {
       const dynamic_global_property_object& dpo = get_dynamic_global_properties();
       const son_schedule_object& sso = son_schedule_id_type()(*this);
       uint64_t current_aslot = dpo.current_aslot + slot_num;
       return sso.current_shuffled_sons[ current_aslot % sso.current_shuffled_sons.size() ];
   }
   if (gpo.parameters.witness_schedule_algorithm == GRAPHENE_WITNESS_SCHEDULED_ALGORITHM &&
       slot_num != 0 )
   {
       const son_schedule_object& sso = son_schedule_id_type()(*this);
       // ask the near scheduler who goes in the given slot
       bool slot_is_near = sso.scheduler.get_slot(slot_num-1, sid);
       if(! slot_is_near)
       {
          // if the near scheduler doesn't know, we have to extend it to
          //   a far scheduler.
          // n.b. instantiating it is slow, but block gaps long enough to
          //   need it are likely pretty rare.

          witness_scheduler_rng far_rng(sso.rng_seed.begin(), GRAPHENE_FAR_SCHEDULE_CTR_IV);

          far_future_son_scheduler far_scheduler =
             far_future_son_scheduler(sso.scheduler, far_rng);
          if(!far_scheduler.get_slot(slot_num-1, sid))
          {
             // no scheduled son -- somebody set up us the bomb
             // n.b. this code path is impossible, the present
             // implementation of far_future_son_scheduler
             // returns true unconditionally
             assert( false );
          }
       }
   }
   return sid;
}

fc::time_point_sec database::get_slot_time(uint32_t slot_num)const
{
   if( slot_num == 0 )
      return fc::time_point_sec();

   auto interval = block_interval();
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();

   if( head_block_num() == 0 )
   {
      // n.b. first block is at genesis_time plus one block interval
      fc::time_point_sec genesis_time = dpo.time;
      return genesis_time + slot_num * interval;
   }

   int64_t head_block_abs_slot = head_block_time().sec_since_epoch() / interval;
   fc::time_point_sec head_slot_time(head_block_abs_slot * interval);

   const global_property_object& gpo = get_global_properties();

   if( dpo.dynamic_flags & dynamic_global_property_object::maintenance_flag )
      slot_num += gpo.parameters.maintenance_skip_slots;

   // "slot 0" is head_slot_time
   // "slot 1" is head_slot_time,
   //   plus maint interval if head block is a maint block
   //   plus block interval if head block is not a maint block
   return head_slot_time + (slot_num * interval);
}

uint32_t database::get_slot_at_time(fc::time_point_sec when)const
{
   fc::time_point_sec first_slot_time = get_slot_time( 1 );
   //@ROL std::cout <<  "@get_slot_at_time " << when.to_iso_string() << " " << first_slot_time.to_iso_string() << "\n";
   if( when < first_slot_time )
      return 0;
   return (when - first_slot_time).to_seconds() / block_interval() + 1;
}

void database::update_witness_schedule()
{
   const witness_schedule_object& wso = witness_schedule_id_type()(*this);
   const global_property_object& gpo = get_global_properties();

   if( head_block_num() % gpo.active_witnesses.size() == 0 )
   {
      modify( wso, [&]( witness_schedule_object& _wso )
      {
         _wso.current_shuffled_witnesses.clear();
         _wso.current_shuffled_witnesses.reserve( gpo.active_witnesses.size() );

         for( const witness_id_type& w : gpo.active_witnesses )
            _wso.current_shuffled_witnesses.push_back( w );

         auto now_hi = uint64_t(head_block_time().sec_since_epoch()) << 32;
         for( uint32_t i = 0; i < _wso.current_shuffled_witnesses.size(); ++i )
         {
            /// High performance random generator
            /// http://xorshift.di.unimi.it/
            uint64_t k = now_hi + uint64_t(i)*2685821657736338717ULL;
            k ^= (k >> 12);
            k ^= (k << 25);
            k ^= (k >> 27);
            k *= 2685821657736338717ULL;

            uint32_t jmax = _wso.current_shuffled_witnesses.size() - i;
            uint32_t j = i + k%jmax;
            std::swap( _wso.current_shuffled_witnesses[i],
                       _wso.current_shuffled_witnesses[j] );
         }
      });
   }
}

void database::update_son_schedule()
{
   const son_schedule_object& sso = son_schedule_id_type()(*this);
   const global_property_object& gpo = get_global_properties();

   if( head_block_num() % gpo.active_sons.size() == 0 )
   {
      modify( sso, [&]( son_schedule_object& _sso )
      {
         _sso.current_shuffled_sons.clear();
         _sso.current_shuffled_sons.reserve( gpo.active_sons.size() );

         for( const son_info& w : gpo.active_sons )
            _sso.current_shuffled_sons.push_back( w.son_id );

         auto now_hi = uint64_t(head_block_time().sec_since_epoch()) << 32;
         for( uint32_t i = 0; i < _sso.current_shuffled_sons.size(); ++i )
         {
            /// High performance random generator
            /// http://xorshift.di.unimi.it/
            uint64_t k = now_hi + uint64_t(i)*2685821657736338717ULL;
            k ^= (k >> 12);
            k ^= (k << 25);
            k ^= (k >> 27);
            k *= 2685821657736338717ULL;

            uint32_t jmax = _sso.current_shuffled_sons.size() - i;
            uint32_t j = i + k%jmax;
            std::swap( _sso.current_shuffled_sons[i],
                       _sso.current_shuffled_sons[j] );
         }
      });
   }
}

vector<witness_id_type> database::get_near_witness_schedule()const
{
   const witness_schedule_object& wso = witness_schedule_id_type()(*this);

   vector<witness_id_type> result;
   result.reserve(wso.scheduler.size());
   uint32_t slot_num = 0;
   witness_id_type wid;

   while( wso.scheduler.get_slot(slot_num++, wid) )
      result.emplace_back(wid);

   return result;
}

void database::update_witness_schedule(const signed_block& next_block)
{
   auto start = fc::time_point::now();
   const global_property_object& gpo = get_global_properties();
   const witness_schedule_object& wso = get(witness_schedule_id_type());
   uint32_t schedule_needs_filled = gpo.active_witnesses.size();
   uint32_t schedule_slot = get_slot_at_time(next_block.timestamp);

   // We shouldn't be able to generate _pending_block with timestamp
   // in the past, and incoming blocks from the network with timestamp
   // in the past shouldn't be able to make it this far without
   // triggering FC_ASSERT elsewhere

   assert( schedule_slot > 0 );

   witness_id_type first_witness;
   bool slot_is_near = wso.scheduler.get_slot( schedule_slot-1, first_witness );

   witness_id_type wit;

   const dynamic_global_property_object& dpo = get_dynamic_global_properties();

   assert( dpo.random.data_size() == witness_scheduler_rng::seed_length );
   assert( witness_scheduler_rng::seed_length == wso.rng_seed.size() );

   modify(wso, [&](witness_schedule_object& _wso)
   {
      _wso.slots_since_genesis += schedule_slot;
      witness_scheduler_rng rng(wso.rng_seed.data, _wso.slots_since_genesis);

      _wso.scheduler._min_token_count = std::max(int(gpo.active_witnesses.size()) / 2, 1);

      if( slot_is_near )
      {
         uint32_t drain = schedule_slot;
         while( drain > 0 )
         {
            if( _wso.scheduler.size() == 0 )
               break;
            _wso.scheduler.consume_schedule();
            --drain;
         }
      }
      else
      {
         _wso.scheduler.reset_schedule( first_witness );
      }
      while( !_wso.scheduler.get_slot(schedule_needs_filled, wit) )
      {
         if( _wso.scheduler.produce_schedule(rng) & emit_turn )
            memcpy(_wso.rng_seed.begin(), dpo.random.data(), dpo.random.data_size());
      }
      _wso.last_scheduling_block = next_block.block_num();
      _wso.recent_slots_filled = (
           (_wso.recent_slots_filled << 1)
           + 1) << (schedule_slot - 1);
   });
   auto end = fc::time_point::now();
   static uint64_t total_time = 0;
   static uint64_t calls = 0;
   total_time += (end - start).count();
   if( ++calls % 1000 == 0 )
      idump( ( double(total_time/1000000.0)/calls) );
}

void database::update_son_schedule(const signed_block& next_block)
{
   auto start = fc::time_point::now();
   const global_property_object& gpo = get_global_properties();
   const son_schedule_object& sso = get(son_schedule_id_type());
   uint32_t schedule_needs_filled = gpo.active_sons.size();
   uint32_t schedule_slot = get_slot_at_time(next_block.timestamp);

   // We shouldn't be able to generate _pending_block with timestamp
   // in the past, and incoming blocks from the network with timestamp
   // in the past shouldn't be able to make it this far without
   // triggering FC_ASSERT elsewhere

   assert( schedule_slot > 0 );

   son_id_type first_son;
   bool slot_is_near = sso.scheduler.get_slot( schedule_slot-1, first_son );

   son_id_type son;

   const dynamic_global_property_object& dpo = get_dynamic_global_properties();

   assert( dpo.random.data_size() == witness_scheduler_rng::seed_length );
   assert( witness_scheduler_rng::seed_length == sso.rng_seed.size() );

   modify(sso, [&](son_schedule_object& _sso)
   {
      _sso.slots_since_genesis += schedule_slot;
      witness_scheduler_rng rng(sso.rng_seed.data, _sso.slots_since_genesis);

      _sso.scheduler._min_token_count = std::max(int(gpo.active_sons.size()) / 2, 1);

      if( slot_is_near )
      {
         uint32_t drain = schedule_slot;
         while( drain > 0 )
         {
            if( _sso.scheduler.size() == 0 )
               break;
            _sso.scheduler.consume_schedule();
            --drain;
         }
      }
      else
      {
         _sso.scheduler.reset_schedule( first_son );
      }
      while( !_sso.scheduler.get_slot(schedule_needs_filled, son) )
      {
         if( _sso.scheduler.produce_schedule(rng) & emit_turn )
            memcpy(_sso.rng_seed.begin(), dpo.random.data(), dpo.random.data_size());
      }
      _sso.last_scheduling_block = next_block.block_num();
      _sso.recent_slots_filled = (
           (_sso.recent_slots_filled << 1)
           + 1) << (schedule_slot - 1);
   });
   auto end = fc::time_point::now();
   static uint64_t total_time = 0;
   static uint64_t calls = 0;
   total_time += (end - start).count();
   if( ++calls % 1000 == 0 )
      idump( ( double(total_time/1000000.0)/calls) );
}

uint32_t database::update_witness_missed_blocks( const signed_block& b )
{
   uint32_t missed_blocks = get_slot_at_time( b.timestamp );
   FC_ASSERT( missed_blocks != 0, "Trying to push double-produced block onto current block?!" );
   missed_blocks--;
   const auto& witnesses = witness_schedule_id_type()(*this).current_shuffled_witnesses;
   if( missed_blocks < witnesses.size() )
      for( uint32_t i = 0; i < missed_blocks; ++i ) {
         const auto& witness_missed = get_scheduled_witness( i+1 )(*this);
         modify( witness_missed, []( witness_object& w ) {
            w.total_missed++;
         });
      }
   return missed_blocks;
}

uint32_t database::witness_participation_rate()const
{
    const global_property_object& gpo = get_global_properties();
    if (gpo.parameters.witness_schedule_algorithm == GRAPHENE_WITNESS_SHUFFLED_ALGORITHM)
    {
       const dynamic_global_property_object& dpo = get_dynamic_global_properties();
       return uint64_t(GRAPHENE_100_PERCENT) * dpo.recent_slots_filled.popcount() / 128;
    }
    if (gpo.parameters.witness_schedule_algorithm == GRAPHENE_WITNESS_SCHEDULED_ALGORITHM)
    {
       const witness_schedule_object& wso = get(witness_schedule_id_type());
       return uint64_t(GRAPHENE_100_PERCENT) * wso.recent_slots_filled.popcount() / 128;
    }
    return 0;
}

} }
