#include <boost/test/unit_test.hpp>
#include <sidechain/input_withdrawal_info.hpp>
#include "../common/database_fixture.hpp"

using namespace graphene::chain;

BOOST_FIXTURE_TEST_SUITE( input_withdrawal_info_tests, database_fixture )

BOOST_AUTO_TEST_CASE( input_withdrawal_info_insert_vin_test ) {
   sidechain::input_withdrawal_info infos( db );
   sidechain::prev_out out = { "1", 1, 13 };
   infos.insert_info_for_vin( out, "addr1", { 0x01, 0x02, 0x03 } );
   BOOST_CHECK( infos.size_info_for_vins() == 1 );

   sidechain::info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_many_insert_vin_test ) {
   sidechain::input_withdrawal_info infos( db );
   for( size_t i = 1; i <= 10; i++ ) {
      sidechain::prev_out out = { std::to_string( i ), static_cast<uint32_t>( i ), static_cast< uint64_t >( i ) };
      infos.insert_info_for_vin( out, "addr" + std::to_string( i ), { 0x01, 0x02, 0x03 } );
   }
   BOOST_CHECK( infos.size_info_for_vins() == 10 );

   sidechain::info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_id_test ) {
   sidechain::input_withdrawal_info infos( db );
   for( size_t i = 0; i < 10; i++ ) {
      sidechain::prev_out out = { std::to_string( i ), static_cast<uint32_t>( i ), static_cast< uint64_t >( i ) };
      infos.insert_info_for_vin( out, "addr" + std::to_string( i ), { 0x01, 0x02, 0x03 } );
      BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).first );
      BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).second->id == i );
   }

   sidechain::info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_check_data_test ) {
   sidechain::input_withdrawal_info infos( db );
   for( size_t i = 0; i < 10; i++ ) {
      sidechain::prev_out out = { std::to_string( i ), static_cast<uint32_t>( i ), static_cast< uint64_t >( i ) };
      infos.insert_info_for_vin( out, "addr" + std::to_string( i ), { 0x01, 0x02, 0x03 } );
   }

   for( size_t i = 0; i < 10; i++ ) {
      BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).first );
      BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).second->id == i );
      BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).second->out.hash_tx == std::to_string( i ) );
      BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).second->out.n_vout == i );
      BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).second->out.amount == i );
      BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).second->address == "addr" + std::to_string( i ) );
      std::vector<char> script = { 0x01, 0x02, 0x03 };
      BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).second->script == script );
   }

   sidechain::info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_modify_test ) {
   sidechain::input_withdrawal_info infos( db );
   for( size_t i = 0; i < 10; i++ ) {
      sidechain::prev_out out = { std::to_string( i ), static_cast<uint32_t>( i ), static_cast< uint64_t >( i ) };
      infos.insert_info_for_vin( out, "addr" + std::to_string( i ), { 0x01, 0x02, 0x03 } );
   }

   for( size_t i = 0; i < 10; i++ ) {
      if( i % 2 == 0 ) {
         auto iter = infos.find_info_for_vin( static_cast< uint64_t >( i ) );
         BOOST_CHECK( iter.first );
         infos.modify_info_for_vin( *iter.second, [&]( sidechain::info_for_vin& obj ) {
            obj.out.hash_tx = std::to_string( i + 1 );
            obj.out.n_vout = i + 1;
            obj.out.amount = i + 1;
            obj.address = "addr" + std::to_string( i ) + std::to_string( i );
         } );
      }
   }

   for( size_t i = 0; i < 10; i++ ) {
      if( i % 2 == 0 ) {
         BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).first );
         BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).second->id == i );
         BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).second->out.hash_tx == std::to_string( i + 1 ) );
         BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).second->out.n_vout == i + 1 );
         BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).second->out.amount == i + 1 );
         BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).second->address == "addr" + std::to_string( i ) + std::to_string( i ) );
         std::vector<char> script = { 0x01, 0x02, 0x03 };
         BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).second->script == script );
      }
   }

   sidechain::info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_remove_vin_test ) {
   sidechain::input_withdrawal_info infos( db );
   for( size_t i = 0; i < 10; i++ ) {
      sidechain::prev_out out = { std::to_string( i ), static_cast<uint32_t>( i ), static_cast< uint64_t >( i ) };
      infos.insert_info_for_vin( out, "addr" + std::to_string( i ), { 0x01, 0x02, 0x03 } );
   }

   for( size_t i = 0; i < 10; i++ ) {
      if( i % 2 == 0 ) {
         auto iter = infos.find_info_for_vin( static_cast< uint64_t >( i ) );
         BOOST_CHECK( iter.first );
         infos.remove_info_for_vin( *iter.second );
      }
   }

   for( size_t i = 0; i < 10; i++ ) {
      if( i % 2 == 0 ) {
         BOOST_CHECK( !infos.find_info_for_vin( static_cast< uint64_t >( i ) ).first );
      } else {
         BOOST_CHECK( infos.find_info_for_vin( static_cast< uint64_t >( i ) ).first );
      }
   }

   sidechain::info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_get_info_for_vins_test ) {
   sidechain::input_withdrawal_info infos( db );
   for( size_t i = 0; i < 10; i++ ) {
      sidechain::prev_out out = { std::to_string( i ), static_cast<uint32_t>( i ), static_cast< uint64_t >( i ) };
      infos.insert_info_for_vin( out, "addr" + std::to_string( i ), { 0x01, 0x02, 0x03 } );
   }

   const auto& vins = infos.get_info_for_vins();
   BOOST_CHECK( vins.size() == 5 ); // 5 amount vins to bitcoin transaction

   for( size_t i = 0; i < 7; i++ ) {
      auto iter = infos.find_info_for_vin( static_cast< uint64_t >( i ) );
      infos.mark_as_used_vin( *iter.second );
   }

   const auto& vins2 = infos.get_info_for_vins();
   BOOST_CHECK( vins2.size() == 3 );

   sidechain::info_for_vin::count_id_info_for_vin = 0;
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_insert_vout_test ) {
   sidechain::input_withdrawal_info infos( db );
   infos.insert_info_for_vout( account_id_type(), "1", 1 );
   BOOST_CHECK( infos.size_info_for_vouts() == 1 );
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_many_insert_vout_test ) {
   sidechain::input_withdrawal_info infos( db );
   for( size_t i = 1; i <= 10; i++ ) {
      infos.insert_info_for_vout( account_id_type(i), std::to_string( i ), static_cast<uint64_t>( i ) );
   }
   BOOST_CHECK( infos.size_info_for_vouts() == 10 );
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_remove_vout_test ) {
   sidechain::input_withdrawal_info infos( db );
   for( size_t i = 0; i < 10; i++ ) {
      infos.insert_info_for_vout( account_id_type(i), std::to_string( i ), static_cast<uint64_t>( i ) );
   }

   for( size_t i = 0; i < 10; i++ ) {
      if( i % 2 == 0 ) {
         auto iter = infos.find_info_for_vout( static_cast< uint64_t >( i ) );
         BOOST_CHECK( iter.first );
         infos.remove_info_for_vout( *iter.second );
      }
   }

   for( size_t i = 0; i < 10; i++ ) {
      if( i % 2 == 0 ) {
         BOOST_CHECK( !infos.find_info_for_vout( static_cast< uint64_t >( i ) ).first );
      } else {
         BOOST_CHECK( infos.find_info_for_vout( static_cast< uint64_t >( i ) ).first );
      }
   }
}

BOOST_AUTO_TEST_CASE( input_withdrawal_info_get_info_for_vouts_test ) {
   sidechain::input_withdrawal_info infos( db );
   for( size_t i = 0; i < 10; i++ ) {
      infos.insert_info_for_vout( account_id_type(i), std::to_string( i ), static_cast<uint64_t>( i ) );
   }

   const auto& vouts = infos.get_info_for_vouts();
   BOOST_CHECK( vouts.size() == 5 ); // 5 amount vouts to bitcoin transaction

   for( size_t i = 0; i < 7; i++ ) {
      auto iter = infos.find_info_for_vout( static_cast< uint64_t >( i ) );
      infos.mark_as_used_vout( *iter.second );
   }

   const auto& vouts2 = infos.get_info_for_vouts();
   BOOST_CHECK( vouts2.size() == 3 );
}

BOOST_AUTO_TEST_SUITE_END()
