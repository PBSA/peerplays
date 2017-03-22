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
#include <graphene/chain/betting_market_evaluator.hpp>
#include <graphene/chain/betting_market_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

namespace graphene { namespace chain {

void_result betting_market_group_create_evaluator::do_evaluate(const betting_market_group_create_operation& op)
{ try {
   FC_ASSERT(trx_state->_is_proposed_trx);

   // the event_id in the operation can be a relative id.  If it is,
   // resolve it and verify that it is truly an event
   object_id_type resolved_event_id = op.event_id;
   if (is_relative(op.event_id))
      resolved_event_id = get_relative_id(op.event_id);

   FC_ASSERT(resolved_event_id.space() == event_id_type::space_id && 
             resolved_event_id.type() == event_id_type::type_id, 
             "event_id must refer to a event_id_type");
   event_id = resolved_event_id;
   FC_ASSERT( db().find_object(event_id), "Invalid event specified" );

   // TODO: should we prevent creating multiple identical betting market groups for an event (same type & options)?
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type betting_market_group_create_evaluator::do_apply(const betting_market_group_create_operation& op)
{ try {
   const betting_market_group_object& new_betting_market_group =
     db().create<betting_market_group_object>( [&]( betting_market_group_object& betting_market_group_obj ) {
         betting_market_group_obj.event_id = event_id;
         betting_market_group_obj.options = op.options;
     });
   return new_betting_market_group.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result betting_market_create_evaluator::do_evaluate(const betting_market_create_operation& op)
{ try {
   FC_ASSERT(trx_state->_is_proposed_trx);

   // the betting_market_group_id in the operation can be a relative id.  If it is,
   // resolve it and verify that it is truly an betting_market_group
   object_id_type resolved_betting_market_group_id = op.group_id;
   if (is_relative(op.group_id))
      resolved_betting_market_group_id = get_relative_id(op.group_id);

   FC_ASSERT(resolved_betting_market_group_id.space() == betting_market_group_id_type::space_id && 
             resolved_betting_market_group_id.type() == betting_market_group_id_type::type_id, 
             "betting_market_group_id must refer to a betting_market_group_id_type");
   group_id = resolved_betting_market_group_id;
   FC_ASSERT( db().find_object(group_id), "Invalid betting_market_group specified" );

   // TODO: should we prevent creating multiple identical betting markets groups in a group (same asset and payout condition)?
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type betting_market_create_evaluator::do_apply(const betting_market_create_operation& op)
{ try {
   const betting_market_object& new_betting_market =
     db().create<betting_market_object>( [&]( betting_market_object& betting_market_obj ) {
         betting_market_obj.group_id = group_id;
         betting_market_obj.payout_condition = op.payout_condition;
         betting_market_obj.asset_id = op.asset_id;
     });
   return new_betting_market.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result bet_place_evaluator::do_evaluate(const bet_place_operation& op)
{ try {
   const database& d = db();

   _betting_market = &op.betting_market_id(d);
   _betting_market_group = &_betting_market->group_id(d);

   FC_ASSERT( op.amount_to_bet.asset_id == _betting_market->asset_id,
              "Asset type bet does not match the market's asset type" );

   _asset = &_betting_market->asset_id(d);
   FC_ASSERT( is_authorized_asset( d, *fee_paying_account, *_asset ) );

   const chain_parameters& current_params = d.get_global_properties().parameters;

   // are their odds valid
   FC_ASSERT( op.backer_multiplier >= current_params.min_bet_multiplier &&
              op.backer_multiplier <= current_params.max_bet_multiplier, 
              "Bet odds are outside the blockchain's limits" );
   if (!current_params.permitted_betting_odds_increments.empty())
   {
      bet_multiplier_type allowed_increment;
      const auto iter = current_params.permitted_betting_odds_increments.upper_bound(op.backer_multiplier);
      if (iter == current_params.permitted_betting_odds_increments.end())
         allowed_increment = std::prev(current_params.permitted_betting_odds_increments.end())->second;
      else
         allowed_increment = iter->second;
      FC_ASSERT(op.backer_multiplier % allowed_increment == 0, "Bet odds must be a multiple of ${allowed_increment}", ("allowed_increment", allowed_increment));
   }

   // verify they reserved enough to cover the percentage fee
   uint16_t percentage_fee = current_params.current_fees->get<bet_place_operation>().percentage_fee;
   fc::uint128_t minimum_percentage_fee_calculation = op.amount_to_bet.amount.value;
   minimum_percentage_fee_calculation *= percentage_fee;
   minimum_percentage_fee_calculation += GRAPHENE_100_PERCENT - 1; // round up
   minimum_percentage_fee_calculation /= GRAPHENE_100_PERCENT;
   share_type minimum_percentage_fee = minimum_percentage_fee_calculation.to_uint64();
   FC_ASSERT(op.amount_reserved_for_fees >= minimum_percentage_fee, "insufficient fees",
             ("fee_provided", op.amount_reserved_for_fees)("fee_required", minimum_percentage_fee));

   // do they have enough in their account to place the bet
   _stake_plus_fees = op.amount_to_bet.amount + op.amount_reserved_for_fees;
   FC_ASSERT( d.get_balance( *fee_paying_account, *_asset ).amount  >= _stake_plus_fees, "insufficient balance",
              ("balance", d.get_balance(*fee_paying_account, *_asset))("stake_plus_fees", _stake_plus_fees)  );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type bet_place_evaluator::do_apply(const bet_place_operation& op)
{ try {
   database& d = db();
   const bet_object& new_bet =
     d.create<bet_object>( [&]( bet_object& bet_obj ) {
         bet_obj.bettor_id = op.bettor_id;
         bet_obj.betting_market_id = op.betting_market_id;
         bet_obj.amount_to_bet = op.amount_to_bet;
         bet_obj.backer_multiplier = op.backer_multiplier;
         bet_obj.amount_reserved_for_fees = op.amount_reserved_for_fees;
         bet_obj.back_or_lay = op.back_or_lay;
     });

   d.adjust_balance(fee_paying_account->id, asset(-_stake_plus_fees, _betting_market->asset_id));

   bool bet_matched = d.place_bet(new_bet);

   return new_bet.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result bet_cancel_evaluator::do_evaluate(const bet_cancel_operation& op)
{ try {
   const database& d = db();
   _bet_to_cancel = &op.bet_to_cancel(d);
   FC_ASSERT( op.bettor_id == _bet_to_cancel->bettor_id, "You can only cancel your own bets" );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result bet_cancel_evaluator::do_apply(const bet_cancel_operation& op)
{ try {
   db().cancel_bet(*_bet_to_cancel);
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain
