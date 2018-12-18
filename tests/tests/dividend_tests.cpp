/*
 * Copyright (c) 2018 oxarbitrage and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( dividend_tests, database_fixture )

BOOST_AUTO_TEST_CASE( create_dividend_uia )
{
   using namespace graphene;
   try {
      BOOST_TEST_MESSAGE("Creating dividend holder asset");
      {
         asset_create_operation creator;
         creator.issuer = account_id_type();
         creator.fee = asset();
         creator.symbol = "DIVIDEND";
         creator.common_options.max_supply = 100000000;
         creator.precision = 2;
         creator.common_options.market_fee_percent = GRAPHENE_MAX_MARKET_FEE_PERCENT/100; /*1%*/
         creator.common_options.issuer_permissions = UIA_ASSET_ISSUER_PERMISSION_MASK;
         creator.common_options.flags = charge_market_fee;
         creator.common_options.core_exchange_rate = price({asset(2),asset(1,asset_id_type(1))});
         trx.operations.push_back(std::move(creator));
         set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      }

      BOOST_TEST_MESSAGE("Creating test accounts");
      create_account("alice");
      create_account("bob");
      create_account("carol");
      create_account("dave");
      create_account("frank");

      BOOST_TEST_MESSAGE("Creating test asset");
      {
         asset_create_operation creator;
         creator.issuer = account_id_type();
         creator.fee = asset();
         creator.symbol = "TESTB"; //cant use TEST
         creator.common_options.max_supply = 100000000;
         creator.precision = 2;
         creator.common_options.market_fee_percent = GRAPHENE_MAX_MARKET_FEE_PERCENT/100; /*1%*/
         creator.common_options.issuer_permissions = UIA_ASSET_ISSUER_PERMISSION_MASK;
         creator.common_options.flags = charge_market_fee;
         creator.common_options.core_exchange_rate = price({asset(2),asset(1,asset_id_type(1))});
         trx.operations.push_back(std::move(creator));
         set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      }
      generate_block();

      BOOST_TEST_MESSAGE("Funding asset fee pool");
      {
         asset_fund_fee_pool_operation fund_op;
         fund_op.from_account = account_id_type();
         fund_op.asset_id = get_asset("TESTB").id;
         fund_op.amount = 500000000;
         trx.operations.push_back(std::move(fund_op));
         set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      }

      // our DIVIDEND asset should not yet be a divdend asset
      const auto& dividend_holder_asset_object = get_asset("DIVIDEND");
      BOOST_CHECK(!dividend_holder_asset_object.dividend_data_id);

      BOOST_TEST_MESSAGE("Converting the new asset to a dividend holder asset");
      {
         asset_update_dividend_operation op;
         op.issuer = dividend_holder_asset_object.issuer;
         op.asset_to_update = dividend_holder_asset_object.id;
         op.new_options.next_payout_time = db.head_block_time() + fc::minutes(1);
         op.new_options.payout_interval = 60 * 60 * 24 * 3;

         trx.operations.push_back(op);
         set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      }
      generate_block();

      BOOST_TEST_MESSAGE("Verifying the dividend holder asset options");
      BOOST_REQUIRE(dividend_holder_asset_object.dividend_data_id);
      const auto& dividend_data = dividend_holder_asset_object.dividend_data(db);
      {
         BOOST_REQUIRE(dividend_data.options.payout_interval);
         BOOST_CHECK_EQUAL(*dividend_data.options.payout_interval, 60 * 60 * 24 * 3);
      }

      const account_object& dividend_distribution_account = dividend_data.dividend_distribution_account(db);
      BOOST_CHECK_EQUAL(dividend_distribution_account.name, "dividend-dividend-distribution");

      // db.modify( db.get_global_properties(), [&]( global_property_object& _gpo )
      // {
      //    _gpo.parameters.current_fees->get<asset_dividend_distribution_operation>().distribution_base_fee = 100;
      //    _gpo.parameters.current_fees->get<asset_dividend_distribution_operation>().distribution_fee_per_holder = 100;
      // } );


   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( test_update_dividend_interval )
{
   using namespace graphene;
   try {
      INVOKE( create_dividend_uia );

      const auto& dividend_holder_asset_object = get_asset("DIVIDEND");
      const auto& dividend_data = dividend_holder_asset_object.dividend_data(db);

      auto advance_to_next_payout_time = [&]() {
         // Advance to the next upcoming payout time
         BOOST_REQUIRE(dividend_data.options.next_payout_time);
         fc::time_point_sec next_payout_scheduled_time = *dividend_data.options.next_payout_time;
         // generate blocks up to the next scheduled time
         generate_blocks(next_payout_scheduled_time);
         // if the scheduled time fell on a maintenance interval, then we should have paid out.
         // if not, we need to advance to the next maintenance interval to trigger the payout
         if (dividend_data.options.next_payout_time)
         {
            // we know there was a next_payout_time set when we entered this, so if
            // it has been cleared, we must have already processed payouts, no need to
            // further advance time.
            BOOST_REQUIRE(dividend_data.options.next_payout_time);
            if (*dividend_data.options.next_payout_time == next_payout_scheduled_time)
               generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
            generate_block();   // get the maintenance skip slots out of the way
         }
      };

      BOOST_TEST_MESSAGE("Updating the payout interval");
      {
         asset_update_dividend_operation op;
         op.issuer = dividend_holder_asset_object.issuer;
         op.asset_to_update = dividend_holder_asset_object.id;
         op.new_options.next_payout_time = fc::time_point::now() + fc::minutes(1);
         op.new_options.payout_interval = 60 * 60 * 24; // 1 days
         trx.operations.push_back(op);
         set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      }
      generate_block();

      BOOST_TEST_MESSAGE("Verifying the updated dividend holder asset options");
      {
         BOOST_REQUIRE(dividend_data.options.payout_interval);
         BOOST_CHECK_EQUAL(*dividend_data.options.payout_interval, 60 * 60 * 24);
      }

      BOOST_TEST_MESSAGE("Removing the payout interval");
      {
         asset_update_dividend_operation op;
         op.issuer = dividend_holder_asset_object.issuer;
         op.asset_to_update = dividend_holder_asset_object.id;
         op.new_options.next_payout_time = dividend_data.options.next_payout_time;
         op.new_options.payout_interval = fc::optional<uint32_t>();
         trx.operations.push_back(op);
         set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      }
      generate_block();
      BOOST_CHECK(!dividend_data.options.payout_interval);
      advance_to_next_payout_time();
      BOOST_REQUIRE_MESSAGE(!dividend_data.options.next_payout_time, "A new payout was scheduled, but none should have been");
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( test_basic_dividend_distribution )
{
   using namespace graphene;
   try {
      INVOKE( create_dividend_uia );

      const auto& dividend_holder_asset_object = get_asset("DIVIDEND");
      const auto& dividend_data = dividend_holder_asset_object.dividend_data(db);
      const account_object& dividend_distribution_account = dividend_data.dividend_distribution_account(db);
      const account_object& alice = get_account("alice");
      const account_object& bob = get_account("bob");
      const account_object& carol = get_account("carol");
      const account_object& dave = get_account("dave");
      const account_object& frank = get_account("frank");
      const auto& test_asset_object = get_asset("TESTB");

      auto issue_asset_to_account = [&](const asset_object& asset_to_issue, const account_object& destination_account, int64_t amount_to_issue)
      {
         asset_issue_operation op;
         op.issuer = asset_to_issue.issuer;
         op.asset_to_issue = asset(amount_to_issue, asset_to_issue.id);
         op.issue_to_account = destination_account.id;
         trx.operations.push_back( op );
         set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      };

      auto verify_pending_balance = [&](const account_object& holder_account_obj, const asset_object& payout_asset_obj, int64_t expected_balance) {
         int64_t pending_balance = get_dividend_pending_payout_balance(dividend_holder_asset_object.id,
                                                                       holder_account_obj.id,
                                                                       payout_asset_obj.id);
         BOOST_CHECK_EQUAL(pending_balance, expected_balance);
      };

      auto advance_to_next_payout_time = [&]() {
         // Advance to the next upcoming payout time
         BOOST_REQUIRE(dividend_data.options.next_payout_time);
         fc::time_point_sec next_payout_scheduled_time = *dividend_data.options.next_payout_time;
         // generate blocks up to the next scheduled time
         generate_blocks(next_payout_scheduled_time);
         // if the scheduled time fell on a maintenance interval, then we should have paid out.
         // if not, we need to advance to the next maintenance interval to trigger the payout
         if (dividend_data.options.next_payout_time)
         {
            // we know there was a next_payout_time set when we entered this, so if
            // it has been cleared, we must have already processed payouts, no need to
            // further advance time.
            BOOST_REQUIRE(dividend_data.options.next_payout_time);
            if (*dividend_data.options.next_payout_time == next_payout_scheduled_time)
               generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
            generate_block();   // get the maintenance skip slots out of the way
         }
      };

      // the first test will be testing pending balances, so we need to hit a
      // maintenance interval that isn't the payout interval.  Payout is
      // every 3 days, maintenance interval is every 1 day.
      advance_to_next_payout_time();

      // Set up the first test, issue alice, bob, and carol each 100 DIVIDEND.
      // Then deposit 300 TEST in the distribution account, and see that they
      // each are credited 100 TEST.
      issue_asset_to_account(dividend_holder_asset_object, alice, 100000);
      issue_asset_to_account(dividend_holder_asset_object, bob, 100000);
      issue_asset_to_account(dividend_holder_asset_object, carol, 100000);

      BOOST_TEST_MESSAGE("Issuing 300 TEST to the dividend account");
      issue_asset_to_account(test_asset_object, dividend_distribution_account, 30000);

      generate_block();

      BOOST_TEST_MESSAGE( "Generating blocks until next maintenance interval" );
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();   // get the maintenance skip slots out of the way

      verify_pending_balance(alice, test_asset_object, 10000);
      verify_pending_balance(bob, test_asset_object, 10000);
      verify_pending_balance(carol, test_asset_object, 10000);

      // For the second test, issue carol more than the other two, so it's
      // alice: 100 DIVIDND, bob: 100 DIVIDEND, carol: 200 DIVIDEND
      // Then deposit 400 TEST in the distribution account, and see that alice
      // and bob are credited with 100 TEST, and carol gets 200 TEST
      BOOST_TEST_MESSAGE("Issuing carol twice as much of the holder asset");
      issue_asset_to_account(dividend_holder_asset_object, carol, 100000); // one thousand at two digits of precision
      issue_asset_to_account(test_asset_object, dividend_distribution_account, 40000); // one thousand at two digits of precision
      BOOST_TEST_MESSAGE( "Generating blocks until next maintenance interval" );
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();   // get the maintenance skip slots out of the way
      verify_pending_balance(alice, test_asset_object, 20000);
      verify_pending_balance(bob, test_asset_object, 20000);
      verify_pending_balance(carol, test_asset_object, 30000);

      fc::time_point_sec old_next_payout_scheduled_time = *dividend_data.options.next_payout_time;
      advance_to_next_payout_time();


      BOOST_REQUIRE_MESSAGE(dividend_data.options.next_payout_time, "No new payout was scheduled");
      BOOST_CHECK_MESSAGE(old_next_payout_scheduled_time != *dividend_data.options.next_payout_time,
                             "New payout was scheduled for the same time as the last payout");
      BOOST_CHECK_MESSAGE(old_next_payout_scheduled_time + *dividend_data.options.payout_interval == *dividend_data.options.next_payout_time,
                             "New payout was not scheduled for the expected time");

      auto verify_dividend_payout_operations = [&](const account_object& destination_account, const asset& expected_payout)
      {
         BOOST_TEST_MESSAGE("Verifying the virtual op was created");
         const account_transaction_history_index& hist_idx = db.get_index_type<account_transaction_history_index>();
         auto account_history_range = hist_idx.indices().get<by_seq>().equal_range(boost::make_tuple(destination_account.id));
         BOOST_REQUIRE(account_history_range.first != account_history_range.second);
         const operation_history_object& history_object = std::prev(account_history_range.second)->operation_id(db);
         const asset_dividend_distribution_operation& distribution_operation = history_object.op.get<asset_dividend_distribution_operation>();
         BOOST_CHECK(distribution_operation.account_id == destination_account.id);
         BOOST_CHECK(std::find(distribution_operation.amounts.begin(), distribution_operation.amounts.end(), expected_payout)
            != distribution_operation.amounts.end());
      };

      BOOST_TEST_MESSAGE("Verifying the payouts");
      BOOST_CHECK_EQUAL(get_balance(alice, test_asset_object), 20000);
      verify_dividend_payout_operations(alice, asset(20000, test_asset_object.id));
      verify_pending_balance(alice, test_asset_object, 0);

      BOOST_CHECK_EQUAL(get_balance(bob, test_asset_object), 20000);
      verify_dividend_payout_operations(bob, asset(20000, test_asset_object.id));
      verify_pending_balance(bob, test_asset_object, 0);

      BOOST_CHECK_EQUAL(get_balance(carol, test_asset_object), 30000);
      verify_dividend_payout_operations(carol, asset(30000, test_asset_object.id));
      verify_pending_balance(carol, test_asset_object, 0);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( test_basic_dividend_distribution_to_core_asset )
{
   using namespace graphene;
   try {
      BOOST_TEST_MESSAGE("Creating test accounts");
      create_account("alice");
      create_account("bob");
      create_account("carol");
      create_account("dave");
      create_account("frank");

      BOOST_TEST_MESSAGE("Creating test asset");
      {
         asset_create_operation creator;
         creator.issuer = account_id_type();
         creator.fee = asset();
         creator.symbol = "TESTB";
         creator.common_options.max_supply = 100000000;
         creator.precision = 2;
         creator.common_options.market_fee_percent = GRAPHENE_MAX_MARKET_FEE_PERCENT/100; /*1%*/
         creator.common_options.issuer_permissions = UIA_ASSET_ISSUER_PERMISSION_MASK;
         creator.common_options.flags = charge_market_fee;
         creator.common_options.core_exchange_rate = price({asset(2),asset(1,asset_id_type(1))});
         trx.operations.push_back(std::move(creator));
         set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      }
      generate_block();

      const auto& dividend_holder_asset_object = asset_id_type(0)(db);
      const auto& dividend_data = dividend_holder_asset_object.dividend_data(db);
      const account_object& dividend_distribution_account = dividend_data.dividend_distribution_account(db);
      const account_object& alice = get_account("alice");
      const account_object& bob = get_account("bob");
      const account_object& carol = get_account("carol");
      const account_object& dave = get_account("dave");
      const account_object& frank = get_account("frank");
      const auto& test_asset_object = get_asset("TESTB");

      auto issue_asset_to_account = [&](const asset_object& asset_to_issue, const account_object& destination_account, int64_t amount_to_issue)
      {
         asset_issue_operation op;
         op.issuer = asset_to_issue.issuer;
         op.asset_to_issue = asset(amount_to_issue, asset_to_issue.id);
         op.issue_to_account = destination_account.id;
         trx.operations.push_back( op );
         set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      };

      auto verify_pending_balance = [&](const account_object& holder_account_obj, const asset_object& payout_asset_obj, int64_t expected_balance) {
         int64_t pending_balance = get_dividend_pending_payout_balance(dividend_holder_asset_object.id,
                                                                       holder_account_obj.id,
                                                                       payout_asset_obj.id);
         BOOST_CHECK_EQUAL(pending_balance, expected_balance);
      };

      auto advance_to_next_payout_time = [&]() {
         // Advance to the next upcoming payout time
         BOOST_REQUIRE(dividend_data.options.next_payout_time);
         fc::time_point_sec next_payout_scheduled_time = *dividend_data.options.next_payout_time;
         idump((next_payout_scheduled_time));
         // generate blocks up to the next scheduled time
         generate_blocks(next_payout_scheduled_time);
         // if the scheduled time fell on a maintenance interval, then we should have paid out.
         // if not, we need to advance to the next maintenance interval to trigger the payout
         if (dividend_data.options.next_payout_time)
         {
            // we know there was a next_payout_time set when we entered this, so if
            // it has been cleared, we must have already processed payouts, no need to
            // further advance time.
            BOOST_REQUIRE(dividend_data.options.next_payout_time);
            if (*dividend_data.options.next_payout_time == next_payout_scheduled_time)
               generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
            generate_block();   // get the maintenance skip slots out of the way
         }
         idump((db.head_block_time()));
      };

      // the first test will be testing pending balances, so we need to hit a
      // maintenance interval that isn't the payout interval.  Payout is
      // every 3 days, maintenance interval is every 1 day.
      advance_to_next_payout_time();

      // Set up the first test, issue alice, bob, and carol, and dave each 1/4 of the total
      // supply of the core asset.
      // Then deposit 400 TEST in the distribution account, and see that they
      // each are credited 100 TEST.
      transfer( committee_account(db), alice, asset( 250000000000000 ) );
      transfer( committee_account(db), bob,   asset( 250000000000000 ) );
      transfer( committee_account(db), carol, asset( 250000000000000 ) );
      transfer( committee_account(db), dave,  asset( 250000000000000 ) );

      BOOST_TEST_MESSAGE("Issuing 300 TEST to the dividend account");
      issue_asset_to_account(test_asset_object, dividend_distribution_account, 40000);

      generate_block();

      BOOST_TEST_MESSAGE( "Generating blocks until next maintenance interval" );
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();   // get the maintenance skip slots out of the way

      verify_pending_balance(alice, test_asset_object, 10000);
      verify_pending_balance(bob, test_asset_object, 10000);
      verify_pending_balance(carol, test_asset_object, 10000);
      verify_pending_balance(dave, test_asset_object, 10000);

      // For the second test, issue dave more than the other two, so it's
      // alice: 1/5 CORE, bob: 1/5 CORE, carol: 1/5 CORE, dave: 2/5 CORE
      // Then deposit 500 TEST in the distribution account, and see that alice
      // bob, and carol are credited with 100 TEST, and dave gets 200 TEST
      BOOST_TEST_MESSAGE("Issuing dave twice as much of the holder asset");
      transfer( alice, dave, asset( 50000000000000 ) );
      transfer( bob, dave,   asset( 50000000000000 ) );
      transfer( carol, dave, asset( 50000000000000 ) );
      issue_asset_to_account(test_asset_object, dividend_distribution_account, 50000); // 500 at two digits of precision
      BOOST_TEST_MESSAGE( "Generating blocks until next maintenance interval" );
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();   // get the maintenance skip slots out of the way
      verify_pending_balance(alice, test_asset_object, 20000);
      verify_pending_balance(bob, test_asset_object, 20000);
      verify_pending_balance(carol, test_asset_object, 20000);
      verify_pending_balance(dave, test_asset_object, 30000);

      fc::time_point_sec old_next_payout_scheduled_time = *dividend_data.options.next_payout_time;
      advance_to_next_payout_time();


      BOOST_REQUIRE_MESSAGE(dividend_data.options.next_payout_time, "No new payout was scheduled");
      BOOST_CHECK_MESSAGE(old_next_payout_scheduled_time != *dividend_data.options.next_payout_time,
                             "New payout was scheduled for the same time as the last payout");
      BOOST_CHECK_MESSAGE(old_next_payout_scheduled_time + *dividend_data.options.payout_interval == *dividend_data.options.next_payout_time,
                             "New payout was not scheduled for the expected time");

      auto verify_dividend_payout_operations = [&](const account_object& destination_account, const asset& expected_payout)
      {
         BOOST_TEST_MESSAGE("Verifying the virtual op was created");
         const account_transaction_history_index& hist_idx = db.get_index_type<account_transaction_history_index>();
         auto account_history_range = hist_idx.indices().get<by_seq>().equal_range(boost::make_tuple(destination_account.id));
         BOOST_REQUIRE(account_history_range.first != account_history_range.second);
         const operation_history_object& history_object = std::prev(account_history_range.second)->operation_id(db);
         const asset_dividend_distribution_operation& distribution_operation = history_object.op.get<asset_dividend_distribution_operation>();
         BOOST_CHECK(distribution_operation.account_id == destination_account.id);
         BOOST_CHECK(std::find(distribution_operation.amounts.begin(), distribution_operation.amounts.end(), expected_payout)
            != distribution_operation.amounts.end());
      };

      BOOST_TEST_MESSAGE("Verifying the payouts");
      BOOST_CHECK_EQUAL(get_balance(alice, test_asset_object), 20000);
      verify_dividend_payout_operations(alice, asset(20000, test_asset_object.id));
      verify_pending_balance(alice, test_asset_object, 0);

      BOOST_CHECK_EQUAL(get_balance(bob, test_asset_object), 20000);
      verify_dividend_payout_operations(bob, asset(20000, test_asset_object.id));
      verify_pending_balance(bob, test_asset_object, 0);

      BOOST_CHECK_EQUAL(get_balance(carol, test_asset_object), 20000);
      verify_dividend_payout_operations(carol, asset(20000, test_asset_object.id));
      verify_pending_balance(carol, test_asset_object, 0);

      BOOST_CHECK_EQUAL(get_balance(dave, test_asset_object), 30000);
      verify_dividend_payout_operations(dave, asset(30000, test_asset_object.id));
      verify_pending_balance(dave, test_asset_object, 0);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( test_dividend_distribution_interval )
{
   using namespace graphene;
   try {
      INVOKE( create_dividend_uia );

      const auto& dividend_holder_asset_object = get_asset("DIVIDEND");
      const auto& dividend_data = dividend_holder_asset_object.dividend_data(db);
      const account_object& dividend_distribution_account = dividend_data.dividend_distribution_account(db);
      const account_object& alice = get_account("alice");
      const account_object& bob = get_account("bob");
      const account_object& carol = get_account("carol");
      const account_object& dave = get_account("dave");
      const account_object& frank = get_account("frank");
      const auto& test_asset_object = get_asset("TESTB");
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_AUTO_TEST_CASE( check_dividend_corner_cases )
{
   using namespace graphene;
   try {
      INVOKE( create_dividend_uia );

      const auto& dividend_holder_asset_object = get_asset("DIVIDEND");
      const auto& dividend_data = dividend_holder_asset_object.dividend_data(db);
      const account_object& dividend_distribution_account = dividend_data.dividend_distribution_account(db);
      const account_object& alice = get_account("alice");
      const account_object& bob = get_account("bob");
      const account_object& carol = get_account("carol");
      const account_object& dave = get_account("dave");
      const account_object& frank = get_account("frank");
      const auto& test_asset_object = get_asset("TESTB");

      auto issue_asset_to_account = [&](const asset_object& asset_to_issue, const account_object& destination_account, int64_t amount_to_issue)
      {
         asset_issue_operation op;
         op.issuer = asset_to_issue.issuer;
         op.asset_to_issue = asset(amount_to_issue, asset_to_issue.id);
         op.issue_to_account = destination_account.id;
         trx.operations.push_back( op );
         set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      };

      auto verify_pending_balance = [&](const account_object& holder_account_obj, const asset_object& payout_asset_obj, int64_t expected_balance) {
         int64_t pending_balance = get_dividend_pending_payout_balance(dividend_holder_asset_object.id,
                                                                       holder_account_obj.id,
                                                                       payout_asset_obj.id);
         BOOST_CHECK_EQUAL(pending_balance, expected_balance);
      };

      auto reserve_asset_from_account = [&](const asset_object& asset_to_reserve, const account_object& from_account, int64_t amount_to_reserve)
      {
         asset_reserve_operation reserve_op;
         reserve_op.payer = from_account.id;
         reserve_op.amount_to_reserve = asset(amount_to_reserve, asset_to_reserve.id);
         trx.operations.push_back(reserve_op);
         set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      };
      auto advance_to_next_payout_time = [&]() {
         // Advance to the next upcoming payout time
         BOOST_REQUIRE(dividend_data.options.next_payout_time);
         fc::time_point_sec next_payout_scheduled_time = *dividend_data.options.next_payout_time;
         // generate blocks up to the next scheduled time
         generate_blocks(next_payout_scheduled_time);
         // if the scheduled time fell on a maintenance interval, then we should have paid out.
         // if not, we need to advance to the next maintenance interval to trigger the payout
         if (dividend_data.options.next_payout_time)
         {
            // we know there was a next_payout_time set when we entered this, so if
            // it has been cleared, we must have already processed payouts, no need to
            // further advance time.
            BOOST_REQUIRE(dividend_data.options.next_payout_time);
            if (*dividend_data.options.next_payout_time == next_payout_scheduled_time)
               generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
            generate_block();   // get the maintenance skip slots out of the way
         }
      };

      // the first test will be testing pending balances, so we need to hit a
      // maintenance interval that isn't the payout interval.  Payout is
      // every 3 days, maintenance interval is every 1 day.
      advance_to_next_payout_time();

      BOOST_TEST_MESSAGE("Testing a payout interval when there are no users holding the dividend asset");
      BOOST_CHECK_EQUAL(get_balance(bob, dividend_holder_asset_object), 0);
      BOOST_CHECK_EQUAL(get_balance(bob, dividend_holder_asset_object), 0);
      BOOST_CHECK_EQUAL(get_balance(bob, dividend_holder_asset_object), 0);
      issue_asset_to_account(test_asset_object, dividend_distribution_account, 1000);
      BOOST_TEST_MESSAGE("Generating blocks until next maintenance interval");
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();   // get the maintenance skip slots out of the way
      BOOST_TEST_MESSAGE("Verify that no pending payments were scheduled");
      verify_pending_balance(alice, test_asset_object, 0);
      verify_pending_balance(bob, test_asset_object, 0);
      verify_pending_balance(carol, test_asset_object, 0);
      advance_to_next_payout_time();
      BOOST_TEST_MESSAGE("Verify that no actual payments took place");
      verify_pending_balance(alice, test_asset_object, 0);
      verify_pending_balance(bob, test_asset_object, 0);
      verify_pending_balance(carol, test_asset_object, 0);
      BOOST_CHECK_EQUAL(get_balance(alice, test_asset_object), 0);
      BOOST_CHECK_EQUAL(get_balance(bob, test_asset_object), 0);
      BOOST_CHECK_EQUAL(get_balance(carol, test_asset_object), 0);
      BOOST_CHECK_EQUAL(get_balance(dividend_distribution_account, test_asset_object), 1000);

      BOOST_TEST_MESSAGE("Now give alice a small balance and see that she takes it all");
      issue_asset_to_account(dividend_holder_asset_object, alice, 1);
      generate_block();
      BOOST_TEST_MESSAGE("Generating blocks until next maintenance interval");
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();   // get the maintenance skip slots out of the way
      BOOST_TEST_MESSAGE("Verify that no alice received her payment of the entire amount");
      verify_pending_balance(alice, test_asset_object, 1000);

      // Test that we can pay out the dividend asset itself
      issue_asset_to_account(dividend_holder_asset_object, bob, 1);
      issue_asset_to_account(dividend_holder_asset_object, carol, 1);
      issue_asset_to_account(dividend_holder_asset_object, dividend_distribution_account, 300);
      generate_block();
      BOOST_CHECK_EQUAL(get_balance(alice, dividend_holder_asset_object), 1);
      BOOST_CHECK_EQUAL(get_balance(bob, dividend_holder_asset_object), 1);
      BOOST_CHECK_EQUAL(get_balance(carol, dividend_holder_asset_object), 1);
      BOOST_TEST_MESSAGE("Generating blocks until next maintenance interval");
      generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
      generate_block();   // get the maintenance skip slots out of the way
      BOOST_TEST_MESSAGE("Verify that the dividend asset was shared out");
      verify_pending_balance(alice, dividend_holder_asset_object, 100);
      verify_pending_balance(bob, dividend_holder_asset_object, 100);
      verify_pending_balance(carol, dividend_holder_asset_object, 100);
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
BOOST_AUTO_TEST_SUITE_END()
