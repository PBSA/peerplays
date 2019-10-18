#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/son_evaluator.hpp>
#include <graphene/chain/vesting_balance_object.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( son_operation_tests, database_fixture )

BOOST_AUTO_TEST_CASE( create_son_test ) {
   generate_blocks(HARDFORK_SON_TIME);
   generate_block();
   set_expiration(db, trx);

   ACTORS((alice)(bob));

   upgrade_to_lifetime_member(alice);
   upgrade_to_lifetime_member(bob);

   transfer( committee_account, alice_id, asset( 1000*GRAPHENE_BLOCKCHAIN_PRECISION ) );
   transfer( committee_account, bob_id, asset( 1000*GRAPHENE_BLOCKCHAIN_PRECISION ) );

   set_expiration(db, trx);
   std::string test_url = "https://create_son_test";

   // create deposit vesting
   vesting_balance_id_type deposit;
   {
      vesting_balance_create_operation op;
      op.creator = alice_id;
      op.owner = alice_id;
      op.amount = asset(10*GRAPHENE_BLOCKCHAIN_PRECISION);
      op.balance_type = vesting_balance_type::son;
      op.policy = dormant_vesting_policy_initializer {};
      trx.operations.push_back(op);

      // amount in the son balance need to be at least 50
      GRAPHENE_REQUIRE_THROW( PUSH_TX( db, trx ), fc::exception );

      op.amount = asset(50*GRAPHENE_BLOCKCHAIN_PRECISION);
      trx.clear();

      trx.operations.push_back(op);
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      deposit = ptx.operation_results[0].get<object_id_type>();

      auto deposit_vesting = db.get<vesting_balance_object>(ptx.operation_results[0].get<object_id_type>());

      BOOST_CHECK_EQUAL(deposit(db).balance.amount.value, 50*GRAPHENE_BLOCKCHAIN_PRECISION);
      auto now = db.head_block_time();
      BOOST_CHECK_EQUAL(deposit(db).is_withdraw_allowed(now, asset(50*GRAPHENE_BLOCKCHAIN_PRECISION)), false); // cant withdraw
   }
   generate_block();
   set_expiration(db, trx);

   // create payment normal vesting
   vesting_balance_id_type payment ;
   {
      vesting_balance_create_operation op;
      op.creator = alice_id;
      op.owner = alice_id;
      op.amount = asset(1*GRAPHENE_BLOCKCHAIN_PRECISION);
      op.balance_type = vesting_balance_type::normal;

      op.validate();

      trx.operations.push_back(op);
      trx.validate();
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      trx.clear();
      payment = ptx.operation_results[0].get<object_id_type>();
   }

   generate_block();
   set_expiration(db, trx);

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
try {
   INVOKE(create_son_test);
   GET_ACTOR(alice);

   auto deposit_vesting = db.get<vesting_balance_object>(vesting_balance_id_type(0));
   auto now = db.head_block_time();
   BOOST_CHECK_EQUAL(deposit_vesting.is_withdraw_allowed(now, asset(50)), false); // cant withdraw

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

   deposit_vesting = db.get<vesting_balance_object>(vesting_balance_id_type(0));
   BOOST_CHECK_EQUAL(deposit_vesting.policy.get<linear_vesting_policy>().vesting_cliff_seconds,
         db.get_global_properties().parameters.son_vesting_period()); // in linear policy

   now = db.head_block_time();
   BOOST_CHECK_EQUAL(deposit_vesting.is_withdraw_allowed(now, asset(50)), false); // but still cant withdraw

   generate_blocks(now + fc::seconds(db.get_global_properties().parameters.son_vesting_period()));
   generate_block();

   deposit_vesting = db.get<vesting_balance_object>(vesting_balance_id_type(0));
   now = db.head_block_time();
   BOOST_CHECK_EQUAL(deposit_vesting.is_withdraw_allowed(now, asset(50)), true); // after 2 days withdraw is allowed
}
catch (fc::exception &e) {
   edump((e.to_detail_string()));
   throw;
} }

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
