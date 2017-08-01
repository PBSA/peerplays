#include <graphene/chain/betting_market_object.hpp>
#include <boost/math/common_factor_rt.hpp>

namespace graphene { namespace chain {

/* static */ share_type bet_object::get_approximate_matching_amount(share_type bet_amount, bet_multiplier_type backer_multiplier, bet_type back_or_lay, bool round_up /* = false */)
{
   fc::uint128_t amount_to_match_128 = bet_amount.value;

   if (back_or_lay == bet_type::back)
   {
       amount_to_match_128 *= backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION;
       if (round_up)
         amount_to_match_128 += GRAPHENE_BETTING_ODDS_PRECISION - 1;
       amount_to_match_128 /= GRAPHENE_BETTING_ODDS_PRECISION;
   }
   else
   {
       amount_to_match_128 *= GRAPHENE_BETTING_ODDS_PRECISION;
       if (round_up)
         amount_to_match_128 += backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION - 1;
       amount_to_match_128 /= backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION;
   }
   return amount_to_match_128.to_uint64();
}

share_type bet_object::get_approximate_matching_amount(bool round_up /* = false */) const
{
   return get_approximate_matching_amount(amount_to_bet.amount, backer_multiplier, back_or_lay, round_up);
}

/* static */ share_type bet_object::get_exact_matching_amount(share_type bet_amount, bet_multiplier_type backer_multiplier, bet_type back_or_lay)
{
   share_type back_ratio;
   share_type lay_ratio;
   std::tie(back_ratio, lay_ratio) = get_ratio(backer_multiplier);
   if (back_or_lay == bet_type::back)
      return bet_amount / back_ratio * lay_ratio;
   else
      return bet_amount / lay_ratio * back_ratio;
}

share_type bet_object::get_exact_matching_amount() const
{
   return get_exact_matching_amount(amount_to_bet.amount, backer_multiplier, back_or_lay);
}

/* static */ std::pair<share_type, share_type> bet_object::get_ratio(bet_multiplier_type backer_multiplier)
{
   share_type gcd = boost::math::gcd<bet_multiplier_type>(GRAPHENE_BETTING_ODDS_PRECISION, backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION);
   return std::make_pair(GRAPHENE_BETTING_ODDS_PRECISION / gcd, (backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION) / gcd);
}

std::pair<share_type, share_type> bet_object::get_ratio() const
{
   return get_ratio(backer_multiplier);
}

share_type bet_object::get_minimum_matchable_amount() const
{
   share_type gcd = boost::math::gcd<bet_multiplier_type>(GRAPHENE_BETTING_ODDS_PRECISION, backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION);
   return (back_or_lay == bet_type::back ? GRAPHENE_BETTING_ODDS_PRECISION : backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION) / gcd;
}

share_type bet_object::get_minimum_matching_amount() const
{
   share_type gcd = boost::math::gcd<bet_multiplier_type>(GRAPHENE_BETTING_ODDS_PRECISION, backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION);
   return (back_or_lay == bet_type::lay ? GRAPHENE_BETTING_ODDS_PRECISION : backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION) / gcd;
}


share_type betting_market_position_object::reduce()
{
   share_type additional_not_cancel_balance = std::min(pay_if_payout_condition, pay_if_not_payout_condition);
   if (additional_not_cancel_balance == 0)
      return 0;
   pay_if_payout_condition -= additional_not_cancel_balance;
   pay_if_not_payout_condition -= additional_not_cancel_balance;
   pay_if_not_canceled += additional_not_cancel_balance;
   
   share_type immediate_winnings = std::min(pay_if_canceled, pay_if_not_canceled);
   if (immediate_winnings == 0)
      return 0;
   pay_if_canceled -= immediate_winnings;
   pay_if_not_canceled -= immediate_winnings;
   return immediate_winnings;
}

} } // graphene::chain

