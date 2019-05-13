/*
 * Copyright (c) 2018 Peerplays Blockchain Standards Association, and contributors.
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
#include <graphene/chain/sport_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/global_betting_statistics_object.hpp>
#include <graphene/chain/manager.hpp>

namespace graphene { namespace chain {

void_result event_create_evaluator::do_evaluate(const event_create_operation& op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_1000_TIME);

   //database& d = db();
   // the event_group_id in the operation can be a relative id.  If it is,
   // resolve it and verify that it is truly an event_group
   object_id_type resolved_event_group_id = op.event_group_id;
   if (is_relative(op.event_group_id))
      resolved_event_group_id = get_relative_id(op.event_group_id);

   FC_ASSERT(resolved_event_group_id.space() == event_group_id_type::space_id && 
             resolved_event_group_id.type() == event_group_id_type::type_id, 
             "event_group_id must refer to a event_group_id_type");
   event_group_id = resolved_event_group_id;

   if( db().head_block_time() < HARDFORK_MANAGER_TIME ) { // remove after HARDFORK_MANAGER_TIME
      FC_ASSERT( trx_state->_is_proposed_trx );
      FC_ASSERT( !op.extensions.value.new_manager && !op.extensions.value.fee_paying_account );
   }
   else {
      FC_ASSERT( is_manager( db(), event_group_id, op.fee_payer() ),
         "trx is not proposed and fee_payer is not the manager of this object" );
   }

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
         if( op.extensions.value.new_manager ) 
            event_obj.manager = *op.extensions.value.new_manager;
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
   FC_ASSERT(db().head_block_time() >= HARDFORK_1000_TIME);
   if( db().head_block_time() < HARDFORK_MANAGER_TIME ) { // remove after HARDFORK_MANAGER_TIME
      FC_ASSERT( trx_state->_is_proposed_trx );
      FC_ASSERT( !op.extensions.value.new_manager && !op.extensions.value.fee_paying_account );
      FC_ASSERT( op.new_event_group_id || op.new_name || op.new_season || 
         op.new_start_time || op.new_status, "nothing to change"); 
   }
   else {
      FC_ASSERT( is_manager( db(), op.event_id, op.fee_payer() ),
         "fee_payer is not the manager of this object" );
      FC_ASSERT( op.new_event_group_id || op.new_name || op.new_season || 
         op.new_start_time || op.new_status || op.extensions.value.new_manager,
         "nothing to change" );   
   }
   
   if (op.new_event_group_id)
   {
      object_id_type resolved_event_group_id = *op.new_event_group_id;
      if (is_relative(*op.new_event_group_id))
         resolved_event_group_id = get_relative_id(*op.new_event_group_id);

      FC_ASSERT(resolved_event_group_id.space() == event_group_id_type::space_id &&
                resolved_event_group_id.type() == event_group_id_type::type_id,
                "event_group_id must refer to a event_group_id_type");
      event_group_id = resolved_event_group_id;

      if( db().head_block_time() >= HARDFORK_MANAGER_TIME )
      {
         FC_ASSERT( is_manager( db(), event_group_id, *op.extensions.value.fee_paying_account ),
            "no manager permission for the new_event_group_id" );   
      }
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result event_update_evaluator::do_apply(const event_update_operation& op)
{ try {
   database& _db = db();
   _db.modify(_db.get(op.event_id),
              [&](event_object& eo) {
                 if( op.new_name )
                    eo.name = *op.new_name;
                 if( op.new_season )
                    eo.season = *op.new_season;
                 if( op.new_start_time )
                    eo.start_time = *op.new_start_time;
                 if( op.new_event_group_id )
                    eo.event_group_id = event_group_id;
                 if( op.new_status )
                    eo.dispatch_new_status(_db, *op.new_status);
                 if( op.extensions.value.new_manager )
                    eo.manager = *op.extensions.value.new_manager;
              });
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result event_update_status_evaluator::do_evaluate(const event_update_status_operation& op)
{ try {
   database& d = db();
   FC_ASSERT(d.head_block_time() >= HARDFORK_1000_TIME);
   if( d.head_block_time() < HARDFORK_MANAGER_TIME ) { // remove after HARDFORK_MANAGER_TIME
      FC_ASSERT( trx_state->_is_proposed_trx );
      FC_ASSERT( !op.extensions.value.fee_paying_account );
   }
   else {
      FC_ASSERT( is_manager( db(), op.event_id, op.fee_payer() ),
         "fee_payer is not the manager of this object" );
   }

   //check that the event to update exists
   _event_to_update = &op.event_id(d);

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result event_update_status_evaluator::do_apply(const event_update_status_operation& op)
{ try {
   database& d = db();

   d.modify( *_event_to_update, [&](event_object& event_obj) {
      if (_event_to_update->get_status() != op.status)
         event_obj.dispatch_new_status(d, op.status);
      event_obj.scores = op.scores;
   });

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain
