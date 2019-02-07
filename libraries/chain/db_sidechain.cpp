#include <graphene/chain/database.hpp>
#include <graphene/chain/bitcoin_address_object.hpp>
#include <graphene/chain/sidechain_proposal_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/protocol/asset.hpp>
#include <graphene/chain/bitcoin_transaction_object.hpp>

#include <sidechain/sidechain_condensing_tx.hpp>
#include <sidechain/sign_bitcoin_transaction.hpp>
#include <sidechain/sidechain_proposal_checker.hpp>

using namespace sidechain;

namespace graphene { namespace chain {

std::map< account_id_type, public_key_type> database::get_active_witnesses_keys() const
{
   const auto& witnesses_by_id = get_index_type<witness_index>().indices().get<by_id>();
   std::map< account_id_type, public_key_type > witnesses_keys;
   auto& active_witnesses = get_global_properties().active_witnesses;
   for( auto witness_id : active_witnesses ) {
      const auto& witness_obj = witnesses_by_id.find( witness_id );
      if( witness_obj != witnesses_by_id.end() ){
         witnesses_keys.emplace( witness_obj->witness_account, witness_obj->signing_key );
      }
   }
   return witnesses_keys;
}

bool database::is_sidechain_fork_needed() const
{
   const auto& params = get_global_properties().parameters.extensions.value.sidechain_parameters;
   return !params.valid();
}

void database::perform_sidechain_fork()
{
   const auto& sidechain_account = create<account_object>( [&]( account_object& obj ) {
      obj.name = "sidechain_account";
      obj.statistics = create<account_statistics_object>([&]( account_statistics_object& acc_stat ){ acc_stat.owner = obj.id; }).id;
      obj.owner.weight_threshold = 5;
      obj.active.weight_threshold = 5;
      obj.membership_expiration_date = time_point_sec::maximum();
      obj.network_fee_percentage = GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
      obj.lifetime_referrer_fee_percentage = GRAPHENE_100_PERCENT - GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
   });

   const asset_object& new_asset = create<asset_object>( [&]( asset_object& obj ) {
      obj.symbol = SIDECHAIN_SYMBOL;
      obj.precision = SIDECHAIN_PRECISION_DIGITS;
      obj.issuer = sidechain_account.get_id();
      obj.options.max_supply = SIDECHAIN_MAX_SHARE_SUPPLY;
      obj.options.issuer_permissions = 0;
      obj.options.flags = 0;
      obj.dynamic_asset_data_id = create<asset_dynamic_data_object>([&]( asset_dynamic_data_object& a ) { a.current_supply = 0; }).id;
   });

   modify( get_global_properties(), [&]( global_property_object& gpo ) {
      sidechain_parameters_extension params_ext;
      params_ext.managing_account = sidechain_account.get_id();
      params_ext.asset_id = new_asset.get_id();
      
      gpo.parameters.extensions.value.sidechain_parameters = params_ext;
      if( gpo.pending_parameters )
         gpo.pending_parameters->extensions.value.sidechain_parameters = params_ext;
   });

   auto global_properties = get_global_properties();
   const auto& witnesses_idx = get_index_type<witness_index>().indices().get<by_id>();
   std::vector<account_id_type> witness_accounts;

   for( auto witness_id : global_properties.active_witnesses ) {
      const auto& witness_obj = witnesses_idx.find( witness_id );
      if( witness_obj != witnesses_idx.end() )
         witness_accounts.push_back( witness_obj->witness_account );
   }

   modify( sidechain_account, [&]( account_object& obj ) {
      for( auto& a : witness_accounts ) {
         obj.owner.add_authority( a, 1 );
         obj.active.add_authority( a, 1 );
      }
   });

   create<bitcoin_address_object>( [&]( bitcoin_address_object& pw ) {   // Create PW address
      pw.address = btc_multisig_segwit_address( 5, get_active_witnesses_keys() );
      pw.owner = sidechain_account.get_id();
      pw.count_invalid_pub_key = 1;
   });

   pw_vout_manager.create_new_vout( {"", 0, 0 } );
}

const sidechain_parameters_extension& database::get_sidechain_params() const
{
   const auto& params = get_global_properties().parameters.extensions.value.sidechain_parameters;
   FC_ASSERT( params.valid() );
   return *params;
}

const account_id_type& database::get_sidechain_account_id() const
{
   return get_sidechain_params().managing_account;
}

const asset_id_type& database::get_sidechain_asset_id() const
{
   return get_sidechain_params().asset_id;
}

bitcoin_address_object database::get_latest_PW() const
{
   const auto& btc_addr_idx = get_index_type<bitcoin_address_index>().indices().get<by_owner>();
   auto itr = btc_addr_idx.upper_bound( get_sidechain_account_id() );
   return *(--itr);
}

int64_t database::get_estimated_fee( size_t tx_vsize, uint64_t estimated_feerate ) {
   static const uint64_t default_feerate = 1000;
   static const uint64_t min_relay_fee = 1000;

   const auto feerate = std::max( default_feerate, estimated_feerate );
   const auto fee = feerate * int64_t( tx_vsize ) / 1000;

   return std::max( min_relay_fee, fee );
}

void database::processing_sidechain_proposals( const witness_object& current_witness, const fc::ecc::private_key& private_key )
{
   const auto& sidechain_proposal_idx = get_index_type<sidechain_proposal_index>().indices().get< by_id >();
   const auto& proposal_idx = get_index_type<proposal_index>().indices().get< by_id >();

   sidechain_proposal_checker checker( *this );

   for( auto& sidechain_proposal : sidechain_proposal_idx ) {

      const auto& proposal = proposal_idx.find( sidechain_proposal.proposal_id );
      FC_ASSERT( proposal != proposal_idx.end() );

      if( !checker.check_reuse( proposal->proposed_transaction.operations.back() ) ) {
         continue;
      }

      switch( sidechain_proposal.proposal_type ) {
         case sidechain_proposal_type::ISSUE_BTC :{ 
            proposal_update_operation puo;
            puo.fee_paying_account = current_witness.witness_account;
            puo.proposal = proposal->id;
            puo.active_approvals_to_add = { current_witness.witness_account };
            _pending_tx.insert( _pending_tx.begin(), create_signed_transaction( private_key, puo ) );
            break;
          }
         case sidechain_proposal_type::SEND_BTC_TRANSACTION :{
            bitcoin_transaction_send_operation op = proposal->proposed_transaction.operations.back().get<bitcoin_transaction_send_operation>();
            if( checker.check_bitcoin_transaction_send_operation( op ) && checker.check_witness_opportunity_to_approve( current_witness, *proposal ) )
            {
               const auto& sign_operation = create_sign_btc_tx_operation( current_witness, private_key, proposal->id );
               _pending_tx.insert( _pending_tx.begin(), create_signed_transaction( private_key, sign_operation ) );
            }
            break;
         }
         case sidechain_proposal_type::RETURN_PBTC_BACK :{}
      }
   }
}

full_btc_transaction database::create_btc_transaction( const std::vector<info_for_vin>& info_vins,
                                                       const std::vector<info_for_vout>& info_vouts,
                                                       const info_for_vin& info_pw_vin )
{
   sidechain_condensing_tx ctx( info_vins, info_vouts );


   if( info_pw_vin.identifier.str().compare( 0, 48, SIDECHAIN_NULL_VIN_IDENTIFIER ) != 0 ) {
      ctx.create_pw_vin( info_pw_vin );
   }

   const auto& pw_address = get_latest_PW().address;
   if( info_vouts.size() > 0 ) {
      ctx.create_vouts_for_witness_fee( pw_address.witnesses_keys );
   }

   const uint64_t& change = ctx.get_amount_vins() - ctx.get_amount_transfer_to_bitcoin();
   if( change > 0 ) {
      ctx.create_pw_vout( change, pw_address.get_witness_script() );
   }

   const uint64_t& size_fee = get_estimated_fee( sidechain_condensing_tx::get_estimate_tx_size( ctx.get_transaction(), pw_address.witnesses_keys.size() ), estimated_feerate.load() );
   ctx.subtract_fee( size_fee, get_sidechain_params().percent_payment_to_witnesses );

   return std::make_pair( ctx.get_transaction(), size_fee );
}

fc::optional<operation> database::create_send_btc_tx_proposal( const witness_object& current_witness )
{
   const auto& info_vins = i_w_info.get_info_for_vins();
   const auto& info_vouts = i_w_info.get_info_for_vouts();
   const auto& info_pw_vin = i_w_info.get_info_for_pw_vin();

   if( info_pw_vin.valid() && ( info_vins.size() || info_vouts.size() ) ) {
      const auto& btc_tx_and_size_fee = create_btc_transaction( info_vins, info_vouts, *info_pw_vin );

      bitcoin_transaction_send_operation btc_send_op;
      btc_send_op.payer = get_sidechain_account_id();
      btc_send_op.pw_vin = *info_pw_vin;
      btc_send_op.vins = info_vins;
      for( auto& out : info_vouts ) {
         btc_send_op.vouts.push_back( out.get_id() );
      }
      btc_send_op.transaction = btc_tx_and_size_fee.first;
      btc_send_op.fee_for_size = btc_tx_and_size_fee.second;

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = current_witness.witness_account;
      proposal_op.proposed_ops.push_back( op_wrapper( btc_send_op ) );
      uint32_t lifetime = ( get_global_properties().parameters.block_interval * get_global_properties().active_witnesses.size() ) * 3;
      proposal_op.expiration_time = time_point_sec( head_block_time().sec_since_epoch() + lifetime );
      return proposal_op;
   }
   return fc::optional<operation>();
}

signed_transaction database::create_signed_transaction( const private_key& signing_private_key, const operation& op )
{
   signed_transaction processed_trx;
   auto dyn_props = get_dynamic_global_properties();
   processed_trx.set_reference_block( dyn_props.head_block_id );
   processed_trx.set_expiration( head_block_time() + get_global_properties().parameters.maximum_time_until_expiration );
   processed_trx.operations.push_back( op );
   current_fee_schedule().set_fee( processed_trx.operations.back() );

   processed_trx.sign( signing_private_key, get_chain_id() );

   return processed_trx;
}

operation database::create_sign_btc_tx_operation( const witness_object& current_witness, const private_key_type& privkey,
                                                  const proposal_id_type& proposal_id )
{
   const auto& proposal_idx = get_index_type<proposal_index>().indices().get<by_id>();
   const auto& proposal_itr = proposal_idx.find( proposal_id );
   bitcoin_transaction_send_operation op = proposal_itr->proposed_transaction.operations.back().get<bitcoin_transaction_send_operation>();

   bitcoin_transaction_sign_operation sign_operation;
   sign_operation.payer = current_witness.witness_account;
   sign_operation.proposal_id = proposal_id;
   const auto secret = privkey.get_secret();
   bytes key(secret.data(), secret.data() + secret.data_size());

   auto vins = op.vins;
   if( op.pw_vin.identifier.str().compare( 0, 48, SIDECHAIN_NULL_VIN_IDENTIFIER ) != 0 ) {
      vins.insert( vins.begin(), op.pw_vin );
   }

   std::vector<bytes> redeem_scripts( i_w_info.get_redeem_scripts( vins ) );
   std::vector<uint64_t> amounts( i_w_info.get_amounts( vins ) );

   sign_operation.signatures = sign_witness_transaction_part( op.transaction, redeem_scripts, amounts, key, context_sign, 1 );

   return sign_operation;
}

void database::remove_sidechain_proposal_object( const proposal_object& proposal )
{ try {
   if( proposal.proposed_transaction.operations.size() == 1 &&
     ( proposal.proposed_transaction.operations.back().which() == operation::tag<bitcoin_transaction_send_operation>::value  || 
       proposal.proposed_transaction.operations.back().which() == operation::tag<bitcoin_issue_operation>::value ) )
   {
      const auto& sidechain_proposal_idx = get_index_type<sidechain_proposal_index>().indices().get<by_proposal>();
      auto sidechain_proposal_itr = sidechain_proposal_idx.find( proposal.id );
      if( sidechain_proposal_itr == sidechain_proposal_idx.end() ) {
         return;
      }
      remove( *sidechain_proposal_itr );
   }
} FC_CAPTURE_AND_RETHROW( (proposal) ) }

void database::roll_back_vin_and_vout( const proposal_object& proposal )
{
   if( proposal.proposed_transaction.operations.size() == 1 &&
       proposal.proposed_transaction.operations.back().which() == operation::tag<bitcoin_transaction_send_operation>::value )
   {
      bitcoin_transaction_send_operation op = proposal.proposed_transaction.operations.back().get<bitcoin_transaction_send_operation>();

      if( pw_vout_manager.get_vout( op.pw_vin.identifier ).valid() ) {
         pw_vout_manager.mark_as_unused_vout( op.pw_vin.identifier );
      }

      for( const auto& vin : op.vins ) {
         const auto& v = i_w_info.find_info_for_vin( vin.identifier );
         if( v.valid() ) {
            i_w_info.mark_as_unused_vin( vin );
         }
      }

      for( const auto& vout : op.vouts ) {
         const auto& v = i_w_info.find_info_for_vout( vout );
         if( v.valid() ) {
            i_w_info.mark_as_unused_vout( *v );
         }
      }

      remove_sidechain_proposal_object( proposal );
   }
}

fc::optional<operation> database::create_bitcoin_issue_proposals( const witness_object& current_witness )
{
   std::vector<fc::sha256> trx_ids;
   bitcoin_confirmations.safe_for<by_confirmed_and_not_used>([&]( btc_tx_confirmations_index::index<by_confirmed_and_not_used>::type::iterator itr_b, btc_tx_confirmations_index::index<by_confirmed_and_not_used>::type::iterator itr_e ){
      for(auto iter = itr_b; iter != itr_e; iter++) {
         if( !iter->is_confirmed_and_not_used() ) return;

         const auto& btc_trx_idx = get_index_type<bitcoin_transaction_index>().indices().get<by_transaction_id>();
         const auto& btc_tx = btc_trx_idx.find( iter->transaction_id );
         if( btc_tx == btc_trx_idx.end() ) continue;
         trx_ids.push_back( iter->transaction_id );
      }
   });

   if( trx_ids.size() ) {
      bitcoin_issue_operation issue_op;
      issue_op.payer = get_sidechain_account_id();
      issue_op.transaction_ids = trx_ids;

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = current_witness.witness_account;
      proposal_op.proposed_ops.push_back( op_wrapper( issue_op ) );
      uint32_t lifetime = ( get_global_properties().parameters.block_interval * get_global_properties().active_witnesses.size() ) * 3;
      proposal_op.expiration_time = time_point_sec( head_block_time().sec_since_epoch() + lifetime );

      return fc::optional<operation>( proposal_op );
   }

   return fc::optional<operation>();
}

} }