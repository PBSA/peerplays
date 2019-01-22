#include <graphene/chain/bitcoin_transaction_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/bitcoin_transaction_object.hpp>

namespace graphene { namespace chain {

void_result bitcoin_transaction_send_evaluator::do_evaluate( const bitcoin_transaction_send_operation& op )
{
   // FC_ASSERT( db().get_sidechain_account_id() == op.payer );
   return void_result();
}

object_id_type bitcoin_transaction_send_evaluator::do_apply( const bitcoin_transaction_send_operation& op )
{  
   database& d = db();
   sidechain::prev_out new_pw_vout = { "", 0, 0 };
   sidechain::bytes wit_script;

   const bitcoin_transaction_object& btc_tx = d.create< bitcoin_transaction_object >( [&]( bitcoin_transaction_object& obj )
   {
      obj.pw_vin = op.pw_vin;
      obj.vins = op.vins;
      obj.vouts = op.vouts;
      obj.transaction = op.transaction;
      obj.transaction_id = op.transaction.get_txid();
      obj.fee_for_size = op.fee_for_size;
      obj.confirm = false;
   });

   if( btc_tx.transaction.vout[0].is_p2wsh() && btc_tx.transaction.vout[0].scriptPubKey == wit_script )
      new_pw_vout = { btc_tx.transaction.get_txid(), 0, btc_tx.transaction.vout[0].value };

   d.pw_vout_manager.create_new_vout( new_pw_vout );
   send_bitcoin_transaction( btc_tx );

   return btc_tx.id;
}

void bitcoin_transaction_send_evaluator::send_bitcoin_transaction( const bitcoin_transaction_object& btc_tx )
{
   database& d = db();
   uint32_t skip = d.get_node_properties().skip_flags;
   if( !(skip & graphene::chain::database::skip_btc_tx_sending) ){
      d.send_btc_tx( btc_tx.transaction );
   }
}

} }
