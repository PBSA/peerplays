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
   sidechain::bytes wit_script;

   finalize_bitcoin_transaction( mutable_op );

   auto new_vins = create_transaction_vins( mutable_op );

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

   sidechain::prev_out new_pw_vout = { "", 0, 0 };
   if( btc_tx.transaction.vout[0].is_p2wsh() && btc_tx.transaction.vout[0].scriptPubKey == wit_script )
      new_pw_vout = { btc_tx.transaction.get_txid(), 0, btc_tx.transaction.vout[0].value };

   d.pw_vout_manager.create_new_vout( new_pw_vout );
   send_bitcoin_transaction( btc_tx );

   return btc_tx.id;
}

std::vector< info_for_used_vin_id_type > bitcoin_transaction_send_evaluator::create_transaction_vins( bitcoin_transaction_send_operation& op )
{
   database& d = db();
   std::vector< info_for_used_vin_id_type > new_vins;

   for( auto itr : op.vins ){
      auto itr_id = d.create< info_for_used_vin_object >( [&]( info_for_used_vin_object& obj )
      {
         obj.identifier = itr.identifier;
         obj.out = itr.out;
         obj.address = itr.address;
         obj.script = itr.script;
      }).get_id();
      new_vins.push_back( itr_id );

      auto obj_itr = d.i_w_info.find_info_for_vin( itr.identifier );
      if( obj_itr.valid() )
         d.i_w_info.remove_info_for_vin( *obj_itr );
   }
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

void_result bitcoin_transaction_sign_evaluator::do_evaluate( const bitcoin_transaction_sign_operation& op )
{
   database& d = db();

   const auto& sidechain_proposal_idx = d.get_index_type<sidechain_proposal_index>().indices().get<by_id>();
   const auto& sidechain_proposal_itr = sidechain_proposal_idx.find( op.sidechain_proposal_id );
   FC_ASSERT( sidechain_proposal_idx.end() != sidechain_proposal_itr,
      "sidechain_proposal not found");

   witness_id_type scheduled_witness = d.get_scheduled_witness( 1 );
   const auto& witness_obj = d.get< witness_object >( scheduled_witness );
   FC_ASSERT( witness_obj.witness_account == op.payer, "Incorrect witness." );

   sidechain::bytes public_key( public_key_data_to_bytes( witness_obj.signing_key.key_data ) );
   const auto& proposal = sidechain_proposal_itr->proposal_id( d );
   auto btc_send_op = proposal.proposed_transaction.operations[0].get<bitcoin_transaction_send_operation>();
   FC_ASSERT( check_sigs( public_key, op.signatures, btc_send_op.vins, btc_send_op.transaction ) ); // Add pw_vin

   // const auto& proposal = sidechain_proposal_itr->proposal_id( d );
   // FC_ASSERT( d.check_witness( witness_obj, *btc_tx, proposal ), "Can't sign this transaction" );

   return void_result();
}

void_result bitcoin_transaction_sign_evaluator::do_apply( const bitcoin_transaction_sign_operation& op )
{
   database& d = db();

   const auto& sidechain_proposal = op.sidechain_proposal_id( d );
   const auto& proposal = sidechain_proposal.proposal_id( d );

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
   update_op.proposal = op.sidechain_proposal_id( d ).proposal_id;
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

   for( size_t i = 0; i < tx.vin.size(); i++ ) {
      const bytes& script = info_for_vins[i].script;
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

} }
