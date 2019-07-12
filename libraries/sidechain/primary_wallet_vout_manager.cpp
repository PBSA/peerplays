#include <sidechain/primary_wallet_vout_manager.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/primary_wallet_vout_object.hpp>
#include <graphene/chain/config.hpp>


namespace sidechain {

bool primary_wallet_vout_manager::is_max_vouts() const
{
   const auto& PW_vout_idx = db.get_index_type<graphene::chain::primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   return !( ( PW_vout_idx.size() - 1 ) <= db.get_sidechain_params().maximum_unconfirmed_vouts );
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
   const auto& next_pw_id = db.get_index_type<graphene::chain::primary_wallet_vout_index>().get_next_id().instance();
   db.create<primary_wallet_vout_object>([&]( primary_wallet_vout_object& obj ) {
      obj.vout = out;
      obj.hash_id = create_hash_id( out.hash_tx, out.n_vout, next_pw_id );
      obj.confirmed = false;
      obj.used = false;
   });
}

void primary_wallet_vout_manager::delete_vouts_after( fc::sha256 hash_id )
{
   const auto& PW_vout_by_id = db.get_index_type<graphene::chain::primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   auto vout_id = get_vout_id( hash_id );
   if( !vout_id.valid() )
      return;

   auto itr = PW_vout_by_id.find( *vout_id );
   itr++;

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

void primary_wallet_vout_manager::mark_as_used_vout( fc::sha256 hash_id )
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

void primary_wallet_vout_manager::mark_as_unused_vout( fc::sha256 hash_id )
{
   const auto& PW_vout_by_id = db.get_index_type<graphene::chain::primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   auto vout_id = get_vout_id( hash_id );
   FC_ASSERT( vout_id.valid() );

   auto itr = PW_vout_by_id.find( *vout_id );

   db.modify(*itr, [&]( primary_wallet_vout_object& PW_vout ) {
      PW_vout.used = false;
   });
}

fc::optional< graphene::chain::primary_wallet_vout_id_type > primary_wallet_vout_manager::get_vout_id( fc::sha256 hash_id ) const
{
   const auto& PW_vout_by_hash_id = db.get_index_type<graphene::chain::primary_wallet_vout_index>().indices().get< graphene::chain::by_hash_id >();
   const auto& itr_hash_id = PW_vout_by_hash_id.find( hash_id );
   if( itr_hash_id == PW_vout_by_hash_id.end() )
      return fc::optional< graphene::chain::primary_wallet_vout_id_type >();
   return fc::optional< graphene::chain::primary_wallet_vout_id_type >( itr_hash_id->get_id() );
}

fc::sha256 primary_wallet_vout_manager::create_hash_id( const std::string& hash_tx, const uint32_t& n_vout, const uint64_t& id ) const
{
   std::stringstream ss;
   ss << std::hex << id;
   std::string id_hex = std::string( 16 - ss.str().size(), '0' ) + ss.str();

   std::string hash_str = fc::sha256::hash( hash_tx + std::to_string( n_vout ) ).str();
   std::string final_hash_id = std::string( hash_str.begin(), hash_str.begin() + 48 ) + id_hex;

   return fc::sha256( final_hash_id );
}

}