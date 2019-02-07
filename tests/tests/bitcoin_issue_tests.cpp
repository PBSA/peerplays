#include <boost/test/unit_test.hpp>
#include "../common/database_fixture.hpp"

#include <fc/crypto/digest.hpp>

#include <graphene/chain/primary_wallet_vout_object.hpp>
#include <graphene/chain/bitcoin_transaction_object.hpp>
#include <graphene/chain/info_for_used_vin_object.hpp>
#include <graphene/chain/info_for_vout_object.hpp>
#include <graphene/chain/bitcoin_address_object.hpp>

using namespace graphene::chain;

BOOST_FIXTURE_TEST_SUITE( bitcoin_issue_tests, database_fixture )

void create_bitcoin_issue_operation_environment( database& db )
{
   std::vector< fc::sha256 >    vins;
   std::vector< info_for_vout_id_type >        vouts;

   for( auto i = 0; i < 3; i++ ){
      db.create<bitcoin_address_object>([&]( bitcoin_address_object& obj ) {
         obj.owner = account_id_type( i );
         obj.address.address = std::to_string( i );
      });

      auto vin_id = db.create<info_for_used_vin_object>([&]( info_for_used_vin_object& obj ) {
         obj.identifier = fc::sha256( std::string( 64, std::to_string( i )[0] ) );
         obj.out.amount = 100000 + i * 100000;
         obj.address = std::to_string( i );
      });

      auto vout_id = db.create<info_for_vout_object>([&]( info_for_vout_object& obj ) {
         obj.payer = account_id_type( i );
         obj.amount = 100000 + i * 100000;
         obj.address = std::to_string( i );
      }).get_id();

      vins.push_back( vin_id.identifier );
      vouts.push_back( vout_id );
   }
   
   db.create<bitcoin_transaction_object>([&]( bitcoin_transaction_object& obj ) {
      obj.pw_vin = db.pw_vout_manager.get_latest_unused_vout()->hash_id;
      obj.vins = vins;
      obj.vouts = vouts;
      obj.transaction_id = fc::sha256( "1111111111111111111111111111111111111111111111111111111111111111" );
      obj.fee_for_size = 0;
   });
}

BOOST_AUTO_TEST_CASE( check_deleting_all_btc_transaction_information )
{
   transaction_evaluation_state context(&db);

   const auto& btc_trx_idx = db.get_index_type<bitcoin_transaction_index>().indices().get<by_id>();
   const auto& vins_info_idx = db.get_index_type<info_for_used_vin_index>().indices().get<by_id>();
   const auto& vouts_info_idx = db.get_index_type<info_for_vout_index>().indices().get<by_id>();

   create_bitcoin_issue_operation_environment( db );
   db.bitcoin_confirmations.insert( sidechain::bitcoin_transaction_confirmations( fc::sha256( "1111111111111111111111111111111111111111111111111111111111111111" ) ) );

   FC_ASSERT( btc_trx_idx.size() == 1 );
   FC_ASSERT( vins_info_idx.size() == 3 );
   FC_ASSERT( vouts_info_idx.size() == 3 );
   FC_ASSERT( db.bitcoin_confirmations.size() == 1 );

   bitcoin_issue_operation op;
   op.payer = db.get_sidechain_account_id();
   op.transaction_ids = { fc::sha256( "1111111111111111111111111111111111111111111111111111111111111111" ) };
   db.apply_operation( context, op );

   FC_ASSERT( btc_trx_idx.size() == 0 );
   FC_ASSERT( vins_info_idx.size() == 0 );
   FC_ASSERT( vouts_info_idx.size() == 0 );
   FC_ASSERT( db.bitcoin_confirmations.size() == 0 );
}

BOOST_AUTO_TEST_CASE( check_adding_issue_to_accounts )
{
   transaction_evaluation_state context(&db);
   const auto& btc_trx_idx = db.get_index_type<account_balance_index>().indices().get<by_account_asset>();      

   create_bitcoin_issue_operation_environment( db );

   bitcoin_issue_operation op;
   op.payer = db.get_sidechain_account_id();
   op.transaction_ids = { fc::sha256( "1111111111111111111111111111111111111111111111111111111111111111" ) };
   
   db.apply_operation( context, op );

   for( auto i = 0; i < 3; i++ ){
      auto itr = btc_trx_idx.find( boost::make_tuple( account_id_type( i ), db.get_sidechain_asset_id() ) );
      FC_ASSERT( itr != btc_trx_idx.end() );

      FC_ASSERT( itr->balance == 100000 + i * 100000 );
   }   
}

BOOST_AUTO_TEST_CASE( check_bitcoin_issue_operation_throw )
{
   transaction_evaluation_state context(&db);

   const auto& btc_trx_idx = db.get_index_type<bitcoin_transaction_index>().indices().get<by_id>();
   const auto& btc_addr_idx = db.get_index_type<bitcoin_address_index>().indices().get<by_address>();
   const auto& vins_info_idx = db.get_index_type<info_for_used_vin_index>().indices().get<by_id>();
   const auto& vouts_info_idx = db.get_index_type<info_for_vout_index>().indices().get<by_id>();

   create_bitcoin_issue_operation_environment( db );
   db.bitcoin_confirmations.insert( sidechain::bitcoin_transaction_confirmations( fc::sha256( "1111111111111111111111111111111111111111111111111111111111111111" ) ) );
   
   {
      auto session = db._undo_db.start_undo_session();

      db.remove( *vouts_info_idx.begin() );

      bitcoin_issue_operation op;
      op.payer = db.get_sidechain_account_id();
      op.transaction_ids = { fc::sha256( "1111111111111111111111111111111111111111111111111111111111111111" ) };
      GRAPHENE_REQUIRE_THROW( db.apply_operation( context, op ) , fc::exception );

      session.undo();
   }

   {
      auto session = db._undo_db.start_undo_session();

      db.remove( *vins_info_idx.begin() );

      bitcoin_issue_operation op;
      op.payer = db.get_sidechain_account_id();
      op.transaction_ids = { fc::sha256( "1111111111111111111111111111111111111111111111111111111111111111" ) };
      GRAPHENE_REQUIRE_THROW( db.apply_operation( context, op ) , fc::exception );

      session.undo();
   }

   {
      auto session = db._undo_db.start_undo_session();

      db.remove( *btc_addr_idx.begin() );

      bitcoin_issue_operation op;
      op.payer = db.get_sidechain_account_id();
      op.transaction_ids = { fc::sha256( "1111111111111111111111111111111111111111111111111111111111111111" ) };
      GRAPHENE_REQUIRE_THROW( db.apply_operation( context, op ) , fc::exception );

      session.undo();
   }

   {
      auto session = db._undo_db.start_undo_session();

      db.remove( *btc_trx_idx.begin() );

      bitcoin_issue_operation op;
      op.payer = db.get_sidechain_account_id();
      op.transaction_ids = { fc::sha256( "1111111111111111111111111111111111111111111111111111111111111111" ) };
      GRAPHENE_REQUIRE_THROW( db.apply_operation( context, op ) , fc::exception );

      session.undo();
   }

   {
      auto session = db._undo_db.start_undo_session();

      bitcoin_issue_operation op;
      op.payer = account_id_type(1);
      op.transaction_ids = { fc::sha256( "1111111111111111111111111111111111111111111111111111111111111111" ) };
      GRAPHENE_REQUIRE_THROW( db.apply_operation( context, op ) , fc::exception );

      session.undo();
   }
}


BOOST_AUTO_TEST_SUITE_END()