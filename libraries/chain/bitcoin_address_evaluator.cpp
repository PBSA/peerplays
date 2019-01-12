#include <graphene/chain/bitcoin_address_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/bitcoin_address_object.hpp>

namespace graphene { namespace chain {

void_result bitcoin_address_create_evaluator::do_evaluate( const bitcoin_address_create_operation& op )
{
   database& d = db();
   auto& acc_idx = d.get_index_type<account_index>().indices().get<by_id>();
   auto acc_itr = acc_idx.find( op.owner );
   FC_ASSERT( acc_itr != acc_idx.end() );
   return void_result();
}

object_id_type bitcoin_address_create_evaluator::do_apply( const bitcoin_address_create_operation& op )
{
   database& d = db();

   const auto& new_btc_address = d.create<bitcoin_address_object>( [&]( bitcoin_address_object& a ) {
      a.owner = op.owner;
      //a.address = sidechain::btc_multisig_segwit_address();
      a.count_invalid_pub_key = 1;
   });

   return new_btc_address.id;
}

public_key_type bitcoin_address_create_evaluator::pubkey_from_id( object_id_type id )
{
   fc::ecc::public_key_data pubkey_data;

   pubkey_data.at( 0 ) = 0x02; // version
   const auto hash = fc::sha256::hash( std::to_string( id.instance() ) ) ;
   std::copy( hash.data(), hash.data() + hash.data_size(), pubkey_data.begin() + 1 );

   return public_key_type( pubkey_data );
}
} } // namespace graphene::chain
