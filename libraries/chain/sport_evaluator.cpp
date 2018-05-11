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
#include <graphene/chain/sport_evaluator.hpp>
#include <graphene/chain/sport_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

namespace graphene { namespace chain {

void_result sport_create_evaluator::do_evaluate(const sport_create_operation& op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_1000_TIME);
   FC_ASSERT(trx_state->_is_proposed_trx);

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type sport_create_evaluator::do_apply(const sport_create_operation& op)
{ try {
   const sport_object& new_sport =
     db().create<sport_object>( [&]( sport_object& sport_obj ) {
         sport_obj.name = op.name;
     });
   return new_sport.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }


void_result sport_update_evaluator::do_evaluate(const sport_update_operation& op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_1000_TIME);
   FC_ASSERT(trx_state->_is_proposed_trx);
   FC_ASSERT(op.new_name.valid());
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

 void_result sport_update_evaluator::do_apply(const sport_update_operation& op)
{ try {
   database& _db = db();
   _db.modify(
      _db.get(op.sport_id),
      [&]( sport_object& spo )
      {
         if( op.new_name.valid() )
             spo.name = *op.new_name;
      });
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

    
void_result sport_delete_evaluator::do_evaluate( const sport_delete_operation& o )
{}
    
void_result sport_delete_evaluator::do_apply( const sport_delete_operation& o )
{}

} } // graphene::chain
