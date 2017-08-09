#include <graphene/chain/database.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/betting_market_object.hpp>
#include <graphene/chain/event_object.hpp>

#include <boost/range/iterator_range.hpp>

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
      //idump((bet_canceled_virtual_op));
      push_applied_operation(std::move(bet_canceled_virtual_op));
   }
   remove(bet);
}

void database::cancel_all_unmatched_bets_on_betting_market(const betting_market_object& betting_market)
{
   const auto& bet_odds_idx = get_index_type<bet_object_index>().indices().get<by_odds>();
   auto book_itr = bet_odds_idx.lower_bound(std::make_tuple(betting_market.id));
   auto book_end = bet_odds_idx.upper_bound(std::make_tuple(betting_market.id));
   while (book_itr != book_end)
   {
      auto old_book_itr = book_itr;
      ++book_itr;
      cancel_bet(*old_book_itr, true);
   }
}

void database::cancel_all_betting_markets_for_event(const event_object& event_obj)
{
   auto& betting_market_group_index = get_index_type<betting_market_group_object_index>().indices().get<by_event_id>();
   auto& betting_market_index = get_index_type<betting_market_object_index>().indices().get<by_betting_market_group_id>();

   //for each betting market group of event
   for (const betting_market_group_object& betting_market_group : 
        boost::make_iterator_range(betting_market_group_index.equal_range(event_obj.id)))
      resolve_betting_market_group(betting_market_group, {});
}

void database::validate_betting_market_group_resolutions(const betting_market_group_object& betting_market_group,
                                                         const std::map<betting_market_id_type, betting_market_resolution_type>& resolutions)
{
    auto& betting_market_index = get_index_type<betting_market_object_index>().indices().get<by_betting_market_group_id>();
    auto betting_market_itr = betting_market_index.lower_bound(betting_market_group.id);
    while (betting_market_itr != betting_market_index.end() &&  betting_market_itr->group_id == betting_market_group.id)
    {
       const betting_market_object& betting_market = *betting_market_itr;
       // every betting market in the group tied with resolution
       //idump((betting_market.id)(resolutions));
       assert(resolutions.count(betting_market.id));
       ++betting_market_itr;
    }
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
                                            const std::map<betting_market_id_type, betting_market_resolution_type>& resolutions,
                                            bool  do_not_remove)
{
   bool cancel = resolutions.size() == 0;

   // we pay the rake fee to the dividend distribution account for the core asset, go ahead
   // and look up that account now
   fc::optional<account_id_type> rake_account_id;
   const asset_object& core_asset_obj = asset_id_type(0)(*this);
   if (core_asset_obj.dividend_data_id)
   {
      const asset_dividend_data_object& core_asset_dividend_data_obj = (*core_asset_obj.dividend_data_id)(*this);
      rake_account_id = core_asset_dividend_data_obj.dividend_distribution_account;
   }

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
      ++betting_market_itr;
      cancel_all_unmatched_bets_on_betting_market(betting_market);

      // [ROL] why tuple
      //auto position_itr = position_index.lower_bound(std::make_tuple(betting_market.id));
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
      uint16_t rake_fee_percentage = get_global_properties().parameters.betting_rake_fee_percentage;
      share_type net_profits;
      share_type payout_amounts;
      account_id_type bettor_id = bettor_positions_pair.first;
      const std::vector<const betting_market_position_object*>& bettor_positions = bettor_positions_pair.second;

      for (const betting_market_position_object* position : bettor_positions)
      {
         betting_market_resolution_type resolution;
         if (cancel)
            resolution = betting_market_resolution_type::cancel;
         else
         {
            // checked in evaluator, should never happen, see above
            assert(resolutions.count(position->betting_market_id));
            resolution = resolutions.at(position->betting_market_id);
         }

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
         if (rake_amount.value)
            adjust_balance(*rake_account_id, asset(rake_amount, betting_market_group.asset_id));
      }
      
      // pay winning - rake
      adjust_balance(bettor_id, asset(payout_amounts - rake_amount, betting_market_group.asset_id));
      // [ROL]
      //idump((payout_amounts)(net_profits.value)(rake_amount.value));

      push_applied_operation(betting_market_group_resolved_operation(bettor_id,
                             betting_market_group.id,
                             resolutions,
                             payout_amounts,
                             rake_amount));
   }

   betting_market_itr = betting_market_index.lower_bound(betting_market_group.id);
   while (betting_market_itr != betting_market_index.end() &&  betting_market_itr->group_id == betting_market_group.id)
   {
      const betting_market_object& betting_market = *betting_market_itr;
      ++betting_market_itr;
      if (!do_not_remove)
        remove(betting_market);
   }
   if (!do_not_remove)
       remove(betting_market_group);
}

#if 0
void database::get_required_deposit_for_bet(const betting_market_object& betting_market, 
                                            betting_market_resolution_type resolution)
{
}
#endif

share_type adjust_betting_position(database& db, account_id_type bettor_id, betting_market_id_type betting_market_id, bet_type back_or_lay, share_type bet_amount, share_type matched_amount)
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
                     bet_multiplier_type actual_multiplier)
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
   //edump((bet_matched_virtual_op));
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
   assert(taker_bet.amount_to_bet.asset_id == maker_bet.amount_to_bet.asset_id);
   assert(taker_bet.amount_to_bet.amount > 0 && maker_bet.amount_to_bet.amount > 0);
   assert(taker_bet.back_or_lay == bet_type::back ? taker_bet.backer_multiplier <= maker_bet.backer_multiplier : taker_bet.backer_multiplier >= maker_bet.backer_multiplier);
   assert(taker_bet.back_or_lay != maker_bet.back_or_lay);

   int result = 0;
   //idump((taker_bet)(maker_bet));

   // using the maker's odds, figure out how much of the maker's bet we would match, rounding down
   // go ahead and get look up the ratio for the bet (a bet with odds 1.92 will have a ratio 25:23)
   share_type back_odds_ratio;
   share_type lay_odds_ratio;
   std::tie(back_odds_ratio, lay_odds_ratio) = maker_bet.get_ratio();

   // and make some shortcuts to get to the maker's and taker's side of the ratio
   const share_type& maker_odds_ratio = maker_bet.back_or_lay == bet_type::back ? back_odds_ratio : lay_odds_ratio;
   const share_type& taker_odds_ratio = maker_bet.back_or_lay == bet_type::back ? lay_odds_ratio : back_odds_ratio;
   //idump((back_odds_ratio)(lay_odds_ratio));
   //idump((maker_odds_ratio)(taker_odds_ratio));

   // now figure out how much of the maker bet we'll consume.  We don't yet know whether the maker or taker
   // will be the limiting factor.
   share_type maximum_taker_factor = taker_bet.amount_to_bet.amount / taker_odds_ratio;
   share_type maximum_maker_factor = maker_bet.amount_to_bet.amount / maker_odds_ratio;
   share_type maximum_factor = std::min(maximum_taker_factor, maximum_maker_factor);
   share_type maker_amount_to_match = maximum_factor * maker_odds_ratio;
   share_type taker_amount_to_match = maximum_factor * taker_odds_ratio;
   //idump((maker_amount_to_match)(taker_amount_to_match));

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

   //idump((taker_amount_to_match)(maker_amount_to_match));

   result |= bet_was_matched(db, taker_bet, taker_amount_to_match, maker_amount_to_match, maker_bet.backer_multiplier);
   result |= bet_was_matched(db, maker_bet, maker_amount_to_match, taker_amount_to_match, maker_bet.backer_multiplier) << 1;

   assert(result != 0);
   return result;
}


// called from the bet_place_evaluator
bool database::place_bet(const bet_object& new_bet_object)
{
   const auto& bet_odds_idx = get_index_type<bet_object_index>().indices().get<by_odds>();

   bet_type bet_type_to_match = new_bet_object.back_or_lay == bet_type::back ? bet_type::lay : bet_type::back;
   auto book_itr = bet_odds_idx.lower_bound(std::make_tuple(new_bet_object.betting_market_id, bet_type_to_match));
   auto book_end = bet_odds_idx.upper_bound(std::make_tuple(new_bet_object.betting_market_id, bet_type_to_match, new_bet_object.backer_multiplier));

   // ilog("");
   // ilog("------------  order book ------------------");
   // for (auto itr = book_itr; itr != book_end; ++itr)
   //    idump((*itr));
   // ilog("------------  order book ------------------");

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
   {
      // if the new (taker) bet was not completely consumed, we need to put whatever remains
      // of it on the books.  But we only allow bets that can be exactly matched
      // on the books, so round the amount down if necessary
      share_type minimum_matchable_amount = new_bet_object.get_minimum_matchable_amount();
      share_type scale_factor = new_bet_object.amount_to_bet.amount / minimum_matchable_amount;
      share_type rounded_bet_amount = scale_factor * minimum_matchable_amount;
      //idump((new_bet_object.amount_to_bet.amount)(rounded_bet_amount)(minimum_matchable_amount)(scale_factor));

      if (rounded_bet_amount == share_type())
      {
         // the remainder of the bet was too small to match, cancel the bet
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
         // idump((bet_adjusted_op)(new_bet_object));
         push_applied_operation(std::move(bet_adjusted_op));
         return false;
      }
      else
         return false;
   }
   else
      return true;
}

} }
