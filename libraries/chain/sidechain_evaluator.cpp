#include <graphene/chain/sidechain_evaluator.hpp>
#include <graphene/chain/bitcoin_transaction_object.hpp>
#include <graphene/chain/info_for_used_vin_object.hpp>
#include <graphene/chain/bitcoin_address_object.hpp>

namespace graphene { namespace chain {

void_result bitcoin_issue_evaluator::do_evaluate( const bitcoin_issue_operation& op )
{ try {
   database& d = db();

   const auto& btc_trx_idx = d.get_index_type<bitcoin_transaction_index>().indices().get<by_transaction_id>();
   const auto& btc_addr_idx = d.get_index_type<bitcoin_address_index>().indices().get<by_address>();
   const auto& vins_info_idx = d.get_index_type<info_for_used_vin_index>().indices().get<by_identifier>();
   const auto& vouts_info_idx = d.get_index_type<info_for_vout_index>().indices().get<by_id>();
   FC_ASSERT( op.payer == db().get_sidechain_account_id() );

   for( const auto& id: op.transaction_ids ) {
      const auto& btc_itr = btc_trx_idx.find( id );
      FC_ASSERT( btc_itr != btc_trx_idx.end() );

      for( auto& vin_id : btc_itr->vins ) {
         const auto& itr = vins_info_idx.find( vin_id );
         FC_ASSERT( itr != vins_info_idx.end() );
         auto addr_itr = btc_addr_idx.find( itr->address );
         FC_ASSERT( addr_itr != btc_addr_idx.end() );
      }
      for( auto& vout_id : btc_itr->vouts )
         FC_ASSERT( vouts_info_idx.find( vout_id ) != vouts_info_idx.end() );
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result bitcoin_issue_evaluator::do_apply( const bitcoin_issue_operation& op )
{ try {
   database& d = db();
   const auto& btc_trx_idx = d.get_index_type<bitcoin_transaction_index>().indices().get<by_transaction_id>();

   for( const auto& id: op.transaction_ids ) {
      const auto& btc_obj = *btc_trx_idx.find( id );
      add_issue( btc_obj );

      d.pw_vout_manager.confirm_vout( btc_obj.pw_vin );
      clear_btc_transaction_information( btc_obj );
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void bitcoin_issue_evaluator::add_issue( const bitcoin_transaction_object& btc_obj )
{
   database& d = db();

   const auto& accounts_to_issue = get_accounts_to_issue( btc_obj.vins );
   const auto& amounts_to_issue = get_amounts_to_issue( btc_obj.vins );

   uint64_t fee_deduction = btc_obj.fee_for_size / ( btc_obj.vins.size() + btc_obj.vouts.size() );

   if( btc_obj.fee_for_size % ( btc_obj.vins.size() + btc_obj.vouts.size() ) != 0 ) {
      fee_deduction += 1;
   }

   bool skip_fee_old = trx_state->skip_fee;
   bool skip_fee_schedule_check_old = trx_state->skip_fee_schedule_check;
   trx_state->skip_fee = true;
   trx_state->skip_fee_schedule_check = true;

   for( size_t i = 0; i < accounts_to_issue.size(); i++ ){
      asset_issue_operation issue_op;
      issue_op.issuer = d.get_sidechain_account_id();
      issue_op.asset_to_issue = asset( amounts_to_issue[i] - fee_deduction, d.get_sidechain_asset_id() );
      issue_op.issue_to_account = accounts_to_issue[i];

      d.apply_operation( *trx_state, issue_op );
   }

   trx_state->skip_fee = skip_fee_old;
   trx_state->skip_fee_schedule_check = skip_fee_schedule_check_old;
}

void bitcoin_issue_evaluator::clear_btc_transaction_information( const bitcoin_transaction_object& btc_obj )
{
   database& d = db();
   const auto& vins_info_idx = d.get_index_type<info_for_used_vin_index>().indices().get<by_identifier>();
   const auto& vouts_info_idx = d.get_index_type<info_for_vout_index>().indices().get<by_id>();

   for( auto& vin_id : btc_obj.vins ) {
      auto vin_itr = vins_info_idx.find( vin_id );
      d.remove( *vin_itr );
   }

   for( auto& vout_id : btc_obj.vouts ) {
      auto vout_itr = vouts_info_idx.find( vout_id );
      d.remove( *vout_itr );
   }

   auto trx_approvals = d.bitcoin_confirmations.find<sidechain::by_hash>( btc_obj.transaction_id );
   if( trx_approvals.valid() ) {
      d.bitcoin_confirmations.remove<sidechain::by_hash>( btc_obj.transaction_id );
   }

   d.remove( btc_obj );
}

std::vector<uint64_t> bitcoin_issue_evaluator::get_amounts_to_issue( std::vector<fc::sha256> vins_identifier )
{
   database& d = db();
   const auto& vins_info_idx = d.get_index_type<info_for_used_vin_index>().indices().get<by_identifier>();

   std::vector<uint64_t> result;

   for( auto& identifier : vins_identifier ) {
      auto vin_itr = vins_info_idx.find( identifier );
      result.push_back( vin_itr->out.amount );
   }

   return result;
}

std::vector<account_id_type> bitcoin_issue_evaluator::get_accounts_to_issue( std::vector<fc::sha256> vins_identifier )
{
   database& d = db();
   const auto& btc_addr_idx = d.get_index_type<bitcoin_address_index>().indices().get<by_address>();
   const auto& vins_info_idx = d.get_index_type<info_for_used_vin_index>().indices().get<by_identifier>();

   std::vector<account_id_type> result;
   
   for( auto& identifier : vins_identifier ) {
      auto vin_itr = vins_info_idx.find( identifier );
      auto addr_itr = btc_addr_idx.find( vin_itr->address );

      result.push_back( addr_itr->owner );
   }

   return result;
}



} } // graphene::chain