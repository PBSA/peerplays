#include <sidechain/primary_wallet_vout_manager.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/primary_wallet_vout_object.hpp>
#include <graphene/chain/config.hpp>


namespace sidechain {

bool primary_wallet_vout_manager::is_reach_max_unconfirmaed_vout() const
{
   const auto& PW_vout_idx = db.get_index_type<graphene::chain::primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   return !( PW_vout_idx.size() < SIDECHAIN_DEFAULT_NUMBER_UNCONFIRMED_VINS );
}

fc::optional< primary_wallet_vout_object > primary_wallet_vout_manager::get_latest_unused_vout() const
{
   const auto& PW_vout_idx = db.get_index_type<graphene::chain::primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   auto itr = PW_vout_idx.end();
   FC_ASSERT( itr != PW_vout_idx.begin() );

   itr--;
   if( itr->used )
      return fc::optional< primary_wallet_vout_object >();
   return fc::optional< primary_wallet_vout_object > (*itr);
}

fc::optional< primary_wallet_vout_object > primary_wallet_vout_manager::get_vout( fc::sha256 hash_id ) const
{
   const auto& PW_vout_by_hash_id = db.get_index_type<graphene::chain::primary_wallet_vout_index>().indices().get< graphene::chain::by_hash_id >();
   const auto& itr_hash_id = PW_vout_by_hash_id.find( hash_id );
   if( itr_hash_id == PW_vout_by_hash_id.end() )
      return fc::optional< primary_wallet_vout_object >();
   return fc::optional< primary_wallet_vout_object >( *itr_hash_id );
}

void primary_wallet_vout_manager::create_new_vout( const sidechain::prev_out& out )
{
   db.create<primary_wallet_vout_object>([&]( primary_wallet_vout_object& obj ) {
      obj.vout = out;
      obj.hash_id = fc::sha256::hash( out.hash_tx + std::to_string( out.n_vout ) );
      obj.confirmed = false;
      obj.used = false;
   });
}

void primary_wallet_vout_manager::delete_vout_with_newer( fc::sha256 hash_id )
{
   const auto& PW_vout_by_id = db.get_index_type<graphene::chain::primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   auto vout_id = get_vout_id( hash_id );
   if( !vout_id.valid() )
      return;

   auto itr = PW_vout_by_id.find( *vout_id );

   while( itr != PW_vout_by_id.end() )
   {
      auto temp = itr;
      itr++;
      db.remove( *temp );
   }
}

void primary_wallet_vout_manager::confirm_vout( fc::sha256 hash_id )
{
   const auto& PW_vout_by_id = db.get_index_type<graphene::chain::primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   auto vout_id = get_vout_id( hash_id );
   FC_ASSERT( vout_id.valid() );

   auto itr = PW_vout_by_id.find( *vout_id );

   db.modify(*itr, [&]( primary_wallet_vout_object& PW_vout ) {
      PW_vout.confirmed = true;
   });

   if( itr != PW_vout_by_id.begin() ){
      itr--;
      FC_ASSERT( itr->confirmed == true );
      db.remove(*itr);
   }
}

void primary_wallet_vout_manager::use_latest_vout( fc::sha256 hash_id )
{
   const auto& PW_vout_by_id = db.get_index_type<graphene::chain::primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   auto vout_id = get_vout_id( hash_id );
   FC_ASSERT( vout_id.valid() );

   auto itr = PW_vout_by_id.find( *vout_id );

   db.modify(*itr, [&]( primary_wallet_vout_object& PW_vout ) {
      PW_vout.used = true;
   });

   if( itr != PW_vout_by_id.begin() ){
      itr--;
      FC_ASSERT( itr->used == true );
   }
}

fc::optional< graphene::chain::primary_wallet_vout_id_type > primary_wallet_vout_manager::get_vout_id( fc::sha256 hash_id ) const
{
   const auto& PW_vout_by_hash_id = db.get_index_type<graphene::chain::primary_wallet_vout_index>().indices().get< graphene::chain::by_hash_id >();
   const auto& itr_hash_id = PW_vout_by_hash_id.find( hash_id );
   if( itr_hash_id == PW_vout_by_hash_id.end() )
      return fc::optional< graphene::chain::primary_wallet_vout_id_type >();
   return fc::optional< graphene::chain::primary_wallet_vout_id_type >( itr_hash_id->get_id() );
}

}