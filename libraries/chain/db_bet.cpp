#include <graphene/chain/database.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/betting_market_object.hpp>

namespace graphene { namespace chain {

void database::cancel_bet( const bet_object& bet, bool create_virtual_op )
{
   share_type amount_to_refund = bet.amount_to_bet;
   //TODO: update global statistics
   adjust_balance(bet.bettor_id, amount_to_refund); //return unmatched stake
   //TODO: do special fee accounting as required
   if (create_virtual_op)
   {
     bet_cancel_operation vop;
     vop.bettor_id = bet.bettor_id;
     vop.bet_to_cancel = bet.id;
     push_applied_operation(vop);
   }
   remove(bet);
}

} }
