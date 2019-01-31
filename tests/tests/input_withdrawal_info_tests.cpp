#include <boost/test/unit_test.hpp>
#include <sidechain/input_withdrawal_info.hpp>
#include "../common/database_fixture.hpp"
#include <sidechain/types.hpp>
#include <graphene/chain/bitcoin_address_object.hpp>

using namespace graphene::chain;
using namespace sidechain;

BOOST_FIXTURE_TEST_SUITE( input_withdrawal_info_tests, database_fixture )

BOOST_AUTO_TEST_CASE( input_withdrawal_info_insert_vin_test )
{
   input_withdrawal_info infos( db );
   prev_out out = { "1", 1, 13 };
   infos.insert_info_for_vin( out, "addr1", { 0x01, 0x02, 0x03 } );
   BOOST_CHECK( infos.size_info_for_vins() == 1 );

   info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_many_insert_vin_test )
{
   input_withdrawal_info infos( db );
   for( size_t i = 1; i <= 10; i++ ) {
      prev_out out = { std::to_string( i ), static_cast<uint32_t>( i ), static_cast< uint64_t >( i ) };
      infos.insert_info_for_vin( out, "addr" + std::to_string( i ), { 0x01, 0x02, 0x03 } );
   }
   BOOST_CHECK( infos.size_info_for_vins() == 10 );

   info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_id_test )
{
   input_withdrawal_info infos( db );
   for( size_t i = 0; i < 10; i++ ) {
      prev_out out = { std::to_string( i ), static_cast<uint32_t>( i ), static_cast< uint64_t >( i ) };
      infos.insert_info_for_vin( out, "addr" + std::to_string( i ), { 0x01, 0x02, 0x03 } );
      auto identifier = fc::sha256::hash( std::to_string( i ) + std::to_string( i ) );
      BOOST_CHECK( infos.find_info_for_vin( identifier ).valid() );
      BOOST_CHECK( infos.find_info_for_vin( identifier )->id == i );
   }

   info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_check_data_test )
{
   input_withdrawal_info infos( db );
   for( size_t i = 0; i < 10; i++ ) {
      prev_out out = { std::to_string( i ), static_cast<uint32_t>( i ), static_cast< uint64_t >( i ) };
      infos.insert_info_for_vin( out, "addr" + std::to_string( i ), { 0x01, 0x02, 0x03 } );
   }

   for( size_t i = 0; i < 10; i++ ) {
      auto identifier = fc::sha256::hash( std::to_string( i ) + std::to_string( i ) );
      BOOST_CHECK( infos.find_info_for_vin( identifier ).valid() );
      BOOST_CHECK( infos.find_info_for_vin( identifier )->id == i );
      BOOST_CHECK( infos.find_info_for_vin( identifier )->out.hash_tx == std::to_string( i ) );
      BOOST_CHECK( infos.find_info_for_vin( identifier )->out.n_vout == i );
      BOOST_CHECK( infos.find_info_for_vin( identifier )->out.amount == i );
      BOOST_CHECK( infos.find_info_for_vin( identifier )->address == "addr" + std::to_string( i ) );
      std::vector<char> script = { 0x01, 0x02, 0x03 };
      BOOST_CHECK( infos.find_info_for_vin( identifier )->script == script );
   }

   info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_modify_test )
{
   input_withdrawal_info infos( db );
   for( size_t i = 0; i < 10; i++ ) {
      prev_out out = { std::to_string( i ), static_cast<uint32_t>( i ), static_cast< uint64_t >( i ) };
      infos.insert_info_for_vin( out, "addr" + std::to_string( i ), { 0x01, 0x02, 0x03 } );
   }

   for( size_t i = 0; i < 10; i++ ) {
      if( i % 2 == 0 ) {
         auto identifier = fc::sha256::hash( std::to_string( i ) + std::to_string( i ) );
         auto iter = infos.find_info_for_vin( identifier );
         BOOST_CHECK( iter.valid() );
         infos.modify_info_for_vin( *iter, [&]( info_for_vin& obj ) {
            obj.out.hash_tx = std::to_string( i + 1 );
            obj.out.n_vout = i + 1;
            obj.out.amount = i + 1;
            obj.address = "addr" + std::to_string( i ) + std::to_string( i );
         } );
      }
   }

   for( size_t i = 0; i < 10; i++ ) {
      if( i % 2 == 0 ) {
         auto identifier = fc::sha256::hash( std::to_string( i ) + std::to_string( i ) );
         BOOST_CHECK( infos.find_info_for_vin( identifier ).valid() );
         BOOST_CHECK( infos.find_info_for_vin( identifier )->id == i );
         BOOST_CHECK( infos.find_info_for_vin( identifier )->out.hash_tx == std::to_string( i + 1 ) );
         BOOST_CHECK( infos.find_info_for_vin( identifier )->out.n_vout == i + 1 );
         BOOST_CHECK( infos.find_info_for_vin( identifier )->out.amount == i + 1 );
         BOOST_CHECK( infos.find_info_for_vin( identifier )->address == "addr" + std::to_string( i ) + std::to_string( i ) );
         std::vector<char> script = { 0x01, 0x02, 0x03 };
         BOOST_CHECK( infos.find_info_for_vin( identifier )->script == script );
      }
   }

   info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_remove_vin_test )
{
   input_withdrawal_info infos( db );
   for( size_t i = 0; i < 10; i++ ) {
      prev_out out = { std::to_string( i ), static_cast<uint32_t>( i ), static_cast< uint64_t >( i ) };
      infos.insert_info_for_vin( out, "addr" + std::to_string( i ), { 0x01, 0x02, 0x03 } );
   }

   for( size_t i = 0; i < 10; i++ ) {
      if( i % 2 == 0 ) {
         auto identifier = fc::sha256::hash( std::to_string( i ) + std::to_string( i ) );
         auto iter = infos.find_info_for_vin( identifier );
         BOOST_CHECK( iter.valid() );
         infos.remove_info_for_vin( *iter );
      }
   }

   for( size_t i = 0; i < 10; i++ ) {
      auto identifier = fc::sha256::hash( std::to_string( i ) + std::to_string( i ) );
      if( i % 2 == 0 ) {
         BOOST_CHECK( !infos.find_info_for_vin( identifier ).valid() );
      } else {
         BOOST_CHECK( infos.find_info_for_vin( identifier ).valid() );
      }
   }

   info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_get_info_for_vins_test )
{
   input_withdrawal_info infos( db );

   const auto pw_obj = db.get_latest_PW();
   auto witnesses_keys = pw_obj.address.witnesses_keys;

   for( size_t i = 0; i < 10; i++ ) {
      const auto& address = db.create<bitcoin_address_object>( [&]( bitcoin_address_object& a ) {
         const fc::ecc::private_key petra_private_key  = generate_private_key( std::to_string( i ) );
         witnesses_keys.begin()->second = public_key_type( petra_private_key.get_public_key() );
         a.address = sidechain::btc_multisig_segwit_address( 5, witnesses_keys);
      });
      prev_out out = { std::to_string( i ), static_cast<uint32_t>( i ), static_cast< uint64_t >( i ) };
      infos.insert_info_for_vin( out, address.get_address(), { 0x01, 0x02, 0x03 } );
   }

   const auto& vins = infos.get_info_for_vins();
   BOOST_CHECK( vins.size() == 5 ); // 5 amount vins to bitcoin transaction

   for( size_t i = 0; i < 7; i++ ) {
      auto identifier = fc::sha256::hash( std::to_string( i ) + std::to_string( i ) );
      auto iter = infos.find_info_for_vin( identifier );
      infos.mark_as_used_vin( *iter );
   }

   const auto& vins2 = infos.get_info_for_vins();
   BOOST_CHECK( vins2.size() == 3 );

   info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_insert_vout_test )
{
   input_withdrawal_info infos( db );
   infos.insert_info_for_vout( account_id_type(), "1", 1 );
   BOOST_CHECK( infos.size_info_for_vouts() == 1 );
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_many_insert_vout_test )
{
   input_withdrawal_info infos( db );
   for( size_t i = 1; i <= 10; i++ ) {
      infos.insert_info_for_vout( account_id_type(i), std::to_string( i ), static_cast<uint64_t>( i ) );
   }
   BOOST_CHECK( infos.size_info_for_vouts() == 10 );
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_remove_vout_test )
{
   input_withdrawal_info infos( db );
   for( size_t i = 0; i < 10; i++ ) {
      infos.insert_info_for_vout( account_id_type(i), std::to_string( i ), static_cast<uint64_t>( i ) );
   }

   for( size_t i = 0; i < 10; i++ ) {
      if( i % 2 == 0 ) {
         auto iter = infos.find_info_for_vout( graphene::chain::info_for_vout_id_type(i) );
         BOOST_CHECK( iter.valid() );
         infos.remove_info_for_vout( *iter );
      }
   }

   for( size_t i = 0; i < 10; i++ ) {
      if( i % 2 == 0 ) {
         BOOST_CHECK( !infos.find_info_for_vout( graphene::chain::info_for_vout_id_type(i) ).valid() );
      } else {
         BOOST_CHECK( infos.find_info_for_vout( graphene::chain::info_for_vout_id_type(i) ).valid() );
      }
   }
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_get_info_for_vouts_test )
{
   input_withdrawal_info infos( db );
   for( size_t i = 0; i < 10; i++ ) {
      infos.insert_info_for_vout( account_id_type(i), std::to_string( i ), static_cast<uint64_t>( i ) );
   }

   const auto& vouts = infos.get_info_for_vouts();
   BOOST_CHECK( vouts.size() == 5 ); // 5 amount vouts to bitcoin transaction

   for( size_t i = 0; i < 7; i++ ) {
      auto iter = infos.find_info_for_vout( graphene::chain::info_for_vout_id_type(i) );
      infos.mark_as_used_vout( *iter );
   }

   const auto& vouts2 = infos.get_info_for_vouts();
   BOOST_CHECK( vouts2.size() == 3 );
}

BOOST_AUTO_TEST_SUITE_END()
