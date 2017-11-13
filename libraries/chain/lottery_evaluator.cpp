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
#include <graphene/chain/lottery_evaluator.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <functional>

#include <boost/algorithm/string/case_conv.hpp>

namespace graphene { namespace chain {

void_result ticket_purchase_evaluator::do_evaluate( const ticket_purchase_operation& op )
{ try {
   lottery = &op.lottery(db());
   FC_ASSERT( lottery->is_lottery() );

   asset_dynamic_data = &lottery->dynamic_asset_data_id(db());   
   FC_ASSERT( asset_dynamic_data->current_supply < lottery->options.max_supply );

   auto lottery_options = *lottery->lottery_options;
   FC_ASSERT( lottery_options.is_active );
   FC_ASSERT( lottery_options.ticket_price.asset_id == op.amount.asset_id );
   FC_ASSERT( (double)op.amount.amount.value / lottery_options.ticket_price.amount.value == (double)op.tickets_to_buy );
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type ticket_purchase_evaluator::do_apply( const ticket_purchase_operation& op )
{ try {
   db().adjust_balance( op.buyer, -op.amount );
   db().adjust_balance( op.buyer, asset( op.tickets_to_buy, lottery->id ) );
   db().modify( *asset_dynamic_data, [&]( asset_dynamic_data_object& data ){
      data.current_supply += op.tickets_to_buy;
   });
   db().modify( *lottery, [&]( asset_object& ao ){
      ao.lottery_options->balance += op.amount;
   });
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain
