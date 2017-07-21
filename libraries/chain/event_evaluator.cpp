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
#include <graphene/chain/event_evaluator.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/event_group_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/global_betting_statistics_object.hpp>

namespace graphene { namespace chain {

void_result event_create_evaluator::do_evaluate(const event_create_operation& op)
{ try {
   FC_ASSERT(trx_state->_is_proposed_trx);

   database& d = db();

   // the event_group_id in the operation can be a relative id.  If it is,
   // resolve it and verify that it is truly an event_group
   object_id_type resolved_event_group_id = op.event_group_id;
   if (is_relative(op.event_group_id))
      resolved_event_group_id = get_relative_id(op.event_group_id);

   FC_ASSERT(resolved_event_group_id.space() == event_group_id_type::space_id && 
             resolved_event_group_id.type() == event_group_id_type::type_id, 
             "event_group_id must refer to a event_group_id_type");
   event_group_id = resolved_event_group_id;
   //const event_group_object& event_group = event_group_id(d);

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type event_create_evaluator::do_apply(const event_create_operation& op)
{ try {
   database& d = db();
   const event_object& new_event =
      d.create<event_object>( [&]( event_object& event_obj ) {
         event_obj.name = op.name;
         event_obj.season = op.season;
         event_obj.start_time = op.start_time;
         event_obj.event_group_id = event_group_id;
     });
   //increment number of active events in global betting statistics object
   const global_betting_statistics_object& betting_statistics = global_betting_statistics_id_type()(d);
   d.modify( betting_statistics, [&](global_betting_statistics_object& bso) {
     bso.number_of_active_events += 1;
   });
   return new_event.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result event_update_evaluator::do_evaluate(const event_update_operation& op)
{ try {
   FC_ASSERT(trx_state->_is_proposed_trx);
   FC_ASSERT(op.new_event_group_id.valid() || op.new_name.valid() || op.new_season.valid() || op.new_start_time.valid(), "nothing to change");

   if (op.new_event_group_id.valid())
   {
       object_id_type resolved_event_group_id = *op.new_event_group_id;
       if (is_relative(*op.new_event_group_id))
          resolved_event_group_id = get_relative_id(*op.new_event_group_id);

       FC_ASSERT(resolved_event_group_id.space() == event_group_id_type::space_id &&
                 resolved_event_group_id.type() == event_group_id_type::type_id,
                 "event_group_id must refer to a event_group_id_type");
       event_group_id = resolved_event_group_id;
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result event_update_evaluator::do_apply(const event_update_operation& op)
{ try {
        database& _db = db();
        _db.modify(
           _db.get(op.event_id),
           [&]( event_object& eo )
           {
              if( op.new_name.valid() )
                  eo.name = *op.new_name;
              if( op.new_season.valid() )
                  eo.season = *op.new_season;
              if( op.new_start_time.valid() )
                  eo.start_time = *op.new_start_time;
              if( op.new_event_group_id.valid() )
                  eo.event_group_id = event_group_id;
           });
        return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result event_update_status_evaluator::do_evaluate(const event_update_status_operation& op)
{ try {
   FC_ASSERT(trx_state->_is_proposed_trx);

   database& d = db();
   //check that the event to update exists
   _event_to_update = &op.event_id(d);

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result event_update_status_evaluator::do_apply(const event_update_status_operation& op)
{ try {
   database& d = db();
   //if event is canceled, first cancel all associated betting markets
   if (op.status == event_status::canceled)
      d.cancel_all_betting_markets_for_event(*_event_to_update);
   //update event
   d.modify( *_event_to_update, [&]( event_object& event_obj) {
      event_obj.scores = op.scores;
      event_obj.status = op.status;
   });
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain
