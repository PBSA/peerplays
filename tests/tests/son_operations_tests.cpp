#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/son_proposal_object.hpp>
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
      op.payer = alice_id;

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
      op.payer = bob_id;

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
}

BOOST_AUTO_TEST_CASE( son_pay_test )
{
   try
   {
      const dynamic_global_property_object& dpo = db.get_dynamic_global_properties();
      const auto block_interval = db.get_global_properties().parameters.block_interval;
      BOOST_CHECK( dpo.son_budget.value == 0);
      generate_blocks(HARDFORK_SON_TIME);
      while (db.head_block_time() <= HARDFORK_SON_TIME) {
         generate_block();
      }
      generate_block();
      set_expiration(db, trx);

      ACTORS((alice)(bob));
      // Send some core to the actors
      transfer( committee_account, alice_id, asset( 20000 * 100000) );
      transfer( committee_account, bob_id, asset( 20000 * 100000) );

      generate_block();
      // Enable default fee schedule to collect fees
      enable_fees();
      // Make SON Budget small for testing purposes
      // Make witness budget zero so that amount can be allocated to SON
      db.modify( db.get_global_properties(), [&]( global_property_object& _gpo )
      {
         _gpo.parameters.extensions.value.son_pay_daily_max = 200;
         _gpo.parameters.witness_pay_per_block = 0;
      } );
      // Upgrades pay fee and this goes to reserve
      upgrade_to_lifetime_member(alice);
      upgrade_to_lifetime_member(bob);
      // Note payment time just to generate enough blocks to make budget
      auto pay_fee_time = db.head_block_time().sec_since_epoch();
      generate_block();
      // Do maintenance from the upcoming block
      auto schedule_maint = [&]()
      {
         db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& _dpo )
         {
            _dpo.next_maintenance_time = db.head_block_time() + 1;
         } );
      };

      // Generate enough blocks to make budget
      while( db.head_block_time().sec_since_epoch() - pay_fee_time < 100 * block_interval )
      {
         generate_block();
      }

      // Enough blocks generated schedule maintenance now
      schedule_maint();
      // This block triggers maintenance
      generate_block();

      // Check that the SON Budget is allocated and Witness budget is zero
      BOOST_CHECK( dpo.son_budget.value == 200);
      BOOST_CHECK( dpo.witness_budget.value == 0);

      // Now create SONs
      std::string test_url1 = "https://create_son_test1";
      std::string test_url2 = "https://create_son_test2";

      // create deposit vesting
      vesting_balance_id_type deposit1;
      {
         vesting_balance_create_operation op;
         op.creator = alice_id;
         op.owner = alice_id;
         op.amount = asset(50*GRAPHENE_BLOCKCHAIN_PRECISION);
         op.balance_type = vesting_balance_type::son;
         op.policy = dormant_vesting_policy_initializer {};

         trx.operations.push_back(op);
         for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
         set_expiration(db, trx);
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.clear();
         deposit1 = ptx.operation_results[0].get<object_id_type>();
      }

      // create payment vesting
      vesting_balance_id_type payment1;
      {
         vesting_balance_create_operation op;
         op.creator = alice_id;
         op.owner = alice_id;
         op.amount = asset(1*GRAPHENE_BLOCKCHAIN_PRECISION);
         op.balance_type = vesting_balance_type::normal;

         trx.operations.push_back(op);
         for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
         set_expiration(db, trx);
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.clear();
         payment1 = ptx.operation_results[0].get<object_id_type>();
      }

      // create deposit vesting
      vesting_balance_id_type deposit2;
      {
         vesting_balance_create_operation op;
         op.creator = bob_id;
         op.owner = bob_id;
         op.amount = asset(50*GRAPHENE_BLOCKCHAIN_PRECISION);
         op.balance_type = vesting_balance_type::son;
         op.policy = dormant_vesting_policy_initializer {};
         
         trx.operations.push_back(op);
         for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
         set_expiration(db, trx);
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.clear();
         deposit2 = ptx.operation_results[0].get<object_id_type>();
      }

      // create payment vesting
      vesting_balance_id_type payment2;
      {
         vesting_balance_create_operation op;
         op.creator = bob_id;
         op.owner = bob_id;
         op.amount = asset(1*GRAPHENE_BLOCKCHAIN_PRECISION);
         op.balance_type = vesting_balance_type::normal;
         
         trx.operations.push_back(op);
         for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
         set_expiration(db, trx);
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.clear();
         payment2 = ptx.operation_results[0].get<object_id_type>();
      }

      // alice becomes son
      {
         son_create_operation op;
         op.owner_account = alice_id;
         op.url = test_url1;
         op.deposit = deposit1;
         op.pay_vb = payment1;
         op.fee = asset(0);
         op.signing_key = alice_public_key;
         trx.operations.push_back(op);
         for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
         sign(trx, alice_private_key);
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      // bob becomes son
      {
         son_create_operation op;
         op.owner_account = bob_id;
         op.url = test_url2;
         op.deposit = deposit2;
         op.pay_vb = payment2;
         op.fee = asset(0);
         op.signing_key = bob_public_key;
         trx.operations.push_back(op);
         for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
         sign(trx, bob_private_key);
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      generate_block();
      // Check if SONs are created properly
      const auto& idx = db.get_index_type<son_index>().indices().get<by_account>();
      BOOST_REQUIRE( idx.size() == 2 );
      // Alice's SON
      auto obj1 = idx.find( alice_id );
      BOOST_REQUIRE( obj1 != idx.end() );
      BOOST_CHECK( obj1->url == test_url1 );
      BOOST_CHECK( obj1->signing_key == alice_public_key );
      BOOST_CHECK( obj1->deposit.instance == deposit1.instance.value );
      BOOST_CHECK( obj1->pay_vb.instance == payment1.instance.value );
      // Bob's SON
      auto obj2 = idx.find( bob_id );
      BOOST_REQUIRE( obj2 != idx.end() );
      BOOST_CHECK( obj2->url == test_url2 );
      BOOST_CHECK( obj2->signing_key == bob_public_key );
      BOOST_CHECK( obj2->deposit.instance == deposit2.instance.value );
      BOOST_CHECK( obj2->pay_vb.instance == payment2.instance.value );
      // Get the statistics object for the SONs
      const auto& sidx = db.get_index_type<son_stats_index>().indices().get<by_id>();
      BOOST_REQUIRE( sidx.size() == 2 );
      auto son_stats_obj1 = sidx.find( obj1->statistics );
      auto son_stats_obj2 = sidx.find( obj2->statistics );
      BOOST_REQUIRE( son_stats_obj1 != sidx.end() );
      BOOST_REQUIRE( son_stats_obj2 != sidx.end() );
      // Modify the transaction signed statistics of Alice's SON
      db.modify( *son_stats_obj1, [&]( son_statistics_object& _s)
      {
         _s.txs_signed = 2;
      });
      // Modify the transaction signed statistics of Bob's SON
      db.modify( *son_stats_obj2, [&]( son_statistics_object& _s)
      {
         _s.txs_signed = 3;
      });

      // Note the balances before the maintenance
      int64_t obj1_balance = db.get_balance(obj1->son_account, asset_id_type()).amount.value;
      int64_t obj2_balance = db.get_balance(obj2->son_account, asset_id_type()).amount.value;
      // Next maintenance triggerred
      generate_blocks(dpo.next_maintenance_time);
      generate_block();
      // Check if the signed transaction statistics are reset for both SONs
      BOOST_REQUIRE_EQUAL(son_stats_obj1->txs_signed, 0);
      BOOST_REQUIRE_EQUAL(son_stats_obj2->txs_signed, 0);
      // Check that Alice and Bob are paid for signing the transactions in the previous day/cycle
      BOOST_REQUIRE_EQUAL(db.get_balance(obj1->son_account, asset_id_type()).amount.value, 80+obj1_balance);
      BOOST_REQUIRE_EQUAL(db.get_balance(obj2->son_account, asset_id_type()).amount.value, 120+obj2_balance);
      // Check the SON Budget is again allocated after maintenance
      BOOST_CHECK( dpo.son_budget.value == 200);
      BOOST_CHECK( dpo.witness_budget.value == 0);
   }FC_LOG_AND_RETHROW()

   }

BOOST_AUTO_TEST_CASE( son_witness_proposal_test )
{
   try
   {
      const dynamic_global_property_object& dpo = db.get_dynamic_global_properties();
      generate_blocks(HARDFORK_SON_TIME);
      generate_block();
      generate_block();
      set_expiration(db, trx);

      ACTORS((alice)(bob));

      upgrade_to_lifetime_member(alice);
      upgrade_to_lifetime_member(bob);

      transfer( committee_account, alice_id, asset( 1000*GRAPHENE_BLOCKCHAIN_PRECISION ) );
      transfer( committee_account, bob_id, asset( 1000*GRAPHENE_BLOCKCHAIN_PRECISION ) );

      set_expiration(db, trx);
      generate_block();
      // Now create SONs
      std::string test_url1 = "https://create_son_test1";
      std::string test_url2 = "https://create_son_test2";

      // create deposit vesting
      vesting_balance_id_type deposit1;
      {
         vesting_balance_create_operation op;
         op.creator = alice_id;
         op.owner = alice_id;
         op.amount = asset(50*GRAPHENE_BLOCKCHAIN_PRECISION);
         op.balance_type = vesting_balance_type::son;
         op.policy = dormant_vesting_policy_initializer {};

         trx.operations.push_back(op);
         for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
         set_expiration(db, trx);
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.clear();
         deposit1 = ptx.operation_results[0].get<object_id_type>();
      }

      // create payment vesting
      vesting_balance_id_type payment1;
      {
         vesting_balance_create_operation op;
         op.creator = alice_id;
         op.owner = alice_id;
         op.amount = asset(1*GRAPHENE_BLOCKCHAIN_PRECISION);
         op.balance_type = vesting_balance_type::normal;

         trx.operations.push_back(op);
         for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
         set_expiration(db, trx);
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.clear();
         payment1 = ptx.operation_results[0].get<object_id_type>();
      }

      // create deposit vesting
      vesting_balance_id_type deposit2;
      {
         vesting_balance_create_operation op;
         op.creator = bob_id;
         op.owner = bob_id;
         op.amount = asset(50*GRAPHENE_BLOCKCHAIN_PRECISION);
         op.balance_type = vesting_balance_type::son;
         op.policy = dormant_vesting_policy_initializer {};

         trx.operations.push_back(op);
         for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
         set_expiration(db, trx);
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.clear();
         deposit2 = ptx.operation_results[0].get<object_id_type>();
      }

      // create payment vesting
      vesting_balance_id_type payment2;
      {
         vesting_balance_create_operation op;
         op.creator = bob_id;
         op.owner = bob_id;
         op.amount = asset(1*GRAPHENE_BLOCKCHAIN_PRECISION);
         op.balance_type = vesting_balance_type::normal;

         trx.operations.push_back(op);
         for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
         set_expiration(db, trx);
         processed_transaction ptx = PUSH_TX(db, trx, ~0);
         trx.clear();
         payment2 = ptx.operation_results[0].get<object_id_type>();
      }

      // alice becomes son
      {
         son_create_operation op;
         op.owner_account = alice_id;
         op.url = test_url1;
         op.deposit = deposit1;
         op.pay_vb = payment1;
         op.fee = asset(0);
         op.signing_key = alice_public_key;
         trx.operations.push_back(op);
         for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
         sign(trx, alice_private_key);
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      // bob becomes son
      {
         son_create_operation op;
         op.owner_account = bob_id;
         op.url = test_url2;
         op.deposit = deposit2;
         op.pay_vb = payment2;
         op.fee = asset(0);
         op.signing_key = bob_public_key;
         trx.operations.push_back(op);
         for( auto& op : trx.operations ) db.current_fee_schedule().set_fee(op);
         sign(trx, bob_private_key);
         PUSH_TX(db, trx, ~0);
         trx.clear();
      }

      generate_block();
      // Check if SONs are created properly
      const auto& idx = db.get_index_type<son_index>().indices().get<by_account>();
      BOOST_REQUIRE( idx.size() == 2 );
      // Alice's SON
      auto obj1 = idx.find( alice_id );
      BOOST_REQUIRE( obj1 != idx.end() );
      BOOST_CHECK( obj1->url == test_url1 );
      BOOST_CHECK( obj1->signing_key == alice_public_key );
      BOOST_CHECK( obj1->deposit.instance == deposit1.instance.value );
      BOOST_CHECK( obj1->pay_vb.instance == payment1.instance.value );
      // Bob's SON
      auto obj2 = idx.find( bob_id );
      BOOST_REQUIRE( obj2 != idx.end() );
      BOOST_CHECK( obj2->url == test_url2 );
      BOOST_CHECK( obj2->signing_key == bob_public_key );
      BOOST_CHECK( obj2->deposit.instance == deposit2.instance.value );
      BOOST_CHECK( obj2->pay_vb.instance == payment2.instance.value );
      // Get the statistics object for the SONs
      const auto& sidx = db.get_index_type<son_stats_index>().indices().get<by_id>();
      BOOST_REQUIRE( sidx.size() == 2 );
      auto son_stats_obj1 = sidx.find( obj1->statistics );
      auto son_stats_obj2 = sidx.find( obj2->statistics );
      BOOST_REQUIRE( son_stats_obj1 != sidx.end() );
      BOOST_REQUIRE( son_stats_obj2 != sidx.end() );


      // Modify SON's status to in_maintenance
      db.modify( *obj1, [&]( son_object& _s)
      {
         _s.status = son_status::in_maintenance;
      });

      // Modify the Alice's SON down timestamp to now-12 hours
      db.modify( *son_stats_obj1, [&]( son_statistics_object& _s)
      {
         _s.last_down_timestamp = fc::time_point_sec(db.head_block_time() - fc::hours(12));
      });

      // Modify SON's status to in_maintenance
      db.modify( *obj2, [&]( son_object& _s)
      {
         _s.status = son_status::in_maintenance;
      });

      // Modify the Bob's SON down timestamp to now-12 hours
      db.modify( *son_stats_obj2, [&]( son_statistics_object& _s)
      {
         _s.last_down_timestamp = fc::time_point_sec(db.head_block_time() - fc::hours(12));
      });

      const auto& son_proposal_idx = db.get_index_type<son_proposal_index>().indices().get<by_id>();
      const auto& proposal_idx = db.get_index_type<proposal_index>().indices().get<by_id>();

      BOOST_CHECK( son_proposal_idx.size() == 0 && proposal_idx.size() == 0 );

      generate_block();
      witness_id_type proposal_initiator = dpo.current_witness;

      BOOST_CHECK( son_proposal_idx.size() == 2 && proposal_idx.size() == 2 );

      for(size_t i = 0 ; i < 3 * db.get_global_properties().active_witnesses.size() ; i++ )
      {
         generate_block();
         if( dpo.current_witness != proposal_initiator)
         {
            BOOST_CHECK( son_proposal_idx.size() == 2 && proposal_idx.size() == 2 );
         }
         else
         {
            break;
         }
      }
      BOOST_CHECK( son_proposal_idx.size() == 0 && proposal_idx.size() == 0 );
      BOOST_REQUIRE( idx.size() == 0 );
      generate_block();
   } FC_LOG_AND_RETHROW()

} BOOST_AUTO_TEST_SUITE_END()
