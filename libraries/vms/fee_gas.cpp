#include "fee_gas.hpp"

namespace vms { namespace base {

void fee_gas::prepare_fee(share_type fee, const contract_operation& op){
   fee_from_account = asset(fee, op.fee.asset_id);
   FC_ASSERT( fee >= 0 );
   fee_paying_account = &op.registrar(d);
   fee_paying_account_statistics = &fee_paying_account->statistics(d);

   fee_asset = &op.fee.asset_id(d);
   fee_asset_dyn_data = &fee_asset->dynamic_asset_data_id(d);

   if( d.head_block_time() > fc::time_point_sec( 1446652800 )) //HARDFORK_419_TIME
   {
      FC_ASSERT( is_authorized_asset( d, *fee_paying_account, *fee_asset ), "Account ${acct} '${name}' attempted to pay fee by using asset ${a} '${sym}', which is unauthorized due to whitelist / blacklist",
         ("acct", fee_paying_account->id)("name", fee_paying_account->name)("a", fee_asset->id)("sym", fee_asset->symbol) );
   }

   if( fee_from_account.asset_id == asset_id_type() )
      core_fee_paid = fee_from_account.amount;
   else
   {
      asset fee_from_pool = fee_from_account * fee_asset->options.core_exchange_rate;
      FC_ASSERT( fee_from_pool.asset_id == asset_id_type() );
      core_fee_paid = fee_from_pool.amount;
      FC_ASSERT( core_fee_paid <= fee_asset_dyn_data->fee_pool, "Fee pool balance of '${b}' is less than the ${r} required to convert ${c}",
         ("r", d.to_pretty_string( fee_from_pool))("b",d.to_pretty_string(fee_asset_dyn_data->fee_pool))("c",d.to_pretty_string(op.fee)) );
   }
}

void fee_gas::convert_fee(){
   if( !trx_state->skip_fee ) {
      if( fee_asset->get_id() != asset_id_type() )
      {
         d.modify(*fee_asset_dyn_data, [this](asset_dynamic_data_object& dd) {
         dd.accumulated_fees += fee_from_account.amount;
         dd.fee_pool -= core_fee_paid;
         });
      }
   }
}

void fee_gas::pay_fee()
{ try {
   if( !trx_state->skip_fee ) {
      d.modify(*fee_paying_account_statistics, [&](account_statistics_object& s){
         s.pay_fee( core_fee_paid, d.get_global_properties().parameters.cashback_vesting_threshold );
      });
   }
} FC_CAPTURE_AND_RETHROW() }

void fee_gas::process_fee(share_type fee, const contract_operation& op){
   prepare_fee(fee, op);
   convert_fee();
   pay_fee();
}

} }
