#include <graphene/chain/bitcoin_transaction_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/bitcoin_transaction_object.hpp>
#include <graphene/chain/info_for_used_vin_object.hpp>
#include <graphene/chain/sidechain_proposal_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/proposal_object.hpp>

#include <sidechain/sign_bitcoin_transaction.hpp>
#include <sidechain/input_withdrawal_info.hpp>
#include <sidechain/sidechain_proposal_checker.hpp>
#include <graphene/chain/bitcoin_address_object.hpp>
#include <graphene/chain/primary_wallet_vout_object.hpp>

namespace graphene { namespace chain {

using namespace sidechain;

void_result bitcoin_transaction_send_evaluator::do_evaluate( const bitcoin_transaction_send_operation& op )
{
   // FC_ASSERT( db().get_sidechain_account_id() == op.payer );
   return void_result();
}

object_id_type bitcoin_transaction_send_evaluator::do_apply( const bitcoin_transaction_send_operation& op )
{
   bitcoin_transaction_send_operation& mutable_op = const_cast<bitcoin_transaction_send_operation&>( op );
   database& d = db();

   finalize_bitcoin_transaction( mutable_op );

   auto new_vins = create_transaction_vins( mutable_op );

   const bitcoin_transaction_object& btc_tx = d.create< bitcoin_transaction_object >( [&]( bitcoin_transaction_object& obj )
   {
      obj.pw_vin = mutable_op.pw_vin.identifier;
      obj.vins = new_vins;
      obj.vouts = mutable_op.vouts;
      obj.transaction = mutable_op.transaction;
      obj.transaction_id = mutable_op.transaction.get_txid();
      obj.fee_for_size = mutable_op.fee_for_size;
   });

   sidechain::prev_out new_pw_vout = { "", 0, 0 };
   sidechain::bytes wit_script = d.get_latest_PW().address.get_witness_script();
   if( btc_tx.transaction.vout[0].is_p2wsh() && btc_tx.transaction.vout[0].scriptPubKey == wit_script )
      new_pw_vout = { btc_tx.transaction.get_txid(), 0, btc_tx.transaction.vout[0].value };

   d.pw_vout_manager.create_new_vout( new_pw_vout );
   send_bitcoin_transaction( btc_tx );

   return btc_tx.id;
}

std::vector<fc::sha256> bitcoin_transaction_send_evaluator::create_transaction_vins( bitcoin_transaction_send_operation& op )
{
   database& d = db();
   std::vector<fc::sha256> new_vins;

   for( auto itr : op.vins ) {
      auto info_for_used_vin_itr = d.create< info_for_used_vin_object >( [&]( info_for_used_vin_object& obj )
      {
         obj.identifier = itr.identifier;
         obj.out = itr.out;
         obj.address = itr.address;
         obj.script = itr.script;
      });
      new_vins.push_back( info_for_used_vin_itr.identifier );

      auto obj_itr = d.i_w_info.find_info_for_vin( itr.identifier );
      if( obj_itr.valid() ) {
         d.i_w_info.remove_info_for_vin( *obj_itr );
      }
   }

   return new_vins;
}

void bitcoin_transaction_send_evaluator::finalize_bitcoin_transaction( bitcoin_transaction_send_operation& op )
{
   database& d = db();

   auto vins = op.vins;
   if( op.pw_vin.identifier.str().compare( 0, 48, SIDECHAIN_NULL_VIN_IDENTIFIER ) != 0 ) {
      vins.insert( vins.begin(), op.pw_vin );
   }

   std::vector<bytes> redeem_scripts( d.i_w_info.get_redeem_scripts( vins ) );
   std::vector<uint64_t> amounts( d.i_w_info.get_amounts( vins ) );

   std::vector<std::vector<sidechain::bytes>> new_stacks( sidechain::sort_sigs( op.transaction, redeem_scripts, amounts, d.context_verify ) );

   for( size_t i = 0; i < new_stacks.size(); i++ ) {
      op.transaction.vin[i].scriptWitness = new_stacks[i];
   }

   sidechain::sign_witness_transaction_finalize( op.transaction, redeem_scripts );
}

void bitcoin_transaction_send_evaluator::send_bitcoin_transaction( const bitcoin_transaction_object& btc_tx )
{
   database& d = db();
   uint32_t skip = d.get_node_properties().skip_flags;
   if( !(skip & graphene::chain::database::skip_btc_tx_sending) && d.send_btc_tx_flag ){
      idump((btc_tx));
      d.send_btc_tx( btc_tx.transaction );
   }
}

void_result bitcoin_transaction_sign_evaluator::do_evaluate( const bitcoin_transaction_sign_operation& op )
{
   database& d = db();

   const auto& proposal_idx = d.get_index_type<proposal_index>().indices().get<by_id>();
   const auto& proposal_itr = proposal_idx.find( op.proposal_id );
   FC_ASSERT( proposal_idx.end() != proposal_itr, "proposal not found");

   const auto& witness_obj = d.get< witness_object >( d._current_witness_id );
   FC_ASSERT( witness_obj.witness_account == op.payer, "Incorrect witness." );

   sidechain::bytes public_key( public_key_data_to_bytes( witness_obj.signing_key.key_data ) );
   auto btc_send_op = proposal_itr->proposed_transaction.operations[0].get<bitcoin_transaction_send_operation>();

   auto vins = btc_send_op.vins;
   if( btc_send_op.pw_vin.identifier.str().compare( 0, 48, SIDECHAIN_NULL_VIN_IDENTIFIER ) != 0 ) {
      vins.insert( vins.begin(), btc_send_op.pw_vin );
   }

   FC_ASSERT( check_sigs( public_key, op.signatures, vins, btc_send_op.transaction ) );

   sidechain::sidechain_proposal_checker checker( d );
   FC_ASSERT( checker.check_witness_opportunity_to_approve( witness_obj, *proposal_itr ), "Can't sign this transaction" );

   return void_result();
}

void_result bitcoin_transaction_sign_evaluator::do_apply( const bitcoin_transaction_sign_operation& op )
{
   database& d = db();
   const auto& proposal = op.proposal_id( d );

   d.modify( proposal, [&]( proposal_object& po ) {
      auto bitcoin_transaction_send_op = po.proposed_transaction.operations[0].get<bitcoin_transaction_send_operation>();
      for( size_t i = 0; i < op.signatures.size(); i++ ) {
         bitcoin_transaction_send_op.transaction.vin[i].scriptWitness.push_back( op.signatures[i] );
      }
      po.proposed_transaction.operations[0] = bitcoin_transaction_send_op;
   });

   update_proposal( op );

   return void_result();
}

void bitcoin_transaction_sign_evaluator::update_proposal( const bitcoin_transaction_sign_operation& op )
{
   database& d = db();
   proposal_update_operation update_op;
   
   update_op.fee_paying_account = op.payer;
   update_op.proposal = op.proposal_id;
   update_op.active_approvals_to_add = { op.payer };

   bool skip_fee_old = trx_state->skip_fee;
   bool skip_fee_schedule_check_old = trx_state->skip_fee_schedule_check;
   trx_state->skip_fee = true;
   trx_state->skip_fee_schedule_check = true;

   d.apply_operation( *trx_state, update_op );

   trx_state->skip_fee = skip_fee_old;
   trx_state->skip_fee_schedule_check = skip_fee_schedule_check_old;
}

bool bitcoin_transaction_sign_evaluator::check_sigs( const bytes& key_data, const std::vector<bytes>& sigs,
                                                 const std::vector<sidechain::info_for_vin>& info_for_vins,
                                                 const bitcoin_transaction& tx )
{
   FC_ASSERT( sigs.size() == info_for_vins.size() && sigs.size() == tx.vin.size() );
   const auto& bitcoin_address_idx = db().get_index_type<bitcoin_address_index>().indices().get< by_address >();

   for( size_t i = 0; i < tx.vin.size(); i++ ) {
      const auto pbtc_address = bitcoin_address_idx.find( info_for_vins[i].address );
      const bytes& script = pbtc_address->address.get_redeem_script();

      const auto& sighash_str = get_signature_hash( tx, script, static_cast<int64_t>( info_for_vins[i].out.amount ), i, 1, true ).str();
      const bytes& sighash_hex = parse_hex( sighash_str );

      if( !verify_sig( sigs[i], key_data, sighash_hex, db().context_verify ) ) {
         return false;
      }

      size_t count_sigs = 0;
      for( auto& s : tx.vin[i].scriptWitness ) {
         if( verify_sig( s, key_data, sighash_hex, db().context_verify ) ) {
            count_sigs++;
         }
      }

      std::vector<bytes> pubkeys = get_pubkey_from_redeemScript( script );
      size_t count_pubkeys = std::count( pubkeys.begin(), pubkeys.end(), key_data );
      if( count_sigs >= count_pubkeys ) {
         return false;
      }

      uint32_t position = std::find( op_num.begin(), op_num.end(), script[0] ) - op_num.begin();
      if( !( position >= 0 && position < op_num.size() ) || tx.vin[i].scriptWitness.size() == position + 1 ) {
         return false;
      }
   }
   return true;
}

void_result bitcoin_transaction_revert_evaluator::do_evaluate( const bitcoin_transaction_revert_operation& op )
{ try {

   database& d = db();
   const auto& btc_trx_idx = d.get_index_type<bitcoin_transaction_index>().indices().get<by_transaction_id>();
   const auto& vouts_info_idx = d.get_index_type<info_for_vout_index>().indices().get<by_id>();
   const auto& vins_info_idx = d.get_index_type<info_for_used_vin_index>().indices().get<by_identifier>();

   for( auto trx_info: op.transactions_info ) {
      const auto& btc_itr = btc_trx_idx.find( trx_info.transaction_id );
      FC_ASSERT( btc_itr != btc_trx_idx.end() );   

      for( const auto& vout_id : btc_itr->vouts ) {
         FC_ASSERT( vouts_info_idx.find( vout_id ) != vouts_info_idx.end() );
      }
   
      for( const auto& vout_id : btc_itr->vins ) {
         FC_ASSERT( vins_info_idx.find( vout_id ) != vins_info_idx.end() );
      }
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result bitcoin_transaction_revert_evaluator::do_apply( const bitcoin_transaction_revert_operation& op )
{ try {

   database& d = db();
   const auto& btc_trx_idx = d.get_index_type<bitcoin_transaction_index>().indices().get<by_transaction_id>();
   const auto& vouts_info_idx = d.get_index_type<info_for_vout_index>().indices().get<by_id>();
   const auto& vins_info_idx = d.get_index_type<info_for_used_vin_index>().indices().get<by_identifier>();

   for( auto trx_info: op.transactions_info ) {
      const auto& btc_trx_obj = *btc_trx_idx.find( trx_info.transaction_id );
      d.pw_vout_manager.delete_vouts_after( btc_trx_obj.pw_vin );
      d.pw_vout_manager.mark_as_unused_vout( btc_trx_obj.pw_vin );
   
      for( const auto& vout_id : btc_trx_obj.vouts ) {
         const auto& vout_obj = *vouts_info_idx.find( vout_id );
         d.modify( vout_obj, [&]( info_for_vout_object& obj ){
            obj.used = false;
         });
      }
   
      for( const auto& vin_id : btc_trx_obj.vins ) {
         const auto& vin_obj = *vins_info_idx.find( vin_id );

         if( trx_info.valid_vins.count( fc::sha256 ( vin_obj.out.hash_tx ) ) ) {
            d.i_w_info.insert_info_for_vin( vin_obj.out, vin_obj.address, vin_obj.script, true );
         }
         d.remove( vin_obj );
      }
      d.bitcoin_confirmations.remove<sidechain::by_hash>( trx_info.transaction_id );
      d.remove( btc_trx_obj );
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

} }
