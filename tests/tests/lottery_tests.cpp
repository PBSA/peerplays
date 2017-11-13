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
      asset_id_type test_asset_id = db.get_index<asset_object>().get_next_id();
      asset_create_operation creator;
      creator.issuer = account_id_type();
      creator.fee = asset();
      char symbol[5] = "LOT";
      symbol[3] = (char)('A' - 1 + test_asset_id.instance.value); symbol[4] = '\0'; // symbol depending on asset_id
      creator.symbol = symbol;
      creator.common_options.max_supply = 20;
      creator.precision = 0;
      creator.common_options.market_fee_percent = GRAPHENE_MAX_MARKET_FEE_PERCENT/100; /*1%*/
      creator.common_options.issuer_permissions = charge_market_fee|white_list|override_authority|transfer_restricted|disable_confidential;
      creator.common_options.flags = charge_market_fee|white_list|override_authority|disable_confidential;
      creator.common_options.core_exchange_rate = price({asset(1),asset(1,asset_id_type(1))});
      creator.common_options.whitelist_authorities = creator.common_options.blacklist_authorities = {account_id_type()};
      
      lottery_asset_options lottery_options;
      lottery_options.benefactors.push_back( benefactor( account_id_type(), 0.5 ) );
      lottery_options.end_date = db.get_dynamic_global_properties().time + fc::minutes(5);
      lottery_options.ticket_price = asset(100);
      lottery_options.winning_tickets.push_back(0.5);
      lottery_options.is_active = test_asset_id.instance.value % 2;

      creator.extension = lottery_options;

      trx.operations.push_back(std::move(creator));
      PUSH_TX( db, trx, ~0 );
      auto test_asset = test_asset_id(db);
      // idump((test_asset.is_lottery()));
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
      trx.operations.clear();

      BOOST_CHECK( tpo.amount == test_asset.lottery_options->balance );
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

BOOST_AUTO_TEST_SUITE_END()
