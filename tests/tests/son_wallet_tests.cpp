#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/peerplays_sidechain/defs.hpp>

using namespace graphene;
using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( son_wallet_tests, database_fixture )

BOOST_AUTO_TEST_CASE( son_wallet_recreate_test ) {

   BOOST_TEST_MESSAGE("son_wallet_recreate_test");

   generate_blocks(HARDFORK_SON_TIME);
   generate_block();
   set_expiration(db, trx);

   ACTORS((alice)(bob));

   upgrade_to_lifetime_member(alice);
   upgrade_to_lifetime_member(bob);

   transfer( committee_account, alice_id, asset( 500000*GRAPHENE_BLOCKCHAIN_PRECISION ) );
   transfer( committee_account, bob_id, asset( 500000*GRAPHENE_BLOCKCHAIN_PRECISION ) );

   generate_block();
   set_expiration(db, trx);

   std::string test_url = "https://create_son_test";

   // create deposit vesting
   vesting_balance_id_type deposit_alice;
   {
      vesting_balance_create_operation op;
      op.creator = alice_id;
      op.owner = alice_id;
      op.amount = asset(500 * GRAPHENE_BLOCKCHAIN_PRECISION);
      op.balance_type = vesting_balance_type::son;
      op.policy = dormant_vesting_policy_initializer {};
      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      trx.clear();
      deposit_alice = ptx.operation_results[0].get<object_id_type>();
   }
   generate_block();
   set_expiration(db, trx);

   // create payment normal vesting
   vesting_balance_id_type payment_alice;
   {
      vesting_balance_create_operation op;
      op.creator = alice_id;
      op.owner = alice_id;
      op.amount = asset(500 * GRAPHENE_BLOCKCHAIN_PRECISION);
      op.balance_type = vesting_balance_type::normal;
      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      trx.clear();
      payment_alice = ptx.operation_results[0].get<object_id_type>();
   }

   generate_block();
   set_expiration(db, trx);

   // alice becomes son
   {
      flat_map<graphene::peerplays_sidechain::sidechain_type, string> sidechain_public_keys;
      sidechain_public_keys[graphene::peerplays_sidechain::sidechain_type::bitcoin] = "bitcoin address";

      son_create_operation op;
      op.owner_account = alice_id;
      op.url = test_url;
      op.deposit = deposit_alice;
      op.pay_vb = payment_alice;
      op.signing_key = alice_public_key;
      op.sidechain_public_keys = sidechain_public_keys;
      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
      trx.clear();
   }
   generate_block();
   set_expiration(db, trx);

   // create deposit vesting
   vesting_balance_id_type deposit_bob;
   {
      vesting_balance_create_operation op;
      op.creator = bob_id;
      op.owner = bob_id;
      op.amount = asset(500 * GRAPHENE_BLOCKCHAIN_PRECISION);
      op.balance_type = vesting_balance_type::son;
      op.policy = dormant_vesting_policy_initializer {};
      trx.operations.push_back(op);
      sign(trx, bob_private_key);
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      trx.clear();
      deposit_bob = ptx.operation_results[0].get<object_id_type>();
   }
   generate_block();
   set_expiration(db, trx);

   // create payment normal vesting
   vesting_balance_id_type payment_bob ;
   {
      vesting_balance_create_operation op;
      op.creator = bob_id;
      op.owner = bob_id;
      op.amount = asset(500 * GRAPHENE_BLOCKCHAIN_PRECISION);
      op.balance_type = vesting_balance_type::normal;
      trx.operations.push_back(op);
      sign(trx, bob_private_key);
      processed_transaction ptx = PUSH_TX(db, trx, ~0);
      trx.clear();
      payment_bob = ptx.operation_results[0].get<object_id_type>();
   }
   generate_block();
   set_expiration(db, trx);

   // bob becomes son
   {
      flat_map<graphene::peerplays_sidechain::sidechain_type, string> sidechain_public_keys;
      sidechain_public_keys[graphene::peerplays_sidechain::sidechain_type::bitcoin] = "bitcoin address";

      son_create_operation op;
      op.owner_account = bob_id;
      op.url = test_url;
      op.deposit = deposit_bob;
      op.pay_vb = payment_bob;
      op.signing_key = bob_public_key;
      op.sidechain_public_keys = sidechain_public_keys;
      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
      trx.clear();
   }
   generate_block();
   set_expiration(db, trx);

   generate_blocks(60);
   set_expiration(db, trx);

   {
      BOOST_TEST_MESSAGE("Send son_wallet_recreate_operation");

      son_wallet_recreate_operation op;

      op.payer = db.get_global_properties().parameters.get_son_btc_account_id();

      {
         son_info si;
         si.son_id = son_id_type(0);
         si.total_votes = 1000;
         si.signing_key = alice_public_key;
         si.sidechain_public_keys[peerplays_sidechain::sidechain_type::bitcoin] = "";
         op.sons.push_back(si);
      }

      {
         son_info si;
         si.son_id = son_id_type(1);
         si.total_votes = 1000;
         si.signing_key = bob_public_key;
         si.sidechain_public_keys[peerplays_sidechain::sidechain_type::bitcoin] = "";
         op.sons.push_back(si);
      }

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   BOOST_TEST_MESSAGE("Check son_wallet_recreate_operation results");

   const auto& idx = db.get_index_type<son_wallet_index>().indices().get<by_id>();
   BOOST_REQUIRE( idx.size() == 1 );
   auto obj = idx.find(son_wallet_id_type(0));
   BOOST_REQUIRE( obj != idx.end() );
   BOOST_REQUIRE( obj->expires == time_point_sec::maximum() );
}

BOOST_AUTO_TEST_CASE( son_wallet_update_test ) {

   BOOST_TEST_MESSAGE("son_wallet_update_test");

   INVOKE(son_wallet_recreate_test);
   GET_ACTOR(alice);

   {
      BOOST_TEST_MESSAGE("Send son_wallet_update_operation");

      son_wallet_update_operation op;

      op.payer = db.get_global_properties().parameters.get_son_btc_account_id();
      op.son_wallet_id = son_wallet_id_type(0);
      op.sidechain = graphene::peerplays_sidechain::sidechain_type::bitcoin;
      op.address = "bitcoin address";

      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX(db, trx, ~0);
   }
   generate_block();

   {
      BOOST_TEST_MESSAGE("Check son_wallet_update_operation results");

      const auto& idx = db.get_index_type<son_wallet_index>().indices().get<by_id>();
      BOOST_REQUIRE( idx.size() == 1 );
      auto obj = idx.find(son_wallet_id_type(0));
      BOOST_REQUIRE( obj != idx.end() );
      BOOST_REQUIRE( obj->addresses.at(graphene::peerplays_sidechain::sidechain_type::bitcoin) == "bitcoin address" );
   }

}

BOOST_AUTO_TEST_SUITE_END()
