#include <graphene/chain/database.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/betting_market_object.hpp>

namespace graphene { namespace chain {

void database::cancel_bet( const bet_object& bet, bool create_virtual_op )
{
   asset amount_to_refund = bet.amount_to_bet;
   amount_to_refund += bet.amount_reserved_for_fees;
   //TODO: update global statistics
   adjust_balance(bet.bettor_id, amount_to_refund); //return unmatched stake
   //TODO: do special fee accounting as required
   if (create_virtual_op)
      push_applied_operation(bet_canceled_operation(bet.bettor_id, bet.id,
                                                    bet.amount_to_bet,
                                                    bet.amount_reserved_for_fees));
   remove(bet);
}

bool maybe_cull_small_bet( database& db, const bet_object& bet_object_to_cull )
{
   return false;
}

bool database::place_bet(const bet_object& new_bet_object)
{
   return false;
}

} }
