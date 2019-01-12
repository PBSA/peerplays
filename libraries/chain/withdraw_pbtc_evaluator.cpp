#include <graphene/chain/withdraw_pbtc_evaluator.hpp>
#include <graphene/chain/info_for_vout_object.hpp>

#include <sidechain/bitcoin_address.hpp>
#include <sidechain/utils.hpp>

namespace graphene { namespace chain {

void_result withdraw_pbtc_evaluator::do_evaluate(const withdraw_pbtc_operation& op)
{
   database& d = db();

   // FC_ASSERT( !d.is_sidechain_fork_needed() );
   FC_ASSERT( op.data.size() > 0 );
   type = bitcoin_address( op.data ).get_type();
   FC_ASSERT( type != payment_type::NULLDATA , "Invalid address type." );
   FC_ASSERT( check_amount( op ) );
   // asset acc_balance = db().get_balance( op.payer, d.get_sidechain_asset_id() );
   // FC_ASSERT( acc_balance.amount.value >= op.amount );

   return void_result();
}

object_id_type withdraw_pbtc_evaluator::do_apply(const withdraw_pbtc_operation& op)
{
   database& d = db();

   auto id = d.create<info_for_vout_object>( [&]( info_for_vout_object& obj ) {
      obj.payer = op.payer;
      obj.addr_type = type;
      obj.data = op.data;
      obj.amount = op.amount;
   } ).get_id();

   reserve_issue( op );

   return id;
}

void withdraw_pbtc_evaluator::reserve_issue( const withdraw_pbtc_operation& op )
{

}

bool withdraw_pbtc_evaluator::check_amount( const withdraw_pbtc_operation& op ) {
   return true;
}

} } // namespace graphene::chain
