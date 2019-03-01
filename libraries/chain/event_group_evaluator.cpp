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
#include <graphene/chain/event_group_evaluator.hpp>
#include <graphene/chain/event_group_object.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/sport_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/manager.hpp>

namespace graphene { namespace chain {

void_result event_group_create_evaluator::do_evaluate(const event_group_create_operation& op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_1000_TIME);

   // the sport id in the operation can be a relative id.  If it is,
   // resolve it and verify that it is truly a sport
   object_id_type resolved_id = op.sport_id;
   if (is_relative(op.sport_id))
      resolved_id = get_relative_id(op.sport_id);

   FC_ASSERT(resolved_id.space() == sport_id_type::space_id && 
             resolved_id.type() == sport_id_type::type_id, "sport_id must refer to a sport_id_type");
   sport_id = resolved_id;

   FC_ASSERT( db().find_object(sport_id), "Invalid sport specified" );

   if( db().head_block_time() < HARDFORK_MANAGER_TIME ) { // remove after HARDFORK_MANAGER_TIME
      FC_ASSERT( trx_state->_is_proposed_trx );
      FC_ASSERT( !op.extensions.value.new_manager && !op.extensions.value.fee_paying_account );
   }
   else {
      FC_ASSERT( is_manager( db(), sport_id, op.fee_payer() ),
         "fee_payer is not the manager of this object" );
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type event_group_create_evaluator::do_apply(const event_group_create_operation& op)
{ try {
   const event_group_object& new_event_group =
     db().create<event_group_object>( [&]( event_group_object& event_group_obj ) {
         event_group_obj.name = op.name;
         event_group_obj.sport_id = sport_id;
         if( op.extensions.value.new_manager ) 
            event_group_obj.manager = *op.extensions.value.new_manager;
     });
   return new_event_group.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result event_group_update_evaluator::do_evaluate(const event_group_update_operation& op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_1000_TIME);
   if( db().head_block_time() < HARDFORK_MANAGER_TIME ) { // remove after HARDFORK_MANAGER_TIME
      FC_ASSERT( trx_state->_is_proposed_trx );
      FC_ASSERT( !op.extensions.value.new_manager && !op.extensions.value.fee_paying_account );
      FC_ASSERT( op.new_sport_id || op.new_name, "nothing to change" );
   }
   else {
      FC_ASSERT( is_manager( db(), op.event_group_id, op.fee_payer() ),
         "fee_payer is not the manager of this object" );
      FC_ASSERT( op.new_sport_id || op.new_name || op.extensions.value.new_manager, "nothing to change" );
   }
      
   if( op.new_sport_id )
   {
       object_id_type resolved_id = *op.new_sport_id;
       if (is_relative(*op.new_sport_id))
          resolved_id = get_relative_id(*op.new_sport_id);

       FC_ASSERT(resolved_id.space() == sport_id_type::space_id &&
                 resolved_id.type() == sport_id_type::type_id, "sport_id must refer to a sport_id_type");
       sport_id = resolved_id;

       FC_ASSERT( db().find_object(sport_id), "invalid sport specified" );
       
       if( db().head_block_time() >= HARDFORK_MANAGER_TIME )
       {
          FC_ASSERT( is_manager( db(), sport_id, op.fee_payer() ),
             "no manager permission for the new_sport_id" );
       }
   }
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result event_group_update_evaluator::do_apply(const event_group_update_operation& op)
{ try {
    database& _db = db();
    _db.modify(
       _db.get(op.event_group_id),
       [&]( event_group_object& ego )
       {
          if( op.new_name.valid() )
              ego.name = *op.new_name;
          if( op.new_sport_id.valid() )
              ego.sport_id = sport_id;
          if( op.extensions.value.new_manager )
              ego.manager = *op.extensions.value.new_manager;
       });
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

    
void_result event_group_delete_evaluator::do_evaluate(const event_group_delete_operation& op)
{ try {
    FC_ASSERT(db().head_block_time() >= HARDFORK_1001_TIME);
    if( db().head_block_time() < HARDFORK_MANAGER_TIME ) { // remove after HARDFORK_MANAGER_TIME
      FC_ASSERT( trx_state->_is_proposed_trx );
      FC_ASSERT( !op.extensions.value.fee_paying_account );
    }
    else {
      FC_ASSERT( is_manager( db(), op.event_group_id, op.fee_payer() ),
        "fee_payer is not the manager of this object" );
    }

    //check for event group existence
    _event_group = &op.event_group_id(db());
    
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result event_group_delete_evaluator::do_apply(const event_group_delete_operation& op)
{ try {
    database& _db = db();
    
    _event_group->cancel_events(_db);
    _db.remove(*_event_group);
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }
    
} } // graphene::chain
