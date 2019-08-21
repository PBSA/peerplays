/*
 * Copyright (c) 2017 PBSA, Inc., and contributors.
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


#include <fc/crypto/digest.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/reflect/variant.hpp>

#include "../common/database_fixture.hpp"

#include <cstring>

using namespace graphene::chain;

BOOST_FIXTURE_TEST_SUITE( lottery_tests, database_fixture )

BOOST_AUTO_TEST_CASE( create_lottery_asset_test )
{
   try {
      generate_block();
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      lottery_asset_create_operation creator;
      creator.issuer = account_id_type();
      creator.fee = asset();
      char symbol[5] = "LOT";
      symbol[3] = (char)('A' - 1 + test_asset_id.instance.value); symbol[4] = '\0'; // symbol depending on asset_id
      creator.symbol = symbol;
      creator.common_options.max_supply = 200;
      creator.precision = 0;
      creator.common_options.market_fee_percent = GRAPHENE_MAX_MARKET_FEE_PERCENT/100; /*1%*/
      creator.common_options.issuer_permissions = charge_market_fee|white_list|override_authority|transfer_restricted|disable_confidential;
      creator.common_options.flags = charge_market_fee|white_list|override_authority|disable_confidential;
      creator.common_options.core_exchange_rate = price({asset(1),asset(1,asset_id_type(1))});
      creator.common_options.whitelist_authorities = creator.common_options.blacklist_authorities = {account_id_type()};
      
      lottery_asset_options lottery_options;
      lottery_options.benefactors.push_back( benefactor( account_id_type(), 25 * GRAPHENE_1_PERCENT ) );
      lottery_options.end_date = db.head_block_time() + fc::minutes(5);
      lottery_options.ticket_price = asset(100);
      lottery_options.winning_tickets = { 5 * GRAPHENE_1_PERCENT, 5 * GRAPHENE_1_PERCENT, 5 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT, 10 * GRAPHENE_1_PERCENT };
      lottery_options.is_active = test_asset_id.instance.value % 2;
      lottery_options.ending_on_soldout = true;

      creator.extensions = lottery_options;

      trx.operations.push_back(std::move(creator));
      PUSH_TX( db, trx, ~0 );
      generate_block();
      
      auto test_asset = test_asset_id(db);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
   
BOOST_AUTO_TEST_CASE( lottery_idx_test )
{
   try {
      // generate loterries with different end_dates and is_active_flag
      for( int i = 0; i < 26; ++i ) {
         generate_blocks(30);
         graphene::chain::test::set_expiration( db, trx );
         asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
         INVOKE( create_lottery_asset_test );
         auto test_asset = test_asset_id(db);
      }

      auto& test_asset_idx = db.get_index_type<asset_index>().indices().get<active_lotteries>();
      auto test_itr = test_asset_idx.begin();
      bool met_not_active = false;
      // check sorting
      while( test_itr != test_asset_idx.end() ) {
         if( !met_not_active && (!test_itr->is_lottery() || !test_itr->lottery_options->is_active) )
            met_not_active = true;
         FC_ASSERT( !met_not_active || met_not_active && (!test_itr->is_lottery() || !test_itr->lottery_options->is_active), "MET ACTIVE LOTTERY AFTER NOT ACTIVE" );
         ++test_itr;
      }
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( tickets_purchase_test )
{
   try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      INVOKE( create_lottery_asset_test );
      auto& test_asset = test_asset_id(db);

      ticket_purchase_operation tpo;
      tpo.fee = asset();
      tpo.buyer = account_id_type();
      tpo.lottery = test_asset.id;
      tpo.tickets_to_buy = 1;
      tpo.amount = asset(100);
      trx.operations.push_back(std::move(tpo));
      graphene::chain::test::set_expiration(db, trx);
      PUSH_TX( db, trx, ~0 );
      generate_block();
      trx.operations.clear();

      BOOST_CHECK( tpo.amount == db.get_balance( test_asset.get_id() ) );
      BOOST_CHECK( tpo.tickets_to_buy == get_balance( account_id_type(), test_asset.id ) );

   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( tickets_purchase_fail_test ) 
{
   try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      INVOKE( create_lottery_asset_test );
      auto& test_asset = test_asset_id(db);

      ticket_purchase_operation tpo;
      tpo.fee = asset();
      tpo.buyer = account_id_type();
      tpo.lottery = test_asset.id;
      tpo.tickets_to_buy = 2;
      tpo.amount = asset(100);
      trx.operations.push_back(tpo);
      BOOST_REQUIRE_THROW( PUSH_TX( db, trx, ~0 ), fc::exception ); // amount/tickets_to_buy != price
      trx.operations.clear();

      tpo.amount = asset(205);
      trx.operations.push_back(tpo);
      BOOST_REQUIRE_THROW( PUSH_TX( db, trx, ~0 ), fc::exception ); // amount/tickets_to_buy != price

      tpo.amount = asset(200, test_asset.id);
      trx.operations.push_back(tpo);
      BOOST_REQUIRE_THROW( PUSH_TX( db, trx, ~0 ), fc::exception ); // trying to buy in other asset
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( lottery_end_by_stage_test )
{
    try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      INVOKE( create_lottery_asset_test );
      auto test_asset = test_asset_id(db);
      for( int i = 1; i < 17; ++i ) {
         if( i == 4 || i == 1 || i == 16 || i == 15 ) continue;
         if( i != 0 )
            transfer(account_id_type(), account_id_type(i), asset(100000));
         ticket_purchase_operation tpo;
         tpo.fee = asset();
         tpo.buyer = account_id_type(i);
         tpo.lottery = test_asset.id;
         tpo.tickets_to_buy = i;
         tpo.amount = asset(100 * (i));
         trx.operations.push_back(std::move(tpo));
         graphene::chain::test::set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         generate_block();
         trx.operations.clear();
      }
      test_asset = test_asset_id(db);
      uint64_t benefactor_balance_before_end = db.get_balance( account_id_type(), asset_id_type() ).amount.value;
      uint64_t jackpot = db.get_balance( test_asset.get_id() ).amount.value;
      uint16_t winners_part = 0;
      for( uint16_t win: test_asset.lottery_options->winning_tickets )
         winners_part += win;
      
      uint16_t participants_percents_sum = 0;
      auto participants = test_asset.distribute_winners_part( db );
      for( auto p : participants )
         for( auto e : p.second)
            participants_percents_sum += e;
       
      BOOST_CHECK( participants_percents_sum == winners_part );
      BOOST_CHECK( db.get_balance( test_asset.get_id() ).amount.value == (jackpot * (GRAPHENE_100_PERCENT - winners_part) / (double)GRAPHENE_100_PERCENT) + jackpot *  winners_part * SWEEPS_DEFAULT_DISTRIBUTION_PERCENTAGE / (double)GRAPHENE_100_PERCENT / (double)GRAPHENE_100_PERCENT );
      test_asset.distribute_benefactors_part( db );
      BOOST_CHECK( db.get_balance( test_asset.get_id() ).amount.value ==  jackpot * SWEEPS_DEFAULT_DISTRIBUTION_PERCENTAGE / (double)GRAPHENE_100_PERCENT * winners_part / (double)GRAPHENE_100_PERCENT );
      test_asset.distribute_sweeps_holders_part( db );
      BOOST_CHECK( db.get_balance( test_asset.get_id() ).amount.value == 0 );
      
      uint64_t benefactor_recieved = db.get_balance( account_id_type(), asset_id_type() ).amount.value - benefactor_balance_before_end;
      test_asset = test_asset_id(db);
      BOOST_CHECK(jackpot * test_asset.lottery_options->benefactors[0].share / GRAPHENE_100_PERCENT == benefactor_recieved);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_AUTO_TEST_CASE( lottery_end_by_stage_with_fractional_test )
{

   try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      INVOKE( create_lottery_asset_test );
      db.modify(test_asset_id(db), [&](asset_object& ao) {
         ao.lottery_options->is_active = true;
      });
      auto test_asset = test_asset_id(db);
      for( int i = 1; i < 17; ++i ) {
         if( i == 4 ) continue;
         if( i != 0 )
            transfer(account_id_type(), account_id_type(i), asset(100000));
         ticket_purchase_operation tpo;
         tpo.fee = asset();
         tpo.buyer = account_id_type(i);
         tpo.lottery = test_asset.id;
         tpo.tickets_to_buy = i;
         tpo.amount = asset(100 * (i));
         trx.operations.push_back(std::move(tpo));
         graphene::chain::test::set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         generate_block();
         trx.operations.clear();
      }
      test_asset = test_asset_id(db);
      uint64_t benefactor_balance_before_end = db.get_balance( account_id_type(), asset_id_type() ).amount.value;
      uint64_t jackpot = db.get_balance( test_asset.get_id() ).amount.value;
      uint16_t winners_part = 0;
      for( uint16_t win: test_asset.lottery_options->winning_tickets )
         winners_part += win;
      
      uint16_t participants_percents_sum = 0;
      auto participants = test_asset.distribute_winners_part( db );
      for( auto p : participants )
         for( auto e : p.second)
            participants_percents_sum += e;
       
      BOOST_CHECK( participants_percents_sum == winners_part );
      // balance should be bigger than expected because of rouning during distribution
      BOOST_CHECK( db.get_balance( test_asset.get_id() ).amount.value > (jackpot * (GRAPHENE_100_PERCENT - winners_part) / (double)GRAPHENE_100_PERCENT) + jackpot *  winners_part * SWEEPS_DEFAULT_DISTRIBUTION_PERCENTAGE / (double)GRAPHENE_100_PERCENT / (double)GRAPHENE_100_PERCENT );
      test_asset.distribute_benefactors_part( db );
      BOOST_CHECK( db.get_balance( test_asset.get_id() ).amount.value >  jackpot * SWEEPS_DEFAULT_DISTRIBUTION_PERCENTAGE / (double)GRAPHENE_100_PERCENT * winners_part / (double)GRAPHENE_100_PERCENT );
      test_asset.distribute_sweeps_holders_part( db );
      // but at the end is always equals 0
      BOOST_CHECK( db.get_balance( test_asset.get_id() ).amount.value == 0 );
      
      uint64_t benefactor_recieved = db.get_balance( account_id_type(), asset_id_type() ).amount.value - benefactor_balance_before_end;
      test_asset = test_asset_id(db);
      BOOST_CHECK(jackpot * test_asset.lottery_options->benefactors[0].share / GRAPHENE_100_PERCENT == benefactor_recieved);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( lottery_end_test )
{
    try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      INVOKE( create_lottery_asset_test );
      auto test_asset = test_asset_id(db);
      for( int i = 1; i < 17; ++i ) {
         if( i == 4 ) continue;
         if( i != 0 )
            transfer(account_id_type(), account_id_type(i), asset(100000));
         ticket_purchase_operation tpo;
         tpo.fee = asset();
         tpo.buyer = account_id_type(i);
         tpo.lottery = test_asset.id;
         tpo.tickets_to_buy = i;
         tpo.amount = asset(100 * (i));
         trx.operations.push_back(std::move(tpo));
         graphene::chain::test::set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      }
      generate_block();
      test_asset = test_asset_id(db);
      uint64_t creator_balance_before_end = db.get_balance( account_id_type(), asset_id_type() ).amount.value;
      uint64_t jackpot = db.get_balance( test_asset.get_id() ).amount.value;
      uint16_t winners_part = 0;
      for( uint8_t win: test_asset.lottery_options->winning_tickets )
         winners_part += win;
       
      while( db.head_block_time() < ( test_asset.lottery_options->end_date + fc::seconds(30) ) )
         generate_block();
       
      BOOST_CHECK( db.get_balance( test_asset.get_id() ).amount.value == 0 );
      uint64_t creator_recieved = db.get_balance( account_id_type(), asset_id_type() ).amount.value - creator_balance_before_end;
      test_asset = test_asset_id(db);
      BOOST_CHECK(jackpot * test_asset.lottery_options->benefactors[0].share / GRAPHENE_100_PERCENT == creator_recieved);
   } catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( claim_sweeps_vesting_balance_test )
{
   try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      INVOKE( lottery_end_test );
      auto test_asset = test_asset_id(db);
      account_id_type benefactor = test_asset.lottery_options->benefactors[0].id;
      const auto& svbo_index = db.get_index_type<sweeps_vesting_balance_index>().indices().get<by_owner>();
      auto benefactor_svbo = svbo_index.find(benefactor);
      BOOST_CHECK( benefactor_svbo != svbo_index.end() );
      
      auto balance_before_claim = db.get_balance( benefactor, SWEEPS_DEFAULT_DISTRIBUTION_ASSET );
      auto available_for_claim = benefactor_svbo->available_for_claim();
      sweeps_vesting_claim_operation claim;
      claim.account = benefactor;
      claim.amount_to_claim = available_for_claim;
      trx.clear();
      graphene::chain::test::set_expiration(db, trx);
      trx.operations.push_back(claim);
      PUSH_TX( db, trx, ~0 );
      generate_block();
      
      BOOST_CHECK( db.get_balance( benefactor, SWEEPS_DEFAULT_DISTRIBUTION_ASSET ) - balance_before_claim == available_for_claim );
      benefactor_svbo = svbo_index.find(benefactor);
      BOOST_CHECK( benefactor_svbo->available_for_claim().amount == 0 );
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( more_winners_then_participants_test )
{
   try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      INVOKE( create_lottery_asset_test );
      auto test_asset = test_asset_id(db);
      for( int i = 1; i < 4; ++i ) {
         if( i == 4 ) continue;
         if( i != 0 )
            transfer(account_id_type(), account_id_type(i), asset(1000000));
         ticket_purchase_operation tpo;
         tpo.fee = asset();
         tpo.buyer = account_id_type(i);
         tpo.lottery = test_asset.id;
         tpo.tickets_to_buy = 1;
         tpo.amount = asset(100);
         trx.operations.push_back(std::move(tpo));
         graphene::chain::test::set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      }
      generate_block();
      test_asset = test_asset_id(db);
      auto holders = test_asset.get_holders(db);
      auto participants = test_asset.distribute_winners_part( db );
      test_asset.distribute_benefactors_part( db );
      test_asset.distribute_sweeps_holders_part( db );
      generate_block();
      for( auto p: participants ) {
         idump(( get_operation_history(p.first) ));
      }
      auto benefactor_history = get_operation_history( account_id_type() );
      for( auto h: benefactor_history ) {
         idump((h));
      }
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( ending_by_date_test )
{
   try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      INVOKE( create_lottery_asset_test );
      auto test_asset = test_asset_id(db);
      for( int i = 1; i < 4; ++i ) {
         if( i == 4 ) continue;
         if( i != 0 )
            transfer(account_id_type(), account_id_type(i), asset(1000000));
         ticket_purchase_operation tpo;
         tpo.fee = asset();
         tpo.buyer = account_id_type(i);
         tpo.lottery = test_asset.id;
         tpo.tickets_to_buy = 1;
         tpo.amount = asset(100);
         trx.operations.push_back(std::move(tpo));
         graphene::chain::test::set_expiration(db, trx);
         PUSH_TX( db, trx, ~0 );
         trx.operations.clear();
      }
      generate_block();
      test_asset = test_asset_id(db);
      auto holders = test_asset.get_holders(db);
      idump(( db.get_balance(test_asset.get_id()) ));
      while( db.head_block_time() < ( test_asset.lottery_options->end_date + fc::seconds(30) ) )
         generate_block();
      idump(( db.get_balance(test_asset.get_id()) ));
      vector<account_id_type> participants = { account_id_type(1), account_id_type(2), account_id_type(3) };
      for( auto p: participants ) {
         idump(( get_operation_history(p) ));
      }
      auto benefactor_history = get_operation_history( account_id_type() );
      for( auto h: benefactor_history ) {
         if( h.op.which() == operation::tag<lottery_reward_operation>::value ) {
            auto reward_op = h.op.get<lottery_reward_operation>();
            idump((reward_op));
            BOOST_CHECK( reward_op.is_benefactor_reward );
            BOOST_CHECK( reward_op.amount.amount.value == 75 );
            BOOST_CHECK( reward_op.amount.asset_id == test_asset.lottery_options->ticket_price.asset_id );
            break;
         }
      }
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( ending_by_participants_count_test )
{
   try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      INVOKE( create_lottery_asset_test );
      auto test_asset = test_asset_id(db);
      FC_ASSERT( test_asset.lottery_options->is_active );
      account_id_type buyer(3);
      transfer(account_id_type(), buyer, asset(10000000));
      ticket_purchase_operation tpo;
      tpo.fee = asset();
      tpo.buyer = buyer;
      tpo.lottery = test_asset.id;
      tpo.tickets_to_buy = 200;
      tpo.amount = asset(200 * 100);
      trx.operations.push_back(tpo);
      graphene::chain::test::set_expiration(db, trx);
      PUSH_TX( db, trx, ~0 );
      trx.operations.clear();
      generate_block();
      test_asset = test_asset_id(db);
      FC_ASSERT( !test_asset.lottery_options->is_active );
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_AUTO_TEST_CASE( try_to_end_empty_lottery_test )
{
   try {
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      INVOKE( create_lottery_asset_test );
      auto test_asset = test_asset_id(db);
      while( db.head_block_time() < ( test_asset.lottery_options->end_date + fc::seconds(30) ) )
         generate_block();
      test_asset = test_asset_id(db);
      BOOST_CHECK( !test_asset.lottery_options->is_active );
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
