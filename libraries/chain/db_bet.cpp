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

void database::cancel_bet( const bet_object& bet, bool create_virtual_op )
{
   asset amount_to_refund = bet.amount_to_bet;
   //TODO: update global statistics
   adjust_balance(bet.bettor_id, amount_to_refund);
   if (create_virtual_op)
   {
      bet_canceled_operation bet_canceled_virtual_op(bet.bettor_id, bet.id,
                                                     bet.amount_to_bet);
      //fc_idump(fc::logger::get("betting"), (bet_canceled_virtual_op));
      push_applied_operation(std::move(bet_canceled_virtual_op));
   }
   remove(bet);
}

void database::cancel_all_unmatched_bets_on_betting_market(const betting_market_object& betting_market)
{
   const auto& bet_odds_idx = get_index_type<bet_object_index>().indices().get<by_odds>();

   // first, cancel all bets on the active books
   auto book_itr = bet_odds_idx.lower_bound(std::make_tuple(betting_market.id));
   auto book_end = bet_odds_idx.upper_bound(std::make_tuple(betting_market.id));
   while (book_itr != book_end)
   {
      auto old_book_itr = book_itr;
      ++book_itr;
      cancel_bet(*old_book_itr, true);
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
         cancel_bet(*old_book_itr, true);
   }
}

void database::validate_betting_market_group_resolutions(const betting_market_group_object& betting_market_group,
                                                         const std::map<betting_market_id_type, betting_market_resolution_type>& resolutions)
{
   auto& betting_market_index = get_index_type<betting_market_object_index>().indices().get<by_betting_market_group_id>();
   auto betting_markets_in_group = boost::make_iterator_range(betting_market_index.equal_range(betting_market_group.id));

   // we must have one resolution for each betting market
   FC_ASSERT(resolutions.size() == boost::size(betting_markets_in_group), 
             "You must publish resolutions for all ${size} markets in the group, you published ${published}", ("size", boost::size(betting_markets_in_group))("published", resolutions.size()));

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
   else
      FC_ASSERT(number_of_wins == 1, "There must be exactly one winning market");
}

void database::cancel_all_unmatched_bets_on_betting_market_group(const betting_market_group_object& betting_market_group)
{
   auto& betting_market_index = get_index_type<betting_market_object_index>().indices().get<by_betting_market_group_id>();
   auto betting_market_itr = betting_market_index.lower_bound(betting_market_group.id);
   while (betting_market_itr != betting_market_index.end() &&  betting_market_itr->group_id == betting_market_group.id)
   {
      const betting_market_object& betting_market = *betting_market_itr;
      ++betting_market_itr;
      cancel_all_unmatched_bets_on_betting_market(betting_market);
   }

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

   affiliate_payout_helper payout_helper( *this, betting_market_group );

   // collect the resolutions of all markets in the BMG: they were previously published and
   // stored in the individual betting markets
   std::map<betting_market_id_type, betting_market_resolution_type> resolutions_by_market_id;

   // collecting bettors and their positions
   std::map<account_id_type, std::vector<const betting_market_position_object*> > bettor_positions_map;

   auto& betting_market_index = get_index_type<betting_market_object_index>().indices().get<by_betting_market_group_id>();
   // [ROL] it seems to be my mistake - wrong index used
   //auto& position_index = get_index_type<betting_market_position_index>().indices().get<by_bettor_betting_market>();
   auto& position_index = get_index_type<betting_market_position_index>().indices().get<by_betting_market_bettor>();
   auto betting_market_itr = betting_market_index.lower_bound(betting_market_group.id);
   while (betting_market_itr != betting_market_index.end() &&  betting_market_itr->group_id == betting_market_group.id)
   {
      const betting_market_object& betting_market = *betting_market_itr;
      FC_ASSERT(betting_market_itr->resolution, "Unexpected error settling betting market ${market_id}: no published resolution",
                ("market_id", betting_market_itr->id));
      resolutions_by_market_id.emplace(betting_market.id, *betting_market_itr->resolution);

      ++betting_market_itr;
      cancel_all_unmatched_bets_on_betting_market(betting_market);

      auto position_itr = position_index.lower_bound(betting_market.id);

      while (position_itr != position_index.end() && position_itr->betting_market_id == betting_market.id)
      {
         const betting_market_position_object& position = *position_itr;
         ++position_itr;

         bettor_positions_map[position.bettor_id].push_back(&position);
      }
   }

   // walking through bettors' positions and collecting winings and fees respecting asset_id
   for (const auto& bettor_positions_pair: bettor_positions_map)
   {
      uint16_t rake_fee_percentage = get_global_properties().parameters.betting_rake_fee_percentage();
      share_type net_profits;
      share_type payout_amounts;
      account_id_type bettor_id = bettor_positions_pair.first;
      const std::vector<const betting_market_position_object*>& bettor_positions = bettor_positions_pair.second;

      for (const betting_market_position_object* position : bettor_positions)
      {
         betting_market_resolution_type resolution;
         try
         {
            resolution = resolutions_by_market_id.at(position->betting_market_id);
         }
         catch (std::out_of_range&)
         {
            FC_THROW_EXCEPTION(fc::key_not_found_exception, "Unexpected betting market ID, shouldn't happen");
         }

         ///if (cancel)
         ///   resolution = betting_market_resolution_type::cancel;
         ///else
         ///{
         ///   // checked in evaluator, should never happen, see above
         ///   assert(resolutions.count(position->betting_market_id));
         ///   resolution = resolutions.at(position->betting_market_id);
         ///}


         switch (resolution)
         {
            case betting_market_resolution_type::win:
               {
                  share_type total_payout = position->pay_if_payout_condition + position->pay_if_not_canceled;
                  payout_amounts += total_payout;
                  net_profits += total_payout - position->pay_if_canceled;
                  break;
               }
            case betting_market_resolution_type::not_win:
               {
                  share_type total_payout = position->pay_if_not_payout_condition + position->pay_if_not_canceled;
                  payout_amounts += total_payout;
                  net_profits += total_payout - position->pay_if_canceled;
                  break;
               }
            case betting_market_resolution_type::cancel:
               payout_amounts += position->pay_if_canceled;
               break;
            default:
               continue;
         }
         remove(*position);
      }

      // pay the fees to the dividend-distribution account if net profit
      share_type rake_amount;
      if (net_profits.value > 0 && rake_account_id)
      {
         rake_amount = ((fc::uint128_t(net_profits.value) * rake_fee_percentage + GRAPHENE_100_PERCENT - 1) / GRAPHENE_100_PERCENT).to_uint64();
         share_type affiliates_share;
         if (rake_amount.value)
            affiliates_share = payout_helper.payout( bettor_id, rake_amount );
         FC_ASSERT( rake_amount.value >= affiliates_share.value );
         if (rake_amount.value > affiliates_share.value)
            adjust_balance(*rake_account_id, asset(rake_amount - affiliates_share, betting_market_group.asset_id));
      }
      
      // pay winning - rake
      adjust_balance(bettor_id, asset(payout_amounts - rake_amount, betting_market_group.asset_id));
      // [ROL]
      //fc_idump(fc::logger::get("betting"), (payout_amounts)(net_profits.value)(rake_amount.value));

      push_applied_operation(betting_market_group_resolved_operation(bettor_id,
                             betting_market_group.id,
                             resolutions_by_market_id,
                             payout_amounts,
                             rake_amount));
   }

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

   const event_object& event = betting_market_group.event_id(*this);

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

share_type adjust_betting_position(database& db, 
                                   account_id_type bettor_id, 
                                   betting_market_id_type betting_market_id, 
                                   bet_type back_or_lay, 
                                   share_type bet_amount, 
                                   share_type matched_amount)
{ try {
   assert(bet_amount >= 0);
   
   share_type guaranteed_winnings_returned = 0;

   if (bet_amount == 0)
      return guaranteed_winnings_returned;

   auto& index = db.get_index_type<betting_market_position_index>().indices().get<by_bettor_betting_market>();
   auto itr = index.find(boost::make_tuple(bettor_id, betting_market_id));
   if (itr == index.end())
   {
      db.create<betting_market_position_object>([&](betting_market_position_object& position) {
         position.bettor_id = bettor_id;
         position.betting_market_id = betting_market_id;
         position.pay_if_payout_condition = back_or_lay == bet_type::back ? bet_amount + matched_amount : 0;
         position.pay_if_not_payout_condition = back_or_lay == bet_type::lay ? bet_amount + matched_amount : 0;
         position.pay_if_canceled = bet_amount;
         position.pay_if_not_canceled = 0;
         // this should not be reducible
      });
   } else {
      db.modify(*itr, [&](betting_market_position_object& position) {
         assert(position.bettor_id == bettor_id);
         assert(position.betting_market_id == betting_market_id);
         position.pay_if_payout_condition += back_or_lay == bet_type::back ? bet_amount + matched_amount : 0;
         position.pay_if_not_payout_condition += back_or_lay == bet_type::lay ? bet_amount + matched_amount : 0;
         position.pay_if_canceled += bet_amount;

         guaranteed_winnings_returned = position.reduce();
      });
   }
   return guaranteed_winnings_returned;
} FC_CAPTURE_AND_RETHROW((bettor_id)(betting_market_id)(bet_amount)) }


// called twice when a bet is matched, once for the taker, once for the maker
bool bet_was_matched(database& db, const bet_object& bet, 
                     share_type amount_bet, share_type amount_matched, 
                     bet_multiplier_type actual_multiplier,
                     bool refund_unmatched_portion)
{
   // record their bet, modifying their position, and return any winnings
   share_type guaranteed_winnings_returned = adjust_betting_position(db, bet.bettor_id, bet.betting_market_id, 
                                                                     bet.back_or_lay, amount_bet, amount_matched);
   db.adjust_balance(bet.bettor_id, asset(guaranteed_winnings_returned, bet.amount_to_bet.asset_id));

   // generate a virtual "match" op
   asset asset_amount_bet(amount_bet, bet.amount_to_bet.asset_id);

   bet_matched_operation bet_matched_virtual_op(bet.bettor_id, bet.id, 
                                                asset_amount_bet,
                                                actual_multiplier,
                                                guaranteed_winnings_returned);
   //fc_edump(fc::logger::get("betting"), (bet_matched_virtual_op));
   db.push_applied_operation(std::move(bet_matched_virtual_op));

   // update the bet on the books
   if (asset_amount_bet == bet.amount_to_bet)
   {
      db.remove(bet);
      return true;
   }
   else
   {
      db.modify(bet, [&](bet_object& bet_obj) {
         bet_obj.amount_to_bet -= asset_amount_bet;
      });

      if (refund_unmatched_portion)
      {
         db.cancel_bet(bet);
         return true;
      }
      else
         return false;
   }
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
int match_bet(database& db, const bet_object& taker_bet, const bet_object& maker_bet )
{
   //fc_idump(fc::logger::get("betting"), (taker_bet)(maker_bet));
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
   if (taker_bet.back_or_lay == bet_type::lay) {
      share_type maximum_factor_taker_is_willing_to_receive = taker_bet.get_exact_matching_amount() / maker_odds_ratio;
      //fc_idump(fc::logger::get("betting"), (maximum_factor_taker_is_willing_to_pay));
      bool taker_was_limited_by_matching_amount = maximum_factor_taker_is_willing_to_receive < maximum_factor_taker_is_willing_to_pay;
      if (taker_was_limited_by_matching_amount)
         maximum_taker_factor = maximum_factor_taker_is_willing_to_receive;
   }
   //fc_idump(fc::logger::get("betting"), (maximum_factor_taker_is_willing_to_pay)(maximum_taker_factor));

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

         db.adjust_balance(taker_bet.bettor_id, asset(taker_refund_amount, taker_bet.amount_to_bet.asset_id));
         // TODO: update global statistics
         bet_adjusted_operation bet_adjusted_op(taker_bet.bettor_id, taker_bet.id, 
                                                asset(taker_refund_amount, taker_bet.amount_to_bet.asset_id));
         // fc_idump(fc::logger::get("betting"), (bet_adjusted_op)(new_bet_object));
         db.push_applied_operation(std::move(bet_adjusted_op));
      }
   }

   // if the maker bet stays on the books, we need to make sure the taker bet is removed from the books (either it fills completely,
   // or any un-filled amount is canceled)
   result |= bet_was_matched(db, taker_bet, taker_amount_to_match, maker_amount_to_match, maker_bet.backer_multiplier, !maker_bet_will_completely_match);
   result |= bet_was_matched(db, maker_bet, maker_amount_to_match, taker_amount_to_match, maker_bet.backer_multiplier, false) << 1;

   assert(result != 0);
   return result;
}


// called from the bet_place_evaluator
bool database::place_bet(const bet_object& new_bet_object)
{
   // We allow users to place bets for any amount, but only amounts that are exact multiples of the odds
   // ratio can be matched.  Immediately return any unmatchable amount in this bet.
   share_type minimum_matchable_amount = new_bet_object.get_minimum_matchable_amount();
   share_type scale_factor = new_bet_object.amount_to_bet.amount / minimum_matchable_amount;
   share_type rounded_bet_amount = scale_factor * minimum_matchable_amount;

   if (rounded_bet_amount == share_type())
   {
      // the bet was too small to match at all, cancel the bet
      cancel_bet(new_bet_object, true);
      return true;
   }
   else if (rounded_bet_amount != new_bet_object.amount_to_bet.amount)
   {
      asset stake_returned = new_bet_object.amount_to_bet;
      stake_returned.amount -= rounded_bet_amount;

      modify(new_bet_object, [&rounded_bet_amount](bet_object& modified_bet_object) {
             modified_bet_object.amount_to_bet.amount = rounded_bet_amount;
             });

      adjust_balance(new_bet_object.bettor_id, stake_returned);
      // TODO: update global statistics
      bet_adjusted_operation bet_adjusted_op(new_bet_object.bettor_id, new_bet_object.id, 
                                             stake_returned);
      // fc_idump(fc::logger::get("betting"), (bet_adjusted_op)(new_bet_object));
      push_applied_operation(std::move(bet_adjusted_op));

      fc_dlog(fc::logger::get("betting"), "Refunded ${refund_amount} to round the bet down to something that can match exactly, new bet: ${new_bet}",
           ("refund_amount", stake_returned.amount)
           ("new_bet", new_bet_object));
   }

   const auto& bet_odds_idx = get_index_type<bet_object_index>().indices().get<by_odds>();

   bet_type bet_type_to_match = new_bet_object.back_or_lay == bet_type::back ? bet_type::lay : bet_type::back;
   auto book_itr = bet_odds_idx.lower_bound(std::make_tuple(new_bet_object.betting_market_id, bet_type_to_match));
   auto book_end = bet_odds_idx.upper_bound(std::make_tuple(new_bet_object.betting_market_id, bet_type_to_match, new_bet_object.backer_multiplier));

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

      orders_matched_flags = match_bet(*this, new_bet_object, *old_book_itr);

      // we continue if the maker bet was completely consumed AND the taker bet was not
      finished = orders_matched_flags != 2;
   }
   if (!(orders_matched_flags & 1))
      fc_ddump(fc::logger::get("betting"), (new_bet_object));


   // return true if the taker bet was completely consumed
   return (orders_matched_flags & 1) != 0;
}

} }
