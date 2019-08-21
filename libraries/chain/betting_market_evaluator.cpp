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
#define DEFAULT_LOGGER "betting"
#include <fc/smart_ref_impl.hpp>

#include <graphene/chain/betting_market_evaluator.hpp>
#include <graphene/chain/betting_market_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

namespace graphene { namespace chain {

void_result betting_market_rules_create_evaluator::do_evaluate(const betting_market_rules_create_operation& op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_1000_TIME);
   FC_ASSERT(trx_state->_is_proposed_trx);
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type betting_market_rules_create_evaluator::do_apply(const betting_market_rules_create_operation& op)
{ try {
   const betting_market_rules_object& new_betting_market_rules =
     db().create<betting_market_rules_object>([&](betting_market_rules_object& betting_market_rules_obj) {
         betting_market_rules_obj.name = op.name;
         betting_market_rules_obj.description = op.description;
     });
   return new_betting_market_rules.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result betting_market_rules_update_evaluator::do_evaluate(const betting_market_rules_update_operation& op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_1000_TIME);
   FC_ASSERT(trx_state->_is_proposed_trx);
   _rules = &op.betting_market_rules_id(db());
   FC_ASSERT(op.new_name.valid() || op.new_description.valid(), "nothing to update");
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result betting_market_rules_update_evaluator::do_apply(const betting_market_rules_update_operation& op)
{ try {
   db().modify(*_rules, [&](betting_market_rules_object& betting_market_rules) {
      if (op.new_name.valid())
         betting_market_rules.name = *op.new_name;
      if (op.new_description.valid())
         betting_market_rules.description = *op.new_description;
   });
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result betting_market_group_create_evaluator::do_evaluate(const betting_market_group_create_operation& op)
{ try {
   database& d = db();
   FC_ASSERT(d.head_block_time() >= HARDFORK_1000_TIME);
   FC_ASSERT(trx_state->_is_proposed_trx);

   // the event_id in the operation can be a relative id.  If it is,
   // resolve it and verify that it is truly an event
   object_id_type resolved_event_id = op.event_id;
   if (is_relative(op.event_id))
      resolved_event_id = get_relative_id(op.event_id);

   FC_ASSERT(resolved_event_id.space() == event_id_type::space_id && 
             resolved_event_id.type() == event_id_type::type_id, 
             "event_id must refer to a event_id_type");
   _event_id = resolved_event_id;
   FC_ASSERT(d.find_object(_event_id), "Invalid event specified");

   FC_ASSERT(d.find_object(op.asset_id), "Invalid asset specified");

   // the rules_id in the operation can be a relative id.  If it is,
   // resolve it and verify that it is truly rules
   object_id_type resolved_rules_id = op.rules_id;
   if (is_relative(op.rules_id))
      resolved_rules_id = get_relative_id(op.rules_id);

   FC_ASSERT(resolved_rules_id.space() == betting_market_rules_id_type::space_id && 
             resolved_rules_id.type() == betting_market_rules_id_type::type_id, 
             "rules_id must refer to a betting_market_rules_id_type");
   _rules_id = resolved_rules_id;
   FC_ASSERT(d.find_object(_rules_id), "Invalid rules specified");
   return void_result();
} FC_CAPTURE_AND_RETHROW((op)) }

object_id_type betting_market_group_create_evaluator::do_apply(const betting_market_group_create_operation& op)
{ try {
   const betting_market_group_object& new_betting_market_group =
      db().create<betting_market_group_object>([&](betting_market_group_object& betting_market_group_obj) {
         betting_market_group_obj.event_id = _event_id;
         betting_market_group_obj.rules_id = _rules_id;
         betting_market_group_obj.description = op.description;
         betting_market_group_obj.asset_id = op.asset_id;
         betting_market_group_obj.never_in_play = op.never_in_play;
         betting_market_group_obj.delay_before_settling = op.delay_before_settling;
      });
   return new_betting_market_group.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result betting_market_group_update_evaluator::do_evaluate(const betting_market_group_update_operation& op)
{ try {
   database& d = db();
   FC_ASSERT(d.head_block_time() >= HARDFORK_1000_TIME);
   FC_ASSERT(trx_state->_is_proposed_trx);
   _betting_market_group = &op.betting_market_group_id(d);

   FC_ASSERT(op.new_description || op.new_rules_id || op.status, "nothing to change");

   if (op.new_rules_id)
   {
      // the rules_id in the operation can be a relative id.  If it is,
      // resolve it and verify that it is truly rules
      object_id_type resolved_rules_id = *op.new_rules_id;
      if (is_relative(*op.new_rules_id))
         resolved_rules_id = get_relative_id(*op.new_rules_id);

      FC_ASSERT(resolved_rules_id.space() == betting_market_rules_id_type::space_id &&
                resolved_rules_id.type() == betting_market_rules_id_type::type_id,
                "rules_id must refer to a betting_market_rules_id_type");
      _rules_id = resolved_rules_id;
      FC_ASSERT(d.find_object(_rules_id), "invalid rules specified");
   }

   if (op.status)
      FC_ASSERT(_betting_market_group->get_status() != *op.status, "status would not change the state of the betting market group");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result betting_market_group_update_evaluator::do_apply(const betting_market_group_update_operation& op)
{ try {
   database& d = db();
   d.modify(*_betting_market_group, [&](betting_market_group_object& betting_market_group) {
      if (op.new_description)
         betting_market_group.description = *op.new_description;
      if (op.new_rules_id)
         betting_market_group.rules_id = _rules_id;

      bool bets_were_delayed = betting_market_group.bets_are_delayed();
      if (op.status)
        betting_market_group.dispatch_new_status(d, *op.status);
        
      bool bets_are_delayed = betting_market_group.bets_are_delayed();

      // if we have transitioned from in-play to not-in-play-but-still-accepting-bets,
      // place all delayed bets now
      if (betting_market_group.bets_are_allowed() && 
          bets_were_delayed && !bets_are_delayed)
      {
         const auto& bet_odds_idx = d.get_index_type<bet_object_index>().indices().get<by_odds>();
         auto bet_iter = bet_odds_idx.begin();
         bool last = bet_iter == bet_odds_idx.end() || !bet_iter->end_of_delay;
         while (!last)
         {
            const bet_object& delayed_bet = *bet_iter;
            ++bet_iter;
            last = bet_iter == bet_odds_idx.end() || !bet_iter->end_of_delay;

            const betting_market_object& betting_market = delayed_bet.betting_market_id(d);
            if (betting_market.group_id == op.betting_market_group_id)
            {
               d.modify(delayed_bet, [](bet_object& bet_obj) {
                  // clear the end_of_delay,  which will re-sort the bet into its place in the book
                  bet_obj.end_of_delay.reset();
               });

               d.place_bet(delayed_bet);
            }
         }
      }
   });
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result betting_market_create_evaluator::do_evaluate(const betting_market_create_operation& op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_1000_TIME);
   FC_ASSERT(trx_state->_is_proposed_trx);

   // the betting_market_group_id in the operation can be a relative id.  If it is,
   // resolve it and verify that it is truly an betting_market_group
   object_id_type resolved_betting_market_group_id = op.group_id;
   if (is_relative(op.group_id))
      resolved_betting_market_group_id = get_relative_id(op.group_id);

   FC_ASSERT(resolved_betting_market_group_id.space() == betting_market_group_id_type::space_id && 
             resolved_betting_market_group_id.type() == betting_market_group_id_type::type_id, 
             "betting_market_group_id must refer to a betting_market_group_id_type");
   _group_id = resolved_betting_market_group_id;
   FC_ASSERT(db().find_object(_group_id), "Invalid betting_market_group specified");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type betting_market_create_evaluator::do_apply(const betting_market_create_operation& op)
{ try {
   const betting_market_object& new_betting_market =
      db().create<betting_market_object>([&](betting_market_object& betting_market_obj) {
         betting_market_obj.group_id = _group_id;
         betting_market_obj.description = op.description;
         betting_market_obj.payout_condition = op.payout_condition;
      });
   return new_betting_market.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result betting_market_update_evaluator::do_evaluate(const betting_market_update_operation& op)
{ try {
   database& d = db();
   FC_ASSERT(d.head_block_time() >= HARDFORK_1000_TIME);
   FC_ASSERT(trx_state->_is_proposed_trx);
   _betting_market = &op.betting_market_id(d);
   FC_ASSERT(op.new_group_id.valid() || op.new_description.valid() || op.new_payout_condition.valid(), "nothing to change");

   if (op.new_group_id.valid())
   {
      // the betting_market_group_id in the operation can be a relative id.  If it is,
      // resolve it and verify that it is truly an betting_market_group
      object_id_type resolved_betting_market_group_id = *op.new_group_id;
      if (is_relative(*op.new_group_id))
         resolved_betting_market_group_id = get_relative_id(*op.new_group_id);

      FC_ASSERT(resolved_betting_market_group_id.space() == betting_market_group_id_type::space_id &&
                resolved_betting_market_group_id.type() == betting_market_group_id_type::type_id,
                "betting_market_group_id must refer to a betting_market_group_id_type");
      _group_id = resolved_betting_market_group_id;
      FC_ASSERT(d.find_object(_group_id), "invalid betting_market_group specified");
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result betting_market_update_evaluator::do_apply(const betting_market_update_operation& op)
{ try {
   db().modify(*_betting_market, [&](betting_market_object& betting_market) {
      if (op.new_group_id.valid())
         betting_market.group_id = _group_id;
      if (op.new_payout_condition.valid())
         betting_market.payout_condition = *op.new_payout_condition;
      if (op.new_description.valid())
         betting_market.description = *op.new_description;
   });
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result bet_place_evaluator::do_evaluate(const bet_place_operation& op)
{ try {
   const database& d = db();
   FC_ASSERT(d.head_block_time() >= HARDFORK_1000_TIME);
   _betting_market = &op.betting_market_id(d);
   _betting_market_group = &_betting_market->group_id(d);

   FC_ASSERT( op.amount_to_bet.asset_id == _betting_market_group->asset_id,
              "Asset type bet does not match the market's asset type" );

   ddump((_betting_market_group->get_status()));
   FC_ASSERT( _betting_market_group->get_status() != betting_market_group_status::frozen, 
              "Unable to place bets while the market is frozen" );
   FC_ASSERT( _betting_market_group->get_status() != betting_market_group_status::closed, 
              "Unable to place bets while the market is closed" );
   FC_ASSERT( _betting_market_group->get_status() != betting_market_group_status::graded, 
              "Unable to place bets while the market is graded" );
   FC_ASSERT( _betting_market_group->get_status() != betting_market_group_status::re_grading, 
              "Unable to place bets while the market is re-grading" );
   FC_ASSERT( _betting_market_group->get_status() != betting_market_group_status::settled, 
              "Unable to place bets while the market is settled" );

   _asset = &_betting_market_group->asset_id(d);
   FC_ASSERT( is_authorized_asset( d, *fee_paying_account, *_asset ) );

   _current_params = &d.get_global_properties().parameters;

   // are their odds valid
   FC_ASSERT( op.backer_multiplier >= _current_params->min_bet_multiplier() &&
              op.backer_multiplier <= _current_params->max_bet_multiplier(),
              "Bet odds are outside the blockchain's limits" );
   if (!_current_params->permitted_betting_odds_increments().empty())
   {
      bet_multiplier_type allowed_increment;
      const auto iter = _current_params->permitted_betting_odds_increments().upper_bound(op.backer_multiplier);
      if (iter == _current_params->permitted_betting_odds_increments().end())
         allowed_increment = std::prev(_current_params->permitted_betting_odds_increments().end())->second;
      else
         allowed_increment = iter->second;
      FC_ASSERT(op.backer_multiplier % allowed_increment == 0, "Bet odds must be a multiple of ${allowed_increment}", ("allowed_increment", allowed_increment));
   }

   FC_ASSERT(op.amount_to_bet.amount > share_type(), "Cannot place a bet with zero amount");

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type bet_place_evaluator::do_apply(const bet_place_operation& op)
{ try {
   database& d = db();
   const bet_object& new_bet =
      d.create<bet_object>([&](bet_object& bet_obj) {
         bet_obj.bettor_id = op.bettor_id;
         bet_obj.betting_market_id = op.betting_market_id;
         bet_obj.amount_to_bet = op.amount_to_bet;
         bet_obj.backer_multiplier = op.backer_multiplier;
         bet_obj.back_or_lay = op.back_or_lay;
         if (_betting_market_group->bets_are_delayed()) {
            // the bet will be included in the block at time `head_block_time() + block_interval`, so make the delay relative 
            // to the time it's included in a block
            bet_obj.end_of_delay = d.head_block_time() + _current_params->block_interval + _current_params->live_betting_delay_time();
         }
      });

   bet_id_type new_bet_id = new_bet.id; // save the bet id here, new_bet may be deleted during place_bet()

   // place the bet, this may return guaranteed winnings
   ddump((_betting_market_group->bets_are_delayed())(_current_params->live_betting_delay_time()));
   if (!_betting_market_group->bets_are_delayed() || _current_params->live_betting_delay_time() <= 0)
      d.place_bet(new_bet);

   // now that their guaranteed winnings have been returned, check whether they have enough in their account to place the bet
   FC_ASSERT( d.get_balance( *fee_paying_account, *_asset ).amount  >= op.amount_to_bet.amount, "insufficient balance",
              ("balance", d.get_balance(*fee_paying_account, *_asset))("amount_to_bet", op.amount_to_bet.amount)  );

   // pay for it
   d.adjust_balance(fee_paying_account->id.as<account_id_type>(), -op.amount_to_bet);

   return new_bet_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result bet_cancel_evaluator::do_evaluate(const bet_cancel_operation& op)
{ try {
   const database& d = db();
   FC_ASSERT(d.head_block_time() >= HARDFORK_1000_TIME);
   _bet_to_cancel = &op.bet_to_cancel(d);
   FC_ASSERT( op.bettor_id == _bet_to_cancel->bettor_id, "You can only cancel your own bets" );

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result bet_cancel_evaluator::do_apply(const bet_cancel_operation& op)
{ try {
   db().cancel_bet(*_bet_to_cancel);
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result betting_market_group_resolve_evaluator::do_evaluate(const betting_market_group_resolve_operation& op)
{ try {
   database& d = db();
   FC_ASSERT(d.head_block_time() >= HARDFORK_1000_TIME);
   _betting_market_group = &op.betting_market_group_id(d);
   d.validate_betting_market_group_resolutions(*_betting_market_group, op.resolutions);
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result betting_market_group_resolve_evaluator::do_apply(const betting_market_group_resolve_operation& op)
{ try {
   db().resolve_betting_market_group(*_betting_market_group, op.resolutions);
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result betting_market_group_cancel_unmatched_bets_evaluator::do_evaluate(const betting_market_group_cancel_unmatched_bets_operation& op)
{ try {
   FC_ASSERT(db().head_block_time() >= HARDFORK_1000_TIME);
   _betting_market_group = &op.betting_market_group_id(db());
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result betting_market_group_cancel_unmatched_bets_evaluator::do_apply(const betting_market_group_cancel_unmatched_bets_operation& op)
{ try {
   db().cancel_all_unmatched_bets_on_betting_market_group(*_betting_market_group);
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }


} } // graphene::chain
