#include <graphene/chain/database.hpp>
#include <graphene/chain/bitcoin_address_object.hpp>

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
}

const sidechain::sidechain_parameters_extension& database::get_sidechain_params() const
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


} }