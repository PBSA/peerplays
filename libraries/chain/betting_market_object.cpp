#include <graphene/chain/betting_market_object.hpp>

namespace graphene { namespace chain {

/* static */ share_type bet_object::get_matching_amount(share_type bet_amount, bet_multiplier_type backer_multiplier, bet_type back_or_lay)
{
   fc::uint128_t amount_to_match_128 = bet_amount.value;

   if (back_or_lay == bet_type::back)
   {
       amount_to_match_128 *= backer_multiplier - GRAPHENE_100_PERCENT;
       amount_to_match_128 /= GRAPHENE_100_PERCENT;
   }
   else
   {
       amount_to_match_128 *= GRAPHENE_100_PERCENT;
       amount_to_match_128 /= backer_multiplier - GRAPHENE_100_PERCENT;
   }
   return amount_to_match_128.to_uint64();
}

share_type bet_object::get_matching_amount() const
{
   return get_matching_amount(amount_to_bet.amount, backer_multiplier, back_or_lay);
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

