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

#include <graphene/chain/database.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/affiliate_payout.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/betting_market_object.hpp>
#include <graphene/chain/event_object.hpp>

#include <boost/range/iterator_range.hpp>
#include <boost/range/combine.hpp>
#include <boost/range/join.hpp>
#include <boost/tuple/tuple.hpp>

namespace graphene { namespace chain {

// helper functions to return the amount of exposure provided or required for a given bet
// if invert is false, returns the wins provided by the bet when it matches, 
// if invert is true, returns the wins required for the bet when it is unmatched 
std::vector<share_type> get_unmatched_bet_position_for_bet(database& db,
                                                           const bet_object& bet, 
                                                           share_type amount_bet, bool invert)
{
   const betting_market_object& betting_market = bet.betting_market_id(db);
   const betting_market_group_object& betting_market_group = betting_market.group_id(db);
   const auto& betting_market_index = db.get_index_type<betting_market_object_index>().indices().get<by_betting_market_group_id>();
   auto betting_markets_in_group = boost::make_iterator_range(betting_market_index.equal_range(betting_market_group.id));
   uint32_t number_of_betting_markets = boost::size(betting_markets_in_group);

   bet_type effective_bet_type;
   if (invert)
      effective_bet_type = bet.back_or_lay == bet_type::back ? bet_type::lay : bet_type::back;
   else
      effective_bet_type = bet.back_or_lay;

   std::vector<share_type> wins_required;
   if (number_of_betting_markets == 0)
      return wins_required;
#ifdef ENABLE_SIMPLE_CROSS_MARKET_MATCHING
   else if (number_of_betting_markets == 1 ||
            (number_of_betting_markets == 2 && 
             betting_market_group.resolution_constraint == betting_market_resolution_constraint::exactly_one_winner))
   {
      // in both cases, we have exactly one real betting market with two possible outcome
      wins_required.resize(2);
      wins_required[effective_bet_type == bet_type::back ? 0 : 1] = amount_bet;
      return wins_required;
   }
#endif
   else
   {
      uint32_t number_of_market_positions = number_of_betting_markets;
      if (betting_market_group.resolution_constraint != betting_market_resolution_constraint::exactly_one_winner)
         ++number_of_market_positions;
      wins_required.reserve(number_of_market_positions);
      for (const betting_market_object& betting_market_in_group : betting_markets_in_group)
         wins_required.push_back((effective_bet_type == bet_type::back) == (betting_market_in_group.id == bet.betting_market_id) ?
                                 amount_bet : 0);
      if (betting_market_group.resolution_constraint != betting_market_resolution_constraint::exactly_one_winner)
         wins_required.push_back(effective_bet_type == bet_type::back ? 0 : amount_bet);
      return wins_required;
   }
}

std::vector<share_type> get_wins_required_for_unmatched_bet(database& db, const bet_object& bet, share_type amount_bet) 
{
   return get_unmatched_bet_position_for_bet(db, bet, amount_bet, true);
}

std::vector<share_type> get_wins_provided_by_matched_bet(database& db, const bet_object& bet, share_type amount_matched) 
{
   return get_unmatched_bet_position_for_bet(db, bet, amount_matched, false);
}


void database::cancel_bet( const bet_object& bet, bool create_virtual_op )
{
   // Find out how much of the bettor's position this bet leans on, so we know how much to refund
   std::vector<share_type> wins_required_for_canceled_bet = get_wins_required_for_unmatched_bet(*this, bet, bet.amount_to_bet.amount);

   const betting_market_object& betting_market = bet.betting_market_id(*this);
   auto& position_index = get_index_type<betting_market_group_position_index>().indices().get<by_bettor_betting_market_group>();
   auto position_itr = position_index.find(boost::make_tuple(bet.bettor_id, betting_market.group_id));
   assert(position_itr != position_index.end()); // they must have had a position to account for this bet
   const betting_market_group_position_object& position = *position_itr;
   assert(position.market_positions.size() == wins_required_for_canceled_bet.size());
   assert(!position.market_positions.empty());

   asset amount_to_refund;
   modify(position, [&](betting_market_group_position_object& market_group_position) {
      // to compute the amount we're able to refund, we 
      // * subtract out the pay_required_by_unmatched_bets caused by this bet
      // * find the maximum amount of unused win position, which is the amount the
      //   bettor is guaranteed to get back if any of the markets in this group wins
      // * limit this amount by their cancel position
      for (unsigned i = 0; i < market_group_position.market_positions.size(); ++i)
         market_group_position.market_positions[i].pay_required_by_unmatched_bets -= wins_required_for_canceled_bet[i];
      share_type possible_refund = market_group_position.market_positions[0].pay_if_payout_condition - 
         market_group_position.market_positions[0].pay_required_by_unmatched_bets;
      for (unsigned i = 1; i < market_group_position.market_positions.size(); ++i)
         possible_refund = std::min(possible_refund, market_group_position.market_positions[i].pay_if_payout_condition - 
                                    market_group_position.market_positions[i].pay_required_by_unmatched_bets);
      amount_to_refund = asset(std::min(possible_refund, market_group_position.pay_if_canceled), 
                               bet.amount_to_bet.asset_id);

      // record that we refunded the bettor in their position
      market_group_position.pay_if_canceled -= amount_to_refund.amount;
      for (betting_market_position& market_position : market_group_position.market_positions)
         market_position.pay_if_payout_condition -= amount_to_refund.amount;
   });

   // we could rewrite this to only modify if not removed
   if (position.is_empty())
      remove(position);

   //TODO: update global statistics
   // refund to their balance
   if (amount_to_refund.amount > 0)
      adjust_balance(bet.bettor_id, amount_to_refund);

   // and create a virtual op recording that this happened
   if (create_virtual_op)
   {
      bet_canceled_operation bet_canceled_virtual_op(bet.bettor_id, bet.id,
                                                     bet.amount_to_bet);
      //fc_idump(fc::logger::get("betting"), (bet_canceled_virtual_op));
      push_applied_operation(std::move(bet_canceled_virtual_op));
   }

   // finally, drop the bet from the order books
   remove(bet);
}

// Cancel all bets (for all bettors) on the given betting market
void cancel_all_unmatched_bets_on_betting_market(database& db, const betting_market_object& betting_market)
{
   const auto& bet_odds_idx = db.get_index_type<bet_object_index>().indices().get<by_odds>();

   // first, cancel all bets on the active books
   auto book_itr = bet_odds_idx.lower_bound(std::make_tuple(betting_market.id));
   auto book_end = bet_odds_idx.upper_bound(std::make_tuple(betting_market.id));
   while (book_itr != book_end)
   {
      auto old_book_itr = book_itr;
      ++book_itr;
      db.cancel_bet(*old_book_itr, true);
   }

   // then, cancel any delayed bets on that market.  We don't have an index for
   // that, so walk through all delayed bets
   book_itr = bet_odds_idx.begin();
   while (book_itr != bet_odds_idx.end() &&
          book_itr->end_of_delay)
   {
      auto old_book_itr = book_itr;
      ++book_itr;
      if (old_book_itr->betting_market_id == betting_market.id)
         db.cancel_bet(*old_book_itr, true);
   }
}

// Cancel all bets (for all bettors) on the given betting market.  This loops through all betting markets in the group
// and cancels each bet in each market separately.  We do it this way because it allows us to generate virtual operations
// that inform the betters that their bets were canceled.
// If this wasn't desired, we could do this much more efficiently:
//  - walk through all market positions for this group, and zero out the pay_required_for_unmatched_bets 
//  - if possible, refund any funds that were backing those bets
//  - remove the bets from the order books
void database::cancel_all_unmatched_bets_on_betting_market_group(const betting_market_group_object& betting_market_group)
{
   auto& betting_market_index = get_index_type<betting_market_object_index>().indices().get<by_betting_market_group_id>();
   auto betting_market_itr = betting_market_index.lower_bound(betting_market_group.id);
   while (betting_market_itr != betting_market_index.end() &&  betting_market_itr->group_id == betting_market_group.id)
   {
      const betting_market_object& betting_market = *betting_market_itr;
      ++betting_market_itr;
      cancel_all_unmatched_bets_on_betting_market(*this, betting_market);
   }
}

void database::validate_betting_market_group_resolutions(const betting_market_group_object& betting_market_group,
                                                         const std::map<betting_market_id_type, betting_market_resolution_type>& resolutions)
{
   auto& betting_market_index = get_index_type<betting_market_object_index>().indices().get<by_betting_market_group_id>();
   auto betting_markets_in_group = boost::make_iterator_range(betting_market_index.equal_range(boost::make_tuple(betting_market_group.id)));

   // we must have one resolution for each betting market
   FC_ASSERT(resolutions.size() == boost::size(betting_markets_in_group), 
             "You must publish resolutions for all ${size} markets in the group, you published ${published}", 
             ("size", boost::size(betting_markets_in_group))
             ("published", resolutions.size()));

   // both are sorted by id, we can walk through both and verify that they match 
   unsigned number_of_wins = 0;
   unsigned number_of_cancels = 0;
   for (const auto& zipped : boost::combine(resolutions, betting_markets_in_group))
   {
      const auto& resolution = boost::get<0>(zipped);
      const auto& betting_market = boost::get<1>(zipped);
      FC_ASSERT(resolution.first == betting_market.id, "Missing resolution for betting market ${id}", ("id", betting_market.id));
      if (resolution.second == betting_market_resolution_type::cancel)
         ++number_of_cancels;
      else if (resolution.second == betting_market_resolution_type::win)
         ++number_of_wins;
      else 
         FC_ASSERT(resolution.second == betting_market_resolution_type::not_win);
   }

   if (number_of_cancels != 0)
      FC_ASSERT(number_of_cancels == resolutions.size(), "You must cancel all betting markets or none of the betting markets in the group");
   else if (betting_market_group.resolution_constraint == betting_market_resolution_constraint::exactly_one_winner)
      FC_ASSERT(number_of_wins == 1, "There must be exactly one winning market");
   else
      FC_ASSERT(number_of_wins == 0 || number_of_wins == 1, "There must be exactly zero or one winning market");
}

void database::resolve_betting_market_group(const betting_market_group_object& betting_market_group,
                                            const std::map<betting_market_id_type, betting_market_resolution_type>& resolutions)
{
   auto& betting_market_index = get_index_type<betting_market_object_index>().indices().get<by_betting_market_group_id>();
   auto betting_markets_in_group = boost::make_iterator_range(betting_market_index.equal_range(betting_market_group.id));

   bool group_was_canceled = resolutions.begin()->second == betting_market_resolution_type::cancel;

   if (group_was_canceled)
      modify(betting_market_group, [group_was_canceled,this](betting_market_group_object& betting_market_group_obj) {
         betting_market_group_obj.on_canceled_event(*this, false); // this cancels the betting markets
      });
   else {
      // TODO: this should be pushed into the bmg's on_graded_event

      // both are sorted by id, we can walk through both and verify that they match 
      for (const auto& zipped : boost::combine(resolutions, betting_markets_in_group))
      {
         const auto& resolution = boost::get<0>(zipped);
         const auto& betting_market = boost::get<1>(zipped);

         modify(betting_market, [this,&resolution](betting_market_object& betting_market_obj) {
            betting_market_obj.on_graded_event(*this, resolution.second);
         });
      }

      modify(betting_market_group, [group_was_canceled,this](betting_market_group_object& betting_market_group_obj) {
         betting_market_group_obj.on_graded_event(*this);
      });
   }
}

void database::settle_betting_market_group(const betting_market_group_object& betting_market_group)
{
   fc_ilog(fc::logger::get("betting"), "Settling betting market group ${id}", ("id", betting_market_group.id));
   // we pay the rake fee to the dividend distribution account for the core asset, go ahead
   // and look up that account now
   fc::optional<account_id_type> rake_account_id;
   const asset_object& core_asset_obj = asset_id_type(0)(*this);
   if (core_asset_obj.dividend_data_id)
   {
      const asset_dividend_data_object& core_asset_dividend_data_obj = (*core_asset_obj.dividend_data_id)(*this);
      rake_account_id = core_asset_dividend_data_obj.dividend_distribution_account;
   }
   uint16_t rake_fee_percentage = get_global_properties().parameters.betting_rake_fee_percentage();

   affiliate_payout_helper payout_helper(*this, betting_market_group);

   // figure out which position we're paying out -- it will either be the cancel position, or the
   // pay_if_payout_condition of one of the market positions.  The BMG position has an array of market
   // positions, store the index of the winning position here: (or leave invalid for cancel)
   fc::optional<uint32_t> winning_position_index;

   auto& betting_market_index = get_index_type<betting_market_object_index>().indices().get<by_betting_market_group_id>();
   auto betting_market_itr = betting_market_index.lower_bound(betting_market_group.id);

   // in the unlikely case that this betting market group has no betting markets, we can skip all of the 
   // payout code
   if (betting_market_itr != betting_market_index.end())
   {
      uint32_t current_betting_market_index = 0;
      uint32_t cancel_count = 0;
      while (betting_market_itr != betting_market_index.end() &&  betting_market_itr->group_id == betting_market_group.id)
      {
         assert(betting_market_itr->resolution); // the blockchain should not allow you to publish an invalid resolution
         fc_ilog(fc::logger::get("betting"), "Market: ${market_id}: ${resolution}", ("market_id", betting_market_itr->id)("resolution", betting_market_itr->resolution));
         FC_ASSERT(betting_market_itr->resolution, "Unexpected error settling betting market ${market_id}: no published resolution",
                   ("market_id", betting_market_itr->id));
         if (*betting_market_itr->resolution == betting_market_resolution_type::cancel)
         {
            // sanity check -- if one market is canceled, they all must be:
            assert(cancel_count == current_betting_market_index);
            FC_ASSERT(cancel_count == current_betting_market_index, 
                      "Unexpected error settling betting market group ${market_group_id}: all markets in a group must be canceled together",
                      ("market_group_id", betting_market_group.id));
            ++cancel_count;
         }
         else if (*betting_market_itr->resolution == betting_market_resolution_type::win)
         {
            // sanity check -- we can never have two markets win
            assert(!winning_position_index);
            FC_ASSERT(!winning_position_index,
                      "Unexpected error settling betting market group ${market_group_id}: there can only be one winner",
                      ("market_group_id", betting_market_group.id));
            winning_position_index = current_betting_market_index;
         }
         ++betting_market_itr;
         ++current_betting_market_index;
      }
      // now that we've walked over all markets, we should know that:
      // - all markets were canceled
      // - one market was the winner
      // - no markets won
      // in the last case, we will have one more market position than we have markets, which functions
      // as a "none of the above", so we'll make that the winner.
      if (!cancel_count)
      {
         if (betting_market_group.resolution_constraint == betting_market_resolution_constraint::exactly_one_winner)
         {
            assert(winning_position_index);
            FC_ASSERT(winning_position_index,
                      "Unexpected error settling betting market group ${market_group_id}: there must be one winner",
                      ("market_group_id", betting_market_group.id));
         }
         else if (!winning_position_index)
            winning_position_index = current_betting_market_index;
      }

      auto& position_index = get_index_type<betting_market_group_position_index>().indices().get<by_betting_market_group_bettor>();
      auto position_iter = position_index.lower_bound(betting_market_group.id);
      while (position_iter != position_index.end() && 
             position_iter->betting_market_group_id == betting_market_group.id)
      {
         const betting_market_group_position_object& position = *position_iter;
         ++position_iter;

         share_type net_profits;
         share_type total_payout;
         if (winning_position_index)
         {
            assert(winning_position_index < position.market_positions.size());
            FC_ASSERT(winning_position_index < position.market_positions.size(),
                      "Unexpected error paying bettor ${bettor_id} for market group ${market_group_id}: market positions array is the wrong size",
                      ("bettor_id", position.bettor_id)("market_group_id", betting_market_group.id));
            total_payout = position.market_positions[*winning_position_index].pay_if_payout_condition;
            net_profits = std::max<share_type>(total_payout - position.pay_if_canceled, 0);
         }
         else // canceled
            total_payout = position.pay_if_canceled;

         // pay the fees to the dividend-distribution account if net profit
         share_type rake_amount;
         if (net_profits.value > 0 && rake_account_id)
         {
            rake_amount = ((fc::uint128_t(net_profits.value) * rake_fee_percentage + GRAPHENE_100_PERCENT - 1) / GRAPHENE_100_PERCENT).to_uint64();
            share_type affiliates_share;
            if (rake_amount.value)
               affiliates_share = payout_helper.payout(position.bettor_id, rake_amount);
            assert(rake_amount.value >= affiliates_share.value);
            FC_ASSERT(rake_amount.value >= affiliates_share.value);
            if (rake_amount.value > affiliates_share.value)
               adjust_balance(*rake_account_id, asset(rake_amount - affiliates_share, betting_market_group.asset_id));
         }
         
         // pay winning - rake
         adjust_balance(position.bettor_id, asset(total_payout - rake_amount, betting_market_group.asset_id));

         push_applied_operation(betting_market_group_resolved_operation(position.bettor_id,
                                betting_market_group.id,
                                total_payout - rake_amount,
                                rake_amount));

         remove(position);
      }
   } // end if the group had at least one market

   // At this point, the betting market group will either be in the "graded" or "canceled" state,
   // if it was graded, mark it as settled.  if it's canceled, let it remain canceled.
   bool was_canceled = betting_market_group.get_status() == betting_market_group_status::canceled;

   if (!was_canceled)
      modify(betting_market_group, [&](betting_market_group_object& group) {
         group.on_settled_event(*this);
      });

   betting_market_itr = betting_market_index.lower_bound(betting_market_group.id);
   while (betting_market_itr != betting_market_index.end() &&  betting_market_itr->group_id == betting_market_group.id) {
      const betting_market_object& betting_market = *betting_market_itr;

      ++betting_market_itr;
      fc_dlog(fc::logger::get("betting"), "removing betting market ${id}", ("id", betting_market.id));
      remove(betting_market);
   }

   fc_dlog(fc::logger::get("betting"), "removing betting market group ${id}", ("id", betting_market_group.id));
   remove(betting_market_group);

   payout_helper.commit();
}

void database::remove_completed_events()
{
   const auto& event_index = get_index_type<event_object_index>().indices().get<by_event_status>();

   auto canceled_event_iter = event_index.lower_bound(event_status::canceled);
   while (canceled_event_iter != event_index.end() && canceled_event_iter->get_status() == event_status::canceled)
   {
      const event_object& event = *canceled_event_iter;
      ++canceled_event_iter;
      fc_dlog(fc::logger::get("betting"), "removing canceled event ${id}", ("id", event.id));
      remove(event);
   }

   auto settled_event_iter = event_index.lower_bound(event_status::settled);
   while (settled_event_iter != event_index.end() && settled_event_iter->get_status() == event_status::settled)
   {
      const event_object& event = *settled_event_iter;
      ++settled_event_iter;
      fc_dlog(fc::logger::get("betting"), "removing settled event ${id}", ("id", event.id));
      remove(event);
   }
}

// called twice when a bet is matched, once for the taker, once for the maker
bool bet_was_matched(database& db, const bet_object& bet,
                     const betting_market_object& betting_market,
                     share_type amount_bet, share_type amount_matched, 
                     bet_multiplier_type actual_multiplier,
                     bool refund_unmatched_portion)
{
   // get the wins provided by this match
   std::vector<share_type> additional_wins = get_wins_provided_by_matched_bet(db, bet, amount_bet + amount_matched);

   // We are removing all or part of this bet from the unmatched order book so we will need to adjust 
   // pay_required_for_unmatched_bets accordingly
   share_type amount_removed_from_unmatched_bets = refund_unmatched_portion ? bet.amount_to_bet.amount : amount_bet;
   std::vector<share_type> wins_required_for_unmatched_bet = get_wins_required_for_unmatched_bet(db, bet, amount_removed_from_unmatched_bets);
   assert(additional_wins.size() == wins_required_for_unmatched_bet.size());

   fc_dlog(fc::logger::get("betting"), "The newly-matched bets provides wins of ${wins_required_for_unmatched_bet}", (wins_required_for_unmatched_bet));

   auto& position_index = db.get_index_type<betting_market_group_position_index>().indices().get<by_bettor_betting_market_group>();
   auto position_itr = position_index.find(boost::make_tuple(bet.bettor_id, betting_market.group_id));
   assert(position_itr != position_index.end()); // they must have had a position to account for this bet
   const betting_market_group_position_object& position = *position_itr;
   assert(position.market_positions.size() == additional_wins.size());
   share_type not_cancel;
   db.modify(position, [&](betting_market_group_position_object& market_group_position) {
      for (unsigned i = 0; i < additional_wins.size(); ++i)
      {
         market_group_position.market_positions[i].pay_if_payout_condition += additional_wins[i];
         if (i == 0)
            not_cancel = market_group_position.market_positions[i].pay_if_payout_condition;
         else
            not_cancel = std::min(not_cancel, market_group_position.market_positions[i].pay_if_payout_condition);
      }
      for (unsigned i = 0; i < additional_wins.size(); ++i)
      {
         market_group_position.market_positions[i].pay_if_payout_condition -= not_cancel;
         market_group_position.market_positions[i].pay_required_by_unmatched_bets -= wins_required_for_unmatched_bet[i];
      }
   });

   // pay for the bet out of the not cancel
   assert(not_cancel >= amount_bet);
   not_cancel -= amount_bet;

   // save this, we'll need it later if we remove the bet
   bet_id_type bet_id = bet.id;
   account_id_type bettor_id = bet.bettor_id;
   asset_id_type bet_asset_id = bet.amount_to_bet.asset_id;


   // adjust the bet, removing it if it completely matched
   bool bet_was_removed = false;
   if (amount_bet == bet.amount_to_bet.amount)
   {
      db.remove(bet);
      bet_was_removed = true;
   }
   else
   {
      db.modify(bet, [&](bet_object& bet_obj) {
         bet_obj.amount_to_bet.amount -= amount_bet;
      });

      if (refund_unmatched_portion)
      {
         db.remove(bet);
         bet_was_removed = true;
      }
      else
         bet_was_removed = false;
   }

   // now, figure out how much of the remaining unmatched bets aren't covered by the position
   share_type max_shortfall;
   for (const betting_market_position& market_position : position.market_positions)
      max_shortfall = std::max(max_shortfall, market_position.pay_required_by_unmatched_bets - market_position.pay_if_payout_condition);
   assert(not_cancel >= max_shortfall); // else there was a bet placed that shouldn't have been allowed
   share_type possible_refund = not_cancel - max_shortfall;
   share_type actual_refund = std::min(possible_refund, position.pay_if_canceled);

   // do the refund
   not_cancel -= actual_refund;
   if (not_cancel > 0 || actual_refund > 0)
      db.modify(position, [&](betting_market_group_position_object& market_group_position) {
         for (betting_market_position& market_position : market_group_position.market_positions)
            market_position.pay_if_payout_condition += not_cancel;
         market_group_position.pay_if_canceled -= actual_refund;
      });

   db.adjust_balance(bettor_id, asset(actual_refund, bet_asset_id));
         
   // generate a virtual "match" op
   asset asset_amount_bet(amount_bet, bet_asset_id);
   bet_matched_operation bet_matched_virtual_op(bettor_id, bet_id, 
                                                asset_amount_bet,
                                                actual_multiplier,
                                                actual_refund);

   db.push_applied_operation(std::move(bet_matched_virtual_op));

   fc_dlog(fc::logger::get("betting"), "After matching bet, new market position is: ${position}", (position));
   if (position.is_empty())
      db.remove(position);

   return bet_was_removed;
}

/**
 *  Matches the two orders,
 *
 *  @return a bit field indicating which orders were filled (and thus removed)
 *
 *  0 - no bet was matched (this will never happen)
 *  1 - taker_bet was filled and removed from the books
 *  2 - maker_bet was filled and removed from the books
 *  3 - both were filled and removed from the books
 */
int match_bet(database& db, const bet_object& taker_bet, const bet_object& maker_bet, const betting_market_object& betting_market)
{
   //fc_ddump(fc::logger::get("betting"), (taker_bet)(maker_bet));
   assert(taker_bet.amount_to_bet.asset_id == maker_bet.amount_to_bet.asset_id);
   assert(taker_bet.amount_to_bet.amount > 0 && maker_bet.amount_to_bet.amount > 0);
   assert(taker_bet.back_or_lay == bet_type::back ? taker_bet.backer_multiplier <= maker_bet.backer_multiplier : 
                                                    taker_bet.backer_multiplier >= maker_bet.backer_multiplier);
   assert(taker_bet.back_or_lay != maker_bet.back_or_lay);

   int result = 0;
   //fc_idump(fc::logger::get("betting"), (taker_bet)(maker_bet));

   // using the maker's odds, figure out how much of the maker's bet we would match, rounding down
   // go ahead and get look up the ratio for the bet (a bet with odds 1.92 will have a ratio 25:23)
   share_type back_odds_ratio;
   share_type lay_odds_ratio;
   std::tie(back_odds_ratio, lay_odds_ratio) = maker_bet.get_ratio();

   // and make some shortcuts to get to the maker's and taker's side of the ratio
   const share_type& maker_odds_ratio = maker_bet.back_or_lay == bet_type::back ? back_odds_ratio : lay_odds_ratio;
   const share_type& taker_odds_ratio = maker_bet.back_or_lay == bet_type::back ? lay_odds_ratio : back_odds_ratio;
   // we need to figure out how much of the bet matches.  the smallest amount
   // that could match is one maker_odds_ratio to one taker_odds_ratio, 
   // but we can match any integer multiple of that ratio (called the 'factor' below), 
   // limited only by the bet amounts.
   //
   //fc_idump(fc::logger::get("betting"), (back_odds_ratio)(lay_odds_ratio));
   //fc_idump(fc::logger::get("betting"), (maker_odds_ratio)(taker_odds_ratio));

   // now figure out how much of the maker bet we'll consume.  We don't yet know whether the maker or taker
   // will be the limiting factor.
   share_type maximum_factor_taker_is_willing_to_pay = taker_bet.amount_to_bet.amount / taker_odds_ratio;

   share_type maximum_taker_factor = maximum_factor_taker_is_willing_to_pay;
#ifdef ENABLE_SIMPLE_CROSS_MARKET_MATCHING
   if (taker_bet.placed_as_back_or_lay == bet_type::lay) {
#else
   if (taker_bet.back_or_lay == bet_type::lay) {
#endif
      share_type maximum_factor_taker_is_willing_to_receive = taker_bet.get_exact_matching_amount() / maker_odds_ratio;
      fc_idump(fc::logger::get("betting"), (maximum_factor_taker_is_willing_to_pay));
      bool taker_was_limited_by_matching_amount = maximum_factor_taker_is_willing_to_receive < maximum_factor_taker_is_willing_to_pay;
      if (taker_was_limited_by_matching_amount)
         maximum_taker_factor = maximum_factor_taker_is_willing_to_receive;
   }
   fc_idump(fc::logger::get("betting"), (maximum_factor_taker_is_willing_to_pay)(maximum_taker_factor));

   share_type maximum_maker_factor = maker_bet.amount_to_bet.amount / maker_odds_ratio;
   share_type maximum_factor = std::min(maximum_taker_factor, maximum_maker_factor);
   share_type maker_amount_to_match = maximum_factor * maker_odds_ratio;
   share_type taker_amount_to_match = maximum_factor * taker_odds_ratio;
   fc_idump(fc::logger::get("betting"), (maker_amount_to_match)(taker_amount_to_match));

   // TODO: analyze whether maximum_maker_amount_to_match can ever be zero here 
   assert(maker_amount_to_match != 0);
   if (maker_amount_to_match == 0)
      return 0;

#ifndef NDEBUG
   assert(taker_amount_to_match <= taker_bet.amount_to_bet.amount);
   assert(taker_amount_to_match / taker_odds_ratio * taker_odds_ratio == taker_amount_to_match);
   {
      // verify we're getting the odds we expect
      fc::uint128_t payout_128 = maker_amount_to_match.value;
      payout_128 += taker_amount_to_match.value;
      payout_128 *= GRAPHENE_BETTING_ODDS_PRECISION;
      payout_128 /= maker_bet.back_or_lay == bet_type::back ? maker_amount_to_match.value : taker_amount_to_match.value;
      assert(payout_128.to_uint64() == maker_bet.backer_multiplier);
   }
#endif

   //fc_idump(fc::logger::get("betting"), (taker_amount_to_match)(maker_amount_to_match));

   // maker bets will always be an exact multiple of maker_odds_ratio, so they will either completely match or remain on the books
   bool maker_bet_will_completely_match = maker_amount_to_match == maker_bet.amount_to_bet.amount;

   if (maker_bet_will_completely_match && taker_amount_to_match != taker_bet.amount_to_bet.amount)
   {
      // then the taker bet will stay on the books.  If the taker odds != the maker odds, we will
      // need to refund the stake the taker was expecting to pay but didn't.
      // compute how much of the taker's bet should still be left on the books and how much
      // the taker should pay for the remaining amount; refund any amount that won't remain
      // on the books and isn't used to pay the bet we're currently matching.

      share_type takers_odds_back_odds_ratio;
      share_type takers_odds_lay_odds_ratio;
      std::tie(takers_odds_back_odds_ratio, takers_odds_lay_odds_ratio) = taker_bet.get_ratio();
      const share_type& takers_odds_taker_odds_ratio = taker_bet.back_or_lay == bet_type::back ? takers_odds_back_odds_ratio : takers_odds_lay_odds_ratio;
      const share_type& takers_odds_maker_odds_ratio = taker_bet.back_or_lay == bet_type::back ? takers_odds_lay_odds_ratio : takers_odds_back_odds_ratio;
      share_type taker_refund_amount;

      if (taker_bet.back_or_lay == bet_type::back)
      {
         // because we matched at the maker's odds and not the taker's odds, the remaining amount to match
         // may not be an even multiple of the taker's odds; round it down.
         share_type taker_remaining_factor = (taker_bet.amount_to_bet.amount - taker_amount_to_match) / takers_odds_taker_odds_ratio;
         share_type taker_remaining_bet_amount = taker_remaining_factor * takers_odds_taker_odds_ratio;
         taker_refund_amount = taker_bet.amount_to_bet.amount - taker_amount_to_match - taker_remaining_bet_amount;
         //idump((taker_remaining_factor)(taker_remaining_bet_amount)(taker_refund_amount));
      }
      else
      {
         // the taker bet is a lay bet.  because we matched at the maker's odds and not the taker's odds, 
         // there are two things we need to take into account.  First, we may have achieved more of a position
         // than we expected had we matched at our taker odds.  If so, we can refund the unused stake.
         // Second, the remaining amount to match may not be an even multiple of the taker's odds; round it down.
         share_type unrounded_taker_remaining_amount_to_match = taker_bet.get_exact_matching_amount() - maker_amount_to_match;
         //idump((unrounded_taker_remaining_amount_to_match));

         // because we matched at the maker's odds and not the taker's odds, the remaining amount to match
         // may not be an even multiple of the taker's odds; round it down.
         share_type taker_remaining_factor = unrounded_taker_remaining_amount_to_match / takers_odds_maker_odds_ratio;
         share_type taker_remaining_maker_amount_to_match = taker_remaining_factor * takers_odds_maker_odds_ratio;
         share_type taker_remaining_bet_amount = taker_remaining_factor * takers_odds_taker_odds_ratio;

         taker_refund_amount = taker_bet.amount_to_bet.amount - taker_amount_to_match - taker_remaining_bet_amount;
         //idump((taker_remaining_factor)(taker_remaining_maker_amount_to_match)(taker_remaining_bet_amount)(taker_refund_amount));
      }

      if (taker_refund_amount > share_type())
      {
         db.modify(taker_bet, [&taker_refund_amount](bet_object& taker_bet_object) {
                      taker_bet_object.amount_to_bet.amount -= taker_refund_amount;
                   });
         fc_dlog(fc::logger::get("betting"), "Refunding ${taker_refund_amount} to taker because we matched at the maker's odds of "
                 "${maker_odds} instead of the taker's odds ${taker_odds}",
                 ("taker_refund_amount", taker_refund_amount)
                 ("maker_odds", maker_bet.backer_multiplier)
                 ("taker_odds", taker_bet.backer_multiplier));
         fc_ddump(fc::logger::get("betting"), (taker_bet));

         // we don't really refund it here, we just adjust their position and this "refunded" amount will simply be added
         // to the amount returned to their balance by bet_was_matched
         auto& position_index = db.get_index_type<betting_market_group_position_index>().indices().get<by_bettor_betting_market_group>();
         auto position_itr = position_index.find(boost::make_tuple(taker_bet.bettor_id, betting_market.group_id));
         assert(position_itr != position_index.end()); // they must have had a position to account for this bet
         FC_ASSERT(position_itr != position_index.end(), "Unexpected error, bettor ${bettor_id} had a bet but no position", ("bettor_id", taker_bet.bettor_id));
         std::vector<share_type> wins_required_for_refunded_bet = get_wins_required_for_unmatched_bet(db, taker_bet, taker_refund_amount);
         assert(wins_required_for_refunded_bet.size() == position_itr->market_positions.size()); 
         FC_ASSERT(wins_required_for_refunded_bet.size() == position_itr->market_positions.size(), 
                   "Unexpected error, bettor's position didn't have the right amount of values");
         db.modify(*position_itr, [&](betting_market_group_position_object& market_group_position) {
            for (uint32_t i = 0; i < market_group_position.market_positions.size(); ++i)
            {
               betting_market_position& market_position = market_group_position.market_positions[i];
               assert(market_position.pay_required_by_unmatched_bets >= wins_required_for_refunded_bet[i]);
               FC_ASSERT(market_position.pay_required_by_unmatched_bets >= wins_required_for_refunded_bet[i], 
                         "Unexpected error, bettor had an unmatched bet which wasn't accounted for in their exposure (expected: >= ${expected}, actual: ${actual})",
                         ("expected", wins_required_for_refunded_bet[i])("actual", market_position.pay_required_by_unmatched_bets));
               market_position.pay_required_by_unmatched_bets -= wins_required_for_refunded_bet[i];
            }
         });

         // TODO: update global statistics
         bet_adjusted_operation bet_adjusted_op(taker_bet.bettor_id, taker_bet.id, 
                                                asset(taker_refund_amount, taker_bet.amount_to_bet.asset_id));
         // fc_idump(fc::logger::get("betting"), (bet_adjusted_op)(new_bet_object));
         db.push_applied_operation(std::move(bet_adjusted_op));
      }
   }

   // if the maker bet stays on the books, we need to make sure the taker bet is removed from the books (either it fills completely,
   // or any un-filled amount is canceled)
   result |= bet_was_matched(db, taker_bet, betting_market, taker_amount_to_match, maker_amount_to_match, maker_bet.backer_multiplier, !maker_bet_will_completely_match);
   result |= bet_was_matched(db, maker_bet, betting_market, maker_amount_to_match, taker_amount_to_match, maker_bet.backer_multiplier, false) << 1;

   assert(result != 0);
   return result;
}


// called from the bet_place_evaluator
bool database::place_bet(const bet_object& new_bet_object)
{
   // We allow users to place bets for any amount, but only amounts that are exact multiples of the odds
   // ratio can be matched.  
   // We'll want to refund any unmatchable amount if we determine that they can place the bet.
   share_type minimum_matchable_amount = new_bet_object.get_minimum_matchable_amount();
   share_type scale_factor = new_bet_object.amount_to_bet.amount / minimum_matchable_amount;
   share_type rounded_bet_amount = scale_factor * minimum_matchable_amount;

   if (rounded_bet_amount == share_type())
   {
      // the bet was too small to match at all, cancel the bet
      // don't go through the normal cancel() code, we've never modified the position
      // to account for it, so we don't need any of the calculations in cancel()
      push_applied_operation(bet_canceled_operation(new_bet_object.bettor_id, new_bet_object.id,
                                                    new_bet_object.amount_to_bet));
      remove(new_bet_object);
      return true;
   }

   // we might be able to place the bet, we need to see if they have enough balance or exposure
   // to cover the bet.
   const betting_market_object& betting_market = new_bet_object.betting_market_id(*this);
   const betting_market_group_object& betting_market_group = betting_market.group_id(*this);

   const auto& betting_market_index = get_index_type<betting_market_object_index>().indices().get<by_betting_market_group_id>();
   auto betting_markets_in_group = boost::make_iterator_range(betting_market_index.equal_range(betting_market_group.id));
   uint32_t number_of_betting_markets = boost::size(betting_markets_in_group);


   // the remaining amount is placed as a bet, so account for it as if we paid for it
   std::vector<share_type> position_required = get_wins_required_for_unmatched_bet(*this, new_bet_object, rounded_bet_amount);
   fc_ilog(fc::logger::get("betting"), "Wins required for unmatched bet: ${position_required}", (position_required));

   auto& position_index = get_index_type<betting_market_group_position_index>().indices().get<by_bettor_betting_market_group>();
   auto position_itr = position_index.find(boost::make_tuple(new_bet_object.bettor_id, betting_market.group_id));
   share_type amount_to_pay_from_balance;
   if (position_itr == position_index.end()) // if they have no position, they must pay the worst case
      for (const share_type& required : position_required)
         amount_to_pay_from_balance = std::max(amount_to_pay_from_balance, required);
   else // we might be able to lean on their current position
      for (unsigned i = 0; i < position_required.size(); ++i)
         amount_to_pay_from_balance = std::max(amount_to_pay_from_balance, 
                                               position_required[i] + position_itr->market_positions[i].pay_required_by_unmatched_bets - 
                                               position_itr->market_positions[i].pay_if_payout_condition);

   fc_idump(fc::logger::get("betting"), (amount_to_pay_from_balance));
   if (amount_to_pay_from_balance > 0)
      adjust_balance(new_bet_object.bettor_id, -amount_to_pay_from_balance);

   // still here? the bettor had enough funds to place the bet
   
   // if we had to round the bet amount, we should now generate a virtual op informing
   // the bettor that we rounded their bet down.
   if (rounded_bet_amount != new_bet_object.amount_to_bet.amount)
   {
      asset stake_returned = new_bet_object.amount_to_bet;
      stake_returned.amount -= rounded_bet_amount;

      modify(new_bet_object, [&rounded_bet_amount](bet_object& modified_bet_object) {
                modified_bet_object.amount_to_bet.amount = rounded_bet_amount;
             });

      // TODO: update global statistics
      bet_adjusted_operation bet_adjusted_op(new_bet_object.bettor_id, new_bet_object.id, 
                                             stake_returned);
      // fc_idump(fc::logger::get("betting"), (bet_adjusted_op)(new_bet_object));
      push_applied_operation(std::move(bet_adjusted_op));

      fc_dlog(fc::logger::get("betting"), "Refunded ${refund_amount} to round the bet down to something that can match exactly, new bet: ${new_bet}",
              ("refund_amount", stake_returned.amount)
              ("new_bet", new_bet_object));
   }

   // Now, adjust their position.  We need to add the position_required for this unmatched bet, and if they had to 
   // pay from their balance, also adjust their current pay & cancel positions
   const betting_market_group_position_object* position;
   if (position_itr == position_index.end())
   {
      position = &create<betting_market_group_position_object>([&](betting_market_group_position_object& market_group_position) {
         market_group_position.bettor_id = new_bet_object.bettor_id;
         market_group_position.betting_market_group_id = betting_market.group_id;
         market_group_position.market_positions.resize(position_required.size());
         for (unsigned i = 0; i < position_required.size(); ++i)
         {
            market_group_position.market_positions[i].pay_required_by_unmatched_bets = position_required[i];
            market_group_position.market_positions[i].pay_if_payout_condition = amount_to_pay_from_balance;
         }
         market_group_position.pay_if_canceled = amount_to_pay_from_balance;
      });
   } else {
      position = &*position_itr;
      modify(*position, [&](betting_market_group_position_object& market_group_position) {
         assert(market_group_position.bettor_id == new_bet_object.bettor_id);
         assert(market_group_position.betting_market_group_id == betting_mraket.group_id);
         assert(market_group_position.market_positions.size() == position_required.size());

         for (unsigned i = 0; i < position_required.size(); ++i)
         {
            market_group_position.market_positions[i].pay_required_by_unmatched_bets += position_required[i];
            market_group_position.market_positions[i].pay_if_payout_condition += amount_to_pay_from_balance;
         }
         market_group_position.pay_if_canceled += amount_to_pay_from_balance;
      });
   }
   return false;
}

bool database::try_to_match_bet(const bet_object& bet_object)
{
   fc_dlog(fc::logger::get("betting"), "Attempting to match bet ${bet_object}", (bet_object));

   const betting_market_object& betting_market = bet_object.betting_market_id(*this);
   const auto& bet_odds_idx = get_index_type<bet_object_index>().indices().get<by_odds>();

   bet_type bet_type_to_match = bet_object.back_or_lay == bet_type::back ? bet_type::lay : bet_type::back;
   auto book_itr = bet_odds_idx.lower_bound(std::make_tuple(bet_object.betting_market_id, bet_type_to_match));
   auto book_end = bet_odds_idx.upper_bound(std::make_tuple(bet_object.betting_market_id, bet_type_to_match, bet_object.backer_multiplier));

   // fc_ilog(fc::logger::get("betting"), "");
   // fc_ilog(fc::logger::get("betting"), "------------  order book ------------------");
   // for (auto itr = book_itr; itr != book_end; ++itr)
   //    fc_idump(fc::logger::get("betting"), (*itr));
   // fc_ilog(fc::logger::get("betting"), "------------  order book ------------------");
   int orders_matched_flags = 0;
   bool finished = false;
   while (!finished && book_itr != book_end)
   {
      auto old_book_itr = book_itr;
      ++book_itr;

      orders_matched_flags = match_bet(*this, bet_object, *old_book_itr, betting_market);

      // we continue if the maker bet was completely consumed AND the taker bet was not
      finished = orders_matched_flags != 2;
   }
   if (!(orders_matched_flags & 1))
      fc_ddump(fc::logger::get("betting"), (bet_object));


   // return true if the taker bet was completely consumed
   return (orders_matched_flags & 1) != 0;
}

} }
