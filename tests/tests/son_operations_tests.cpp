#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/son_evaluator.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( son_operation_tests, database_fixture )

BOOST_AUTO_TEST_CASE( create_son_test ) {
   generate_blocks(HARDFORK_SON_TIME);
   while (db.head_block_time() <= HARDFORK_SON_TIME) {
      generate_block();
   }
   generate_block();
   set_expiration(db, trx);

   ACTORS((alice)(bob));

   upgrade_to_lifetime_member(alice);
   upgrade_to_lifetime_member(bob);

   transfer( committee_account, alice_id, asset( 100000 ) );
   transfer( committee_account, bob_id, asset( 100000 ) );

   set_expiration(db, trx);
   std::string test_url = "https://create_son_test";

   // create deposit vesting
   vesting_balance_id_type deposit;
   {
      vesting_balance_create_operation op;
      op.creator = alice_id;
      op.owner = alice_id;
      op.amount = asset(10);
      //op.balance_type = vesting_balance_type::unspecified;

      trx.operations.push_back(op);
      set_expiration(db, trx);
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      trx.clear();
      deposit = ptx.operation_results[0].get<object_id_type>();
   }
   // create payment vesting
   vesting_balance_id_type payment;
   {
      vesting_balance_create_operation op;
      op.creator = alice_id;
      op.owner = alice_id;
      op.amount = asset(10);
      //op.balance_type = vesting_balance_type::unspecified;

      trx.operations.push_back(op);
      set_expiration(db, trx);
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      trx.clear();
      payment = ptx.operation_results[0].get<object_id_type>();
   }

   // alice became son
   {
      son_create_operation op;
      op.owner_account = alice_id;
      op.url = test_url;
      op.deposit = deposit;
      op.pay_vb = payment;
      op.signing_key = alice_public_key;
      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   const auto& idx = db.get_index_type<son_index>().indices().get<by_account>();
   BOOST_REQUIRE( idx.size() == 1 );
   auto obj = idx.find( alice_id );
   BOOST_REQUIRE( obj != idx.end() );
   BOOST_CHECK( obj->url == test_url );
   BOOST_CHECK( obj->signing_key == alice_public_key );
   BOOST_CHECK( obj->deposit.instance == deposit.instance.value );
   BOOST_CHECK( obj->pay_vb.instance == payment.instance.value );
}

BOOST_AUTO_TEST_CASE( update_son_test ) {

   INVOKE(create_son_test);
   GET_ACTOR(alice);

   std::string new_url = "https://anewurl.com";

   {
      son_update_operation op;
      op.owner_account = alice_id;
      op.new_url = new_url;
      op.son_id = son_id_type(0);

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   const auto& idx = db.get_index_type<son_index>().indices().get<by_account>();
   BOOST_REQUIRE( idx.size() == 1 );
   auto obj = idx.find( alice_id );
   BOOST_REQUIRE( obj != idx.end() );
   BOOST_CHECK( obj->url == new_url );
}

BOOST_AUTO_TEST_CASE( delete_son_test ) {

   INVOKE(create_son_test);
   GET_ACTOR(alice);

   {
      son_delete_operation op;
      op.owner_account = alice_id;
      op.son_id = son_id_type(0);

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   const auto& idx = db.get_index_type<son_index>().indices().get<by_account>();
   BOOST_REQUIRE( idx.empty() );
}

BOOST_AUTO_TEST_CASE( update_delete_not_own ) { // fee payer needs to be the son object owner
try {

   INVOKE(create_son_test);
   GET_ACTOR(alice);
   GET_ACTOR(bob);

   // bob tries to update a son object he dont own
   {
      son_update_operation op;
      op.owner_account = bob_id;
      op.new_url = "whatever";
      op.son_id = son_id_type(0);

      trx.operations.push_back(op);
      sign(trx, bob_private_key);
      GRAPHENE_REQUIRE_THROW(PUSH_TX( db, trx ), fc::exception);
   }
   generate_block();

   set_expiration(db, trx);
   trx.clear();

   const auto& idx = db.get_index_type<son_index>().indices().get<by_account>();
   auto obj = idx.find( alice_id );
   BOOST_REQUIRE( obj != idx.end() );
   // not changing
   BOOST_CHECK( obj->url == "https://create_son_test" );

   // bob tries to delete a son object he dont own
   {
      son_delete_operation op;
      op.owner_account = bob_id;
      op.son_id = son_id_type(0);

      trx.operations.push_back(op);
      sign(trx, bob_private_key);
      GRAPHENE_REQUIRE_THROW(PUSH_TX( db, trx ), fc::exception);

   }
   generate_block();

   obj = idx.find( alice_id );
   // not deleting
   BOOST_REQUIRE( obj != idx.end() );
   BOOST_CHECK( obj->son_account.instance ==  alice_id.instance);
}
catch (fc::exception &e) {
   edump((e.to_detail_string()));
   throw;
}

} BOOST_AUTO_TEST_SUITE_END()
