#include <sidechain/sidechain_proposal_checker.hpp>
#include <sidechain/sidechain_condensing_tx.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/primary_wallet_vout_object.hpp>
#include <graphene/chain/bitcoin_address_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/bitcoin_transaction_object.hpp>
#include <graphene/chain/info_for_used_vin_object.hpp>

namespace sidechain {

bool sidechain_proposal_checker::check_bitcoin_transaction_send_operation( const bitcoin_transaction_send_operation& op )
{
   bool info_for_pw_vin = check_info_for_pw_vin( op.pw_vin );
   bool info_for_vins = check_info_for_vins( op.vins );
   bool info_for_vouts = check_info_for_vouts( op.vouts );
   bool transaction = check_transaction( op );

   return info_for_pw_vin && info_for_vins && info_for_vouts && transaction;
}

bool sidechain_proposal_checker::check_reuse_pw_vin( const fc::sha256& pw_vin )
{
   const auto& pw_vin_status = pw_vin_ident.insert( pw_vin );
   if( !pw_vin_status.second ) {
      return false;
   }
   return true;
}

bool sidechain_proposal_checker::check_reuse_user_vins( const std::vector<fc::sha256>& user_vin_identifiers )
{
   for( const auto& vin : user_vin_identifiers ) {
      const auto& user_vin_status = user_vin_ident.insert( vin );
      if( !user_vin_status.second ) {
         return false;
      }
   }
   return true;
}

bool sidechain_proposal_checker::check_reuse_vouts( const std::vector<info_for_vout_id_type>& user_vout_ids )
{
   for( const auto& vout : user_vout_ids ) {
      const auto& user_vout_status = vout_ids.insert( vout );
      if( !user_vout_status.second ) {
         return false;
      }
   }
   return true;
}

bool sidechain_proposal_checker::check_reuse( const operation& op )
{
   fc::sha256 pw_vin_identifier;
   std::vector<fc::sha256> user_vin_identifiers;
   std::vector<info_for_vout_id_type> user_vout_ids;

   auto get_bto_tx_info = [ & ]( fc::sha256 trx_id ) {
      const auto& bto_itr_idx = db.get_index_type<bitcoin_transaction_index>().indices().get<graphene::chain::by_transaction_id>();
      const auto& bto_itr = bto_itr_idx.find( trx_id );
      if( bto_itr == bto_itr_idx.end() ) {
         return false;
      }
      pw_vin_identifier = bto_itr->pw_vin;
      user_vout_ids = bto_itr->vouts;
      user_vin_identifiers = bto_itr->vins;

      return true;
   };

   if( op.which() == operation::tag<bitcoin_transaction_send_operation>::value ) {
      bitcoin_transaction_send_operation btc_tx_send_op = op.get<bitcoin_transaction_send_operation>();
      pw_vin_identifier = btc_tx_send_op.pw_vin.identifier;
      user_vout_ids = btc_tx_send_op.vouts;
      for( const auto& vin : btc_tx_send_op.vins ) {
         user_vin_identifiers.push_back( vin.identifier );
      }
   } else if ( op.which() == operation::tag<bitcoin_issue_operation>::value ) {
      bitcoin_issue_operation btc_issue_op = op.get<bitcoin_issue_operation>();
      for( const auto& id : btc_issue_op.transaction_ids ) {
         if ( !get_bto_tx_info( id ) )
            return false;
      }
   } else if ( op.which() == operation::tag<bitcoin_transaction_revert_operation>::value ) {
      bitcoin_transaction_revert_operation btc_tx_revert_op = op.get<bitcoin_transaction_revert_operation>();
      for( auto trx_info: btc_tx_revert_op.transactions_info ) {
         if ( !get_bto_tx_info( trx_info.transaction_id ) )
            return false;
      }
   }

   return check_reuse_pw_vin( pw_vin_identifier ) &&
          check_reuse_user_vins( user_vin_identifiers ) &&
          check_reuse_vouts( user_vout_ids );
}

bool sidechain_proposal_checker::check_info_for_pw_vin( const info_for_vin& info_for_vin )
{
   const auto& prevout = db.pw_vout_manager.get_vout( info_for_vin.identifier );
   const auto& pw_address = db.get_latest_PW().address;
   if( !prevout.valid() || info_for_vin.out != prevout->vout ||
       info_for_vin.address != pw_address.get_address() || info_for_vin.script != pw_address.get_witness_script() ) {
      return false;
   }
   return true;
}

bool sidechain_proposal_checker::check_info_for_vins( const std::vector<info_for_vin>& info_for_vins )
{
   for( const auto& vin : info_for_vins ) {
      const auto& v = db.i_w_info.find_info_for_vin( vin.identifier );
      if( !v.valid() || *v != vin ) {
         return false;
      }
   }
   return true;
}

bool sidechain_proposal_checker::check_info_for_vouts( const std::vector<info_for_vout_id_type>& info_for_vout_ids )
{
   const auto& info_for_vout_idx = db.get_index_type<info_for_vout_index>().indices().get<graphene::chain::by_id>();
   for( const auto& id : info_for_vout_ids ) {
      const auto& itr = info_for_vout_idx.find( id );
      if( itr == info_for_vout_idx.end() ) {
         return false;
      }
   }
   return true;
}

bool sidechain_proposal_checker::check_transaction( const bitcoin_transaction_send_operation& btc_trx_op )
{
   std::vector<info_for_vout> info_vouts;
   const auto& info_for_vout_idx = db.get_index_type<info_for_vout_index>().indices().get<graphene::chain::by_id>();
   for( const auto& vout_id : btc_trx_op.vouts ) {
      const auto& vout_itr = info_for_vout_idx.find( vout_id );
      if( vout_itr == info_for_vout_idx.end() ) {
         return false;
      }
      info_vouts.push_back( *vout_itr );
   }

   const auto& temp_full_tx = db.create_btc_transaction( btc_trx_op.vins, info_vouts, btc_trx_op.pw_vin );

   if( temp_full_tx.first != btc_trx_op.transaction || temp_full_tx.second != btc_trx_op.fee_for_size ) {
      return false;
   }

   return true;
}

bool sidechain_proposal_checker::check_witness_opportunity_to_approve( const witness_object& current_witness, const proposal_object& proposal )
{
   auto is_active_witness = [ & ]() {
      return db.get_global_properties().active_witnesses.find( current_witness.id ) != db.get_global_properties().active_witnesses.end();
   };

   // Checks can witness approve this proposal or not
   auto does_the_witness_have_authority = [ & ]() {
      const auto& accounts_index = db.get_index_type<account_index>().indices().get<graphene::chain::by_id>();
      auto account_pBTC_issuer = accounts_index.find( db.get_sidechain_account_id() );

      return account_pBTC_issuer->owner.account_auths.count( current_witness.witness_account ) &&
             !proposal.available_active_approvals.count( current_witness.witness_account );
   };

   return is_active_witness() && does_the_witness_have_authority();
}

bool sidechain_proposal_checker::check_witnesses_keys( const witness_object& current_witness, const proposal_object& proposal ) 
{
   bitcoin_transaction_send_operation op = proposal.proposed_transaction.operations.back().get<bitcoin_transaction_send_operation>();

   auto vins = op.vins;
   if( op.pw_vin.identifier.str().compare( 0, 48, SIDECHAIN_NULL_VIN_IDENTIFIER ) != 0 ) {
      vins.insert( vins.begin(), op.pw_vin );
   }

   const auto& bitcoin_address_idx = db.get_index_type<bitcoin_address_index>().indices().get< by_address >();
   
   for ( const auto& info_vins : vins ) {
      const auto& address_itr = bitcoin_address_idx.find( info_vins.address );
      if ( address_itr != bitcoin_address_idx.end() ) {
         const auto& witness_key = current_witness.signing_key;
         const auto& witnesses_keys = address_itr->address.witnesses_keys;
         const auto& witnesses_keys_itr = witnesses_keys.find( current_witness.witness_account );
         if ( witnesses_keys_itr == witnesses_keys.end() || witnesses_keys_itr->second != witness_key ) {
            return false;
         }
      }
   }

   return true;
}

bool sidechain_proposal_checker::check_bitcoin_transaction_revert_operation( const bitcoin_transaction_revert_operation& op )
{
   const auto& btc_trx_idx = db.get_index_type<bitcoin_transaction_index>().indices().get<by_transaction_id>();

   for( auto trx_info: op.transactions_info )
   {
      auto value = db.bitcoin_confirmations.find<sidechain::by_hash>( trx_info.transaction_id );
      if( !value.valid() ||
          btc_trx_idx.find( value->transaction_id ) == btc_trx_idx.end() ||
          trx_info != revert_trx_info( value->transaction_id, value->valid_vins ) )
         return false;
   }

   return !op.transactions_info.empty();
}

}
