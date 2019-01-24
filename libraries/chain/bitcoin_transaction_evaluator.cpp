#include <graphene/chain/bitcoin_transaction_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/bitcoin_transaction_object.hpp>
#include <graphene/chain/info_for_used_vin_object.hpp>

#include <sidechain/sign_bitcoin_transaction.hpp>

namespace graphene { namespace chain {

void_result bitcoin_transaction_send_evaluator::do_evaluate( const bitcoin_transaction_send_operation& op )
{
   // FC_ASSERT( db().get_sidechain_account_id() == op.payer );
   return void_result();
}

object_id_type bitcoin_transaction_send_evaluator::do_apply( const bitcoin_transaction_send_operation& op )
{
   bitcoin_transaction_send_operation& mutable_op = const_cast<bitcoin_transaction_send_operation&>( op );
   database& d = db();
   std::vector< info_for_used_vin_id_type > new_vins;
   sidechain::prev_out new_pw_vout = { "", 0, 0 };
   sidechain::bytes wit_script;

   finalize_bitcoin_transaction( mutable_op );

   for( auto itr : op.vins ){
      auto itr_id = d.create< info_for_used_vin_object >( [&]( info_for_used_vin_object& obj )
      {
         obj.identifier = itr.identifier;
         obj.out = itr.out;
         obj.address = itr.address;
         obj.script = itr.script;
      }).get_id();
      new_vins.push_back( itr_id );
   }

   const bitcoin_transaction_object& btc_tx = d.create< bitcoin_transaction_object >( [&]( bitcoin_transaction_object& obj )
   {
      obj.pw_vin = mutable_op.pw_vin;
      obj.vins = new_vins;
      obj.vouts = mutable_op.vouts;
      obj.transaction = mutable_op.transaction;
      obj.transaction_id = mutable_op.transaction.get_txid();
      obj.fee_for_size = mutable_op.fee_for_size;
      obj.confirm = false;
   });

   if( btc_tx.transaction.vout[0].is_p2wsh() && btc_tx.transaction.vout[0].scriptPubKey == wit_script )
      new_pw_vout = { btc_tx.transaction.get_txid(), 0, btc_tx.transaction.vout[0].value };

   d.pw_vout_manager.create_new_vout( new_pw_vout );
   send_bitcoin_transaction( btc_tx );

   return btc_tx.id;
}

void bitcoin_transaction_send_evaluator::finalize_bitcoin_transaction( bitcoin_transaction_send_operation& op )
{
   database& d = db();

   std::vector<std::vector<sidechain::bytes>> new_stacks( sidechain::sort_sigs( op.transaction, op.vins, d.context_verify ) );

   for( size_t i = 0; i < new_stacks.size(); i++ ) {
      op.transaction.vin[i].scriptWitness = new_stacks[i];
   }

   sidechain::sign_witness_transaction_finalize( op.transaction, op.vins );
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
