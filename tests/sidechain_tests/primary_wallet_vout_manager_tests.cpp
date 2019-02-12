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

BOOST_AUTO_TEST_CASE( check_max_pw_vout_objects )
{
   const auto& idx = db.get_index_type<primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();

   primary_wallet_vout_manager pw_vout_manager( db );
   create_primary_wallet_vouts( pw_vout_manager, db, 1 );
   BOOST_CHECK( idx.size() == 1 );

   create_primary_wallet_vouts( pw_vout_manager, db, 24 );
   BOOST_CHECK( idx.size() == 25 );
   BOOST_CHECK( pw_vout_manager.is_max_vouts() == true );
}

BOOST_AUTO_TEST_CASE( check_pw_vout_objects_chain )
{
   const auto& idx = db.get_index_type<primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   primary_wallet_vout_manager pw_vout_manager( db );
   create_primary_wallet_vouts( pw_vout_manager, db, 24 );

   auto itr = idx.begin();
   for( size_t i = 0; i < idx.size(); i++ ) {
      BOOST_CHECK( itr->hash_id == fc::sha256::hash( std::to_string( i ) + std::to_string( i )));
      itr++;
   }

   BOOST_CHECK( pw_vout_manager.get_latest_unused_vout().valid() );

   auto pw_vout = *pw_vout_manager.get_latest_unused_vout();

   BOOST_CHECK( pw_vout.vout.hash_tx == "23" );
   BOOST_CHECK( pw_vout.vout.n_vout == 23 );
   BOOST_CHECK( pw_vout.hash_id == fc::sha256::hash( "23" + std::to_string( 23 )));
}

BOOST_AUTO_TEST_CASE( delete_pw_vout_objects )
{
   const auto& idx = db.get_index_type<primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   primary_wallet_vout_manager pw_vout_manager( db );
   create_primary_wallet_vouts( pw_vout_manager, db, 24 );

   BOOST_CHECK( idx.size() == 24 );

   pw_vout_manager.delete_vout_with_newer( fc::sha256::hash( "123" + std::to_string( 123 )));
   BOOST_CHECK( idx.size() == 24 );

   pw_vout_manager.delete_vout_with_newer( fc::sha256::hash( "13" + std::to_string( 13 )));
   BOOST_CHECK( idx.size() == 13 );

   auto itr = idx.begin();
   for( size_t i = 0; i < idx.size(); i++ ) {
      BOOST_CHECK( itr->hash_id == fc::sha256::hash( std::to_string( i ) + std::to_string( i )));
      itr++;
   }

   create_primary_wallet_vouts( pw_vout_manager, db, 8 );
   pw_vout_manager.delete_vout_with_newer( fc::sha256::hash( "20" + std::to_string( 20 )));
   BOOST_CHECK( idx.size() == 20 );

   itr = idx.begin();
   for( size_t i = 0; i < idx.size(); i++ ) {
      BOOST_CHECK( itr->hash_id == fc::sha256::hash( std::to_string( i ) + std::to_string( i )));
      itr++;
   }

   pw_vout_manager.delete_vout_with_newer( fc::sha256::hash( "0" + std::to_string( 0 )));
   BOOST_CHECK( idx.size() == 0 );
}

BOOST_AUTO_TEST_CASE( confirm_pw_vout_objects )
{
   const auto& idx = db.get_index_type<primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   primary_wallet_vout_manager pw_vout_manager( db );
   create_primary_wallet_vouts( pw_vout_manager, db, 2 );

   pw_vout_manager.confirm_vout( fc::sha256::hash( "0" + std::to_string( 0 )));

   auto itr = idx.begin();
   BOOST_CHECK( itr->confirmed == true );

   pw_vout_manager.confirm_vout( fc::sha256::hash( "1" + std::to_string( 1 )));

   itr++;
   BOOST_CHECK( itr->confirmed == true );
   BOOST_CHECK( idx.size() == 1 );

   create_primary_wallet_vouts( pw_vout_manager, db, 1 );
   itr++;
   pw_vout_manager.confirm_vout( fc::sha256::hash( "2" + std::to_string( 2 )));

   BOOST_CHECK( itr->confirmed == true );
   BOOST_CHECK( idx.size() == 1 );

   GRAPHENE_REQUIRE_THROW( pw_vout_manager.confirm_vout( fc::sha256::hash( "4" + std::to_string( 4 ))), fc::exception );
   GRAPHENE_REQUIRE_THROW( pw_vout_manager.confirm_vout( fc::sha256::hash( "123" + std::to_string( 123 ))), fc::exception );
}

BOOST_AUTO_TEST_CASE( use_pw_vout_objects )
{
   const auto& idx = db.get_index_type<primary_wallet_vout_index>().indices().get< graphene::chain::by_id >();
   primary_wallet_vout_manager pw_vout_manager( db );

   create_primary_wallet_vouts( pw_vout_manager, db, 1 );
   pw_vout_manager.mark_as_used_vout( fc::sha256::hash( "0" + std::to_string( 0 )));

   auto itr = idx.begin();
   BOOST_CHECK( !pw_vout_manager.get_latest_unused_vout().valid() );
   BOOST_CHECK( itr->used == true );

   create_primary_wallet_vouts( pw_vout_manager, db, 1 );
   pw_vout_manager.mark_as_used_vout( fc::sha256::hash( "1" + std::to_string( 1 )));
   itr++;

   BOOST_CHECK( !pw_vout_manager.get_latest_unused_vout().valid() );
   BOOST_CHECK( itr->used == true );

   create_primary_wallet_vouts( pw_vout_manager, db, 2 );

   GRAPHENE_REQUIRE_THROW( pw_vout_manager.mark_as_used_vout( fc::sha256::hash( "3" + std::to_string( 3 ))), fc::exception );
}

BOOST_AUTO_TEST_SUITE_END()