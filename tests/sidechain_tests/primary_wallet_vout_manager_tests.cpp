#include <boost/test/unit_test.hpp>
#include <sidechain/primary_wallet_vout_manager.hpp>
#include <graphene/chain/primary_wallet_vout_object.hpp>
#include "../common/database_fixture.hpp"
#include <sidechain/types.hpp>

using namespace graphene::chain;
using namespace sidechain;

BOOST_FIXTURE_TEST_SUITE( primary_wallet_vout_manager_tests, database_fixture )

void create_primary_wallet_vouts( primary_wallet_vout_manager& pw_vout_manager, const graphene::chain::database& db, uint32_t num )
{  
   uint64_t start_pos = 0;
   const auto& idx = db.get_index_type<primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();

   if( idx.begin() != idx.end() ){
      auto last_itr = --idx.end();
      start_pos = last_itr->id.instance() + 1;
   }

   for( uint64_t i = start_pos; i < start_pos + num; i++ ) {
      prev_out out = { std::to_string(i), i, i };
      pw_vout_manager.create_new_vout( out );
   }
}

fc::sha256 create_hash_id( const std::string& hash_tx, const uint32_t& n_vout, const uint64_t& id )
{
   std::stringstream ss;
   ss << std::hex << id;
   std::string id_hex = std::string( 16 - ss.str().size(), '0' ) + ss.str();

   std::string hash_str = fc::sha256::hash( hash_tx + std::to_string( n_vout ) ).str();
   std::string final_hash_id = std::string( hash_str.begin(), hash_str.begin() + 48 ) + id_hex;

   return fc::sha256( final_hash_id );
}

BOOST_AUTO_TEST_CASE( check_max_pw_vout_objects )
{
   const auto& idx = db.get_index_type<primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();

   primary_wallet_vout_manager pw_vout_manager( db );
   create_primary_wallet_vouts( pw_vout_manager, db, 1 );
   BOOST_CHECK( idx.size() == 2 );

   create_primary_wallet_vouts( pw_vout_manager, db, 25 );
   BOOST_CHECK( idx.size() == 27 );
   BOOST_CHECK( pw_vout_manager.is_max_vouts() == true );
}

BOOST_AUTO_TEST_CASE( check_pw_vout_objects_chain )
{
   const auto& idx = db.get_index_type<primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   primary_wallet_vout_manager pw_vout_manager( db );
   create_primary_wallet_vouts( pw_vout_manager, db, 24 );

   for( auto itr: idx ) {
      BOOST_CHECK( itr.hash_id == create_hash_id( itr.vout.hash_tx, itr.vout.n_vout, itr.id.instance() ) );
   }

   BOOST_CHECK( pw_vout_manager.get_latest_unused_vout().valid() );

   auto pw_vout = *pw_vout_manager.get_latest_unused_vout();

   BOOST_CHECK_EQUAL( pw_vout.vout.hash_tx, "24" );
   BOOST_CHECK_EQUAL( pw_vout.vout.n_vout, 24 );
   BOOST_CHECK( pw_vout.hash_id ==  create_hash_id( "24", 24, pw_vout.id.instance() ) );
}

BOOST_AUTO_TEST_CASE( delete_pw_vout_objects )
{
   const auto& idx = db.get_index_type<primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   primary_wallet_vout_manager pw_vout_manager( db );
   create_primary_wallet_vouts( pw_vout_manager, db, 24 );

   BOOST_CHECK_EQUAL( idx.size(), 25 );

   auto pw_vout = *pw_vout_manager.get_latest_unused_vout();
   pw_vout_manager.delete_vout_with_newer( create_hash_id( pw_vout.vout.hash_tx, pw_vout.vout.n_vout, pw_vout.id.instance() ) );
   BOOST_CHECK_EQUAL( idx.size(), 24 );
   
   pw_vout_manager.delete_vout_with_newer( create_hash_id( "13", 13, 13 ) );

   BOOST_CHECK_EQUAL( idx.size(), 13 );

   for( auto itr: idx ) {
      BOOST_CHECK( itr.hash_id == create_hash_id( itr.vout.hash_tx, itr.vout.n_vout, itr.id.instance() ) );
   }

   create_primary_wallet_vouts( pw_vout_manager, db, 8 );   
   pw_vout_manager.delete_vout_with_newer( create_hash_id( "20", 20, 20 ) );
   BOOST_CHECK_EQUAL( idx.size(), 21 );

   for( auto itr: idx ) {
      BOOST_CHECK( itr.hash_id == create_hash_id( itr.vout.hash_tx, itr.vout.n_vout, itr.id.instance() ) );
   }

   auto itr_primary_wallet = idx.begin();
   pw_vout_manager.delete_vout_with_newer( create_hash_id( itr_primary_wallet->vout.hash_tx, itr_primary_wallet->vout.n_vout, itr_primary_wallet->id.instance() ) );
   BOOST_CHECK_EQUAL( idx.size(), 0 );
}

BOOST_AUTO_TEST_CASE( confirm_pw_vout_objects )
{
   const auto& idx = db.get_index_type<primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   primary_wallet_vout_manager pw_vout_manager( db );
   create_primary_wallet_vouts( pw_vout_manager, db, 1 );

   auto itr = idx.begin();
   pw_vout_manager.confirm_vout( create_hash_id( itr->vout.hash_tx, itr->vout.n_vout, itr->id.instance() ) );

   BOOST_CHECK( itr->confirmed == true );

   itr++;
   pw_vout_manager.confirm_vout( create_hash_id( itr->vout.hash_tx, itr->vout.n_vout, itr->id.instance() ) );

   BOOST_CHECK( itr->confirmed == true );
   BOOST_CHECK_EQUAL( idx.size(), 1 );

   create_primary_wallet_vouts( pw_vout_manager, db, 1 );
   itr++;
   pw_vout_manager.confirm_vout( create_hash_id( itr->vout.hash_tx, itr->vout.n_vout, itr->id.instance() ) );

   BOOST_CHECK( itr->confirmed == true );
   BOOST_CHECK_EQUAL( idx.size(), 1 );

   GRAPHENE_REQUIRE_THROW( pw_vout_manager.confirm_vout( fc::sha256::hash( "4" + std::to_string( 4 ))), fc::exception );
   GRAPHENE_REQUIRE_THROW( pw_vout_manager.confirm_vout( fc::sha256::hash( "123" + std::to_string( 123 ))), fc::exception );
}

BOOST_AUTO_TEST_CASE( use_pw_vout_objects )
{
   const auto& idx = db.get_index_type<primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   primary_wallet_vout_manager pw_vout_manager( db );

   auto itr = idx.begin();
   pw_vout_manager.mark_as_used_vout( create_hash_id( itr->vout.hash_tx, itr->vout.n_vout, itr->id.instance() ) );

   BOOST_CHECK( !pw_vout_manager.get_latest_unused_vout().valid() );
   BOOST_CHECK( itr->used == true );

   create_primary_wallet_vouts( pw_vout_manager, db, 1 );
   itr++;
   pw_vout_manager.mark_as_used_vout( create_hash_id( itr->vout.hash_tx, itr->vout.n_vout, itr->id.instance() ) );

   BOOST_CHECK( !pw_vout_manager.get_latest_unused_vout().valid() );
   BOOST_CHECK( itr->used == true );

   create_primary_wallet_vouts( pw_vout_manager, db, 2 );
   itr++;
   itr++;

   GRAPHENE_REQUIRE_THROW( pw_vout_manager.mark_as_used_vout( create_hash_id( itr->vout.hash_tx, itr->vout.n_vout, itr->id.instance() ) ), fc::exception );
}

BOOST_AUTO_TEST_SUITE_END()