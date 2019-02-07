#include <graphene/chain/withdraw_pbtc_evaluator.hpp>
#include <graphene/chain/info_for_vout_object.hpp>
#include <graphene/chain/bitcoin_address_object.hpp>

#include <sidechain/sidechain_condensing_tx.hpp>
#include <sidechain/input_withdrawal_info.hpp>
#include <sidechain/bitcoin_address.hpp>
#include <sidechain/utils.hpp>

namespace graphene { namespace chain {

void_result withdraw_pbtc_evaluator::do_evaluate(const withdraw_pbtc_operation& op)
{
   database& d = db();

   FC_ASSERT( !d.is_sidechain_fork_needed() );
   FC_ASSERT( op.data.size() > 0 );
   type = bitcoin_address( op.data ).get_type();
   FC_ASSERT( type != payment_type::NULLDATA , "Invalid address type." );
   FC_ASSERT( check_amount_higher_than_fee( op ) );
   asset acc_balance = db().get_balance( op.payer, d.get_sidechain_asset_id() );
   FC_ASSERT( acc_balance.amount.value >= op.amount );

   return void_result();
}

void_result withdraw_pbtc_evaluator::do_apply(const withdraw_pbtc_operation& op)
{
   db().i_w_info.insert_info_for_vout( op.payer, op.data, op.amount );
   reserve_issue( op );
   return void_result();
}

void withdraw_pbtc_evaluator::reserve_issue( const withdraw_pbtc_operation& op )
{
   database& d = db();

   bool skip_fee_old = trx_state->skip_fee;
   bool skip_fee_schedule_check_old = trx_state->skip_fee_schedule_check;
   trx_state->skip_fee = true;
   trx_state->skip_fee_schedule_check = true;

   asset_reserve_operation reserve_op;
   reserve_op.amount_to_reserve = asset( op.amount, d.get_sidechain_asset_id() );
   reserve_op.payer = op.payer;

   d.apply_operation( *trx_state, reserve_op );

   trx_state->skip_fee = skip_fee_old;
   trx_state->skip_fee_schedule_check = skip_fee_schedule_check_old;
}

bool withdraw_pbtc_evaluator::check_amount_higher_than_fee( const withdraw_pbtc_operation& op ) {
   database& d = db();

   info_for_vout_object obj;
   obj.payer = op.payer;
   obj.address = op.data;
   obj.amount = op.amount;
   obj.used = false;

   const auto& pw_address = d.get_latest_PW().address;

   sidechain::info_for_vin pw_vin;
   pw_vin.identifier = fc::sha256( std::string(64, '1') );
   pw_vin.out.hash_tx = std::string(64, '1');
   pw_vin.out.n_vout = 0;
   pw_vin.out.amount = 2*op.amount;
   pw_vin.address = pw_address.get_address();
   pw_vin.script = pw_address.get_witness_script();

   const auto& mock_trx = d.create_btc_transaction( {}, { obj }, pw_vin );

   if( op.amount < mock_trx.second )
      return false;

   uint64_t fee_for_witnesses = ( (op.amount - mock_trx.second) * d.get_sidechain_params().percent_payment_to_witnesses ) / GRAPHENE_100_PERCENT;

   if( op.amount < mock_trx.second + fee_for_witnesses )
      return false;

   return true;
}

} } // namespace graphene::chain
