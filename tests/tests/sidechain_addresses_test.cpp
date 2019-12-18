#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/peerplays_sidechain/defs.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( sidechain_addresses_tests, database_fixture )

BOOST_AUTO_TEST_CASE( sidechain_address_add_test ) {

   BOOST_TEST_MESSAGE("sidechain_address_add_test");

   generate_block();
   set_expiration(db, trx);

   ACTORS((alice));

   generate_block();
   set_expiration(db, trx);

   {
      BOOST_TEST_MESSAGE("Send sidechain_address_add_operation");

      sidechain_address_add_operation op;

      op.sidechain_address_account = alice_id;
      op.sidechain = graphene::peerplays_sidechain::sidechain_type::bitcoin;
      op.address = "address";
      op.private_key = "private_key";
      op.public_key = "public_key";

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   BOOST_TEST_MESSAGE("Check sidechain_address_add_operation results");

   const auto& idx = db.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain>();
   BOOST_REQUIRE( idx.size() == 1 );
   auto obj = idx.find( boost::make_tuple( alice_id, graphene::peerplays_sidechain::sidechain_type::bitcoin ) );
   BOOST_REQUIRE( obj != idx.end() );
   BOOST_CHECK( obj->sidechain_address_account == alice_id );
   BOOST_CHECK( obj->sidechain == graphene::peerplays_sidechain::sidechain_type::bitcoin );
   BOOST_CHECK( obj->address == "address" );
   BOOST_CHECK( obj->private_key == "private_key" );
   BOOST_CHECK( obj->public_key == "public_key" );
}

BOOST_AUTO_TEST_CASE( sidechain_address_update_test ) {

   BOOST_TEST_MESSAGE("sidechain_address_update_test");

   INVOKE(sidechain_address_add_test);

   GET_ACTOR(alice);

   const auto& idx = db.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain>();
   BOOST_REQUIRE( idx.size() == 1 );
   auto obj = idx.find( boost::make_tuple( alice_id, graphene::peerplays_sidechain::sidechain_type::bitcoin ) );
   BOOST_REQUIRE( obj != idx.end() );

   std::string new_address = "new_address";
   std::string new_private_key = "new_private_key";
   std::string new_public_key = "new_public_key";

   {
      BOOST_TEST_MESSAGE("Send sidechain_address_update_operation");

      sidechain_address_update_operation op;
      op.sidechain_address_id = sidechain_address_id_type(0);
      op.sidechain_address_account = obj->sidechain_address_account;
      op.sidechain = obj->sidechain;
      op.address = new_address;
      op.private_key = new_private_key;
      op.public_key = new_public_key;

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   {
      BOOST_TEST_MESSAGE("Check sidechain_address_update_operation results");

      const auto& idx = db.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain>();
      BOOST_REQUIRE( idx.size() == 1 );
      auto obj = idx.find( boost::make_tuple( alice_id, graphene::peerplays_sidechain::sidechain_type::bitcoin ) );
      BOOST_REQUIRE( obj != idx.end() );
      BOOST_CHECK( obj->sidechain_address_account == obj->sidechain_address_account );
      BOOST_CHECK( obj->sidechain == obj->sidechain );
      BOOST_CHECK( obj->address == new_address );
      BOOST_CHECK( obj->private_key == new_private_key );
      BOOST_CHECK( obj->public_key == new_public_key );
   }
}

BOOST_AUTO_TEST_CASE( sidechain_address_delete_test ) {

   BOOST_TEST_MESSAGE("sidechain_address_delete_test");

   INVOKE(sidechain_address_add_test);

   GET_ACTOR(alice);

   const auto& idx = db.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain>();
   BOOST_REQUIRE( idx.size() == 1 );
   auto obj = idx.find( boost::make_tuple( alice_id, graphene::peerplays_sidechain::sidechain_type::bitcoin ) );
   BOOST_REQUIRE( obj != idx.end() );

   {
      BOOST_TEST_MESSAGE("Send sidechain_address_delete_operation");

      sidechain_address_delete_operation op;
      op.sidechain_address_id = sidechain_address_id_type(0);
      op.sidechain_address_account = obj->sidechain_address_account;
      op.sidechain = obj->sidechain;

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   {
      BOOST_TEST_MESSAGE("Check sidechain_address_delete_operation results");

      const auto& idx = db.get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain>();
      BOOST_REQUIRE( idx.size() == 0 );
      auto obj = idx.find( boost::make_tuple( alice_id, graphene::peerplays_sidechain::sidechain_type::bitcoin ) );
      BOOST_REQUIRE( obj == idx.end() );
   }
}

BOOST_AUTO_TEST_SUITE_END()

