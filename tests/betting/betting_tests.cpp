/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
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
#include <fc/crypto/openssl.hpp>
#include <openssl/rand.h>

#include "../common/database_fixture.hpp"
#include <graphene/utilities/tempdir.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/witness_object.hpp>

#include <graphene/chain/sport_object.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/event_group_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/betting_market_object.hpp>

#include <graphene/bookie/bookie_api.hpp>
//#include <boost/algorithm/string/replace.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

#define CREATE_ICE_HOCKEY_BETTING_MARKET() \
  const sport_object& ice_hockey = create_sport({{"en", "Ice Hockey"}, {"zh_Hans", "冰球"}, {"ja", "アイスホッケー"}}); \
  const event_group_object& nhl = create_event_group({{"en", "NHL"}, {"zh_Hans", "國家冰球聯盟"}, {"ja", "ナショナルホッケーリーグ"}}, ice_hockey.id); \
  const event_object& capitals_vs_blackhawks = create_event({{"en", "Washington Capitals/Chicago Blackhawks"}, {"zh_Hans", "華盛頓首都隊/芝加哥黑鷹"}, {"ja", "ワシントン・キャピタルズ/シカゴ・ブラックホークス"}}, {{"en", "2016-17"}}, nhl.id); \
  const betting_market_rules_object& betting_market_rules = create_betting_market_rules({{"en", "NHL Rules v1.0"}}, {{"en", "The winner will be the team with the most points at the end of the game.  The team with fewer points will not be the winner."}}); \
  const betting_market_group_object& moneyline_betting_markets = create_betting_market_group({{"en", "Moneyline"}}, capitals_vs_blackhawks.id, betting_market_rules.id, asset_id_type()); \
  const betting_market_object& capitals_win_market = create_betting_market(moneyline_betting_markets.id, {{"en", "Washington Capitals win"}}); \
  const betting_market_object& blackhawks_win_market = create_betting_market(moneyline_betting_markets.id, {{"en", "Chicago Blackhawks win"}});

BOOST_FIXTURE_TEST_SUITE( betting_tests, database_fixture )

#if 0
BOOST_AUTO_TEST_CASE(generate_block)
{
   try
   {
      ACTORS( (alice)(bob) );

      // failure if ACTORS
      generate_blocks(10);

   } FC_LOG_AND_RETHROW()
}
#endif

BOOST_AUTO_TEST_CASE(try_create_sport)
{
   try
   {
        sport_create_operation sport_create_op;
        sport_create_op.name = {{"en", "Ice Hockey"}, {"zh_Hans", "冰球"}, {"ja", "アイスホッケー"}};

        proposal_id_type proposal_id = propose_operation(sport_create_op);

        const auto& active_witnesses = db.get_global_properties().active_witnesses;
        int voting_count = active_witnesses.size() / 2;

        // 5 for
        std::vector<witness_id_type> witnesses;
        for (const witness_id_type& witness_id : active_witnesses)
        {
            witnesses.push_back(witness_id);
            if (--voting_count == 0)
                break;
        }
        process_proposal_by_witnesses(witnesses, proposal_id);

        // 1st out
        witnesses.clear();
        auto itr = active_witnesses.begin();
        witnesses.push_back(*itr);
        process_proposal_by_witnesses(witnesses, proposal_id, true);

        const auto& sport_index = db.get_index_type<sport_object_index>().indices().get<by_id>();
        // not yet approved
        BOOST_REQUIRE(sport_index.rbegin() == sport_index.rend());

        // 6th for
        witnesses.clear();
        itr += 5;
        witnesses.push_back(*itr);
        process_proposal_by_witnesses(witnesses, proposal_id);

        // not yet approved
        BOOST_REQUIRE(sport_index.rbegin() == sport_index.rend());

        // 7th for
        witnesses.clear();
        ++itr;
        witnesses.push_back(*itr);
        process_proposal_by_witnesses(witnesses, proposal_id);

        // done
        BOOST_REQUIRE(sport_index.rbegin() != sport_index.rend());
        sport_id_type sport_id = (*sport_index.rbegin()).id;
        BOOST_REQUIRE(sport_id == sport_id_type());

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(simple_bet_win)
{
   try
   {
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET();

      // give alice and bob 10k each
      transfer(account_id_type(), alice_id, asset(10000));
      transfer(account_id_type(), bob_id, asset(10000));

      // place bets at 10:1
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 11 * GRAPHENE_BETTING_ODDS_PRECISION);
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000, asset_id_type()), 11 * GRAPHENE_BETTING_ODDS_PRECISION);

      // reverse positions at 1:1
      place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(1100, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(1100, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(binned_order_books)
{
   try
   {
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET();

      graphene::bookie::bookie_api bookie_api(app);

      // give alice and bob 10k each
      transfer(account_id_type(), alice_id, asset(10000));
      transfer(account_id_type(), bob_id, asset(10000));

      // place back bets at decimal odds of 1.55, 1.6, 1.65, 1.66, and 1.67
      place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(100, asset_id_type()), 155 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(100, asset_id_type()), 16 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(100, asset_id_type()), 165 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(100, asset_id_type()), 166 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(100, asset_id_type()), 167 * GRAPHENE_BETTING_ODDS_PRECISION / 100);

      const auto& bet_odds_idx = db.get_index_type<bet_object_index>().indices().get<by_odds>();

      auto bet_iter = bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id));
      while (bet_iter != bet_odds_idx.end() && 
             bet_iter->betting_market_id == capitals_win_market.id)
      {
        idump((*bet_iter));
        ++bet_iter;
      }

      graphene::bookie::binned_order_book binned_orders_point_one = bookie_api.get_binned_order_book(capitals_win_market.id, 1);
      idump((binned_orders_point_one));

      // the binned orders returned should be chosen so that we if we assume those orders are real and we place
      // matching lay orders, we will completely consume the underlying orders and leave no orders on the books
      BOOST_CHECK_EQUAL(binned_orders_point_one.aggregated_back_bets.size(), 2);
      BOOST_CHECK_EQUAL(binned_orders_point_one.aggregated_lay_bets.size(), 0);
      for (const graphene::bookie::order_bin& binned_order : binned_orders_point_one.aggregated_back_bets)
      {
        // compute the matching lay order
        share_type lay_amount = bet_object::get_approximate_matching_amount(binned_order.amount_to_bet, binned_order.backer_multiplier, bet_type::back, false /* round down */);
        ilog("Alice is laying with ${lay_amount} at odds ${odds} to match the binned back amount ${back_amount}", ("lay_amount", lay_amount)("odds", binned_order.backer_multiplier)("back_amount", binned_order.amount_to_bet));
        place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(lay_amount, asset_id_type()), binned_order.backer_multiplier);
      }

      bet_iter = bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id));
      while (bet_iter != bet_odds_idx.end() && 
             bet_iter->betting_market_id == capitals_win_market.id)
      {
        idump((*bet_iter));
        ++bet_iter;
      }

      BOOST_CHECK(bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id)) == bet_odds_idx.end());

      // place lay bets at decimal odds of 1.55, 1.6, 1.65, 1.66, and 1.67
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 155 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 16 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 165 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 166 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 167 * GRAPHENE_BETTING_ODDS_PRECISION / 100);

      binned_orders_point_one = bookie_api.get_binned_order_book(capitals_win_market.id, 1);
      idump((binned_orders_point_one));

      // the binned orders returned should be chosen so that we if we assume those orders are real and we place
      // matching lay orders, we will completely consume the underlying orders and leave no orders on the books
      BOOST_CHECK_EQUAL(binned_orders_point_one.aggregated_back_bets.size(), 0);
      BOOST_CHECK_EQUAL(binned_orders_point_one.aggregated_lay_bets.size(), 2);
      for (const graphene::bookie::order_bin& binned_order : binned_orders_point_one.aggregated_lay_bets)
      {
        // compute the matching lay order
        share_type back_amount = bet_object::get_approximate_matching_amount(binned_order.amount_to_bet, binned_order.backer_multiplier, bet_type::lay, false /* round down */);
        ilog("Alice is backing with ${back_amount} at odds ${odds} to match the binned lay amount ${lay_amount}", ("back_amount", back_amount)("odds", binned_order.backer_multiplier)("lay_amount", binned_order.amount_to_bet));
        place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(back_amount, asset_id_type()), binned_order.backer_multiplier);
      }

      bet_iter = bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id));
      while (bet_iter != bet_odds_idx.end() && 
             bet_iter->betting_market_id == capitals_win_market.id)
      {
        idump((*bet_iter));
        ++bet_iter;
      }

      BOOST_CHECK(bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id)) == bet_odds_idx.end());

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( peerplays_sport_create_test )
{
   try
   {
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET();

      // give alice and bob 10M each
      transfer(account_id_type(), alice_id, asset(10000000));
      transfer(account_id_type(), bob_id, asset(10000000));

      // have bob lay a bet for 1M (+20k fees) at 1:1 odds                  
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      // have alice back a matching bet at 1:1 odds (also costing 1.02M) 
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

      // caps win
      resolve_betting_market_group(moneyline_betting_markets.id,
                                  {{capitals_win_market.id, betting_market_resolution_type::win},
                                   {blackhawks_win_market.id, betting_market_resolution_type::cancel}});


      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage;
      uint32_t rake_value = (-1000000 + 2000000) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      BOOST_TEST_MESSAGE("Rake value " +  std::to_string(rake_value));
      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 + 2000000 - rake_value);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( cancel_unmatched_in_betting_group_test )
{
   try
   {
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET();

      // give alice and bob 10M each
      transfer(account_id_type(), alice_id, asset(10000000));
      transfer(account_id_type(), bob_id, asset(10000000));

      // have bob lay a bet for 1M (+20k fees) at 1:1 odds
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      // have alice back a matching bet at 1:1 odds (also costing 1.02M)
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      // place unmatched
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(500, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      place_bet(bob_id, blackhawks_win_market.id, bet_type::lay, asset(600, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 - 500);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000 - 600);

      // cancel unmatched
      cancel_unmatched_bets(moneyline_betting_markets.id);

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(inexact_odds)
{
  try
  {
     ACTORS( (alice)(bob) );
     CREATE_ICE_HOCKEY_BETTING_MARKET();

     transfer(account_id_type(), alice_id, asset(10000000));
     share_type alice_expected_balance = 10000000;
     BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

     // lay 46 at 1.94 odds (50:47) -- this is too small to be placed on the books and there's
     // nothing for it to match, so it should be canceled
     place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(46, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
     BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

     // lay 47 at 1.94 odds (50:47) -- this is an exact amount, nothing surprising should happen here
     place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(47, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
     alice_expected_balance -= 47;
     BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

     // lay 100 at 1.91 odds (100:91) -- this is an inexact match, we should get refunded 9 and leave a bet for 91 on the books
     place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 191 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
     alice_expected_balance -= 91;
     BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);


     transfer(account_id_type(), bob_id, asset(10000000));
     share_type bob_expected_balance = 10000000;
     BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);

     // now have bob match it with a back of 300 at 1.91
     // This should: 
     //   match the full 47 @ 1.94 with 50
     //   match the full 91 @ 1.91 with 100
     //     leaving 150
     //     back bets at 100:91 must be a multiple of 100, so refund 50
     //   leaves a back bet of 100 @ 1.91 on the books
     place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(300, asset_id_type()), 191 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
     bob_expected_balance -= 250;
     BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(persistent_objects_test)
{
  try
  {
     ACTORS( (alice)(bob) );
     CREATE_ICE_HOCKEY_BETTING_MARKET();
     graphene::bookie::bookie_api bookie_api(app);

     transfer(account_id_type(), alice_id, asset(10000000));
     transfer(account_id_type(), bob_id, asset(10000000));
     share_type alice_expected_balance = 10000000;
     share_type bob_expected_balance = 10000000;
     BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);
     BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

     // lay 46 at 1.94 odds (50:47) -- this is too small to be placed on the books and there's
     // nothing for it to match, so it should be canceled
     bet_id_type automatically_canceled_bet_id = place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(46, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
     BOOST_CHECK_MESSAGE(!db.find(automatically_canceled_bet_id), "Bet should have been canceled, but the blockchain still knows about it");
     fc::variants objects_from_bookie = bookie_api.get_objects({automatically_canceled_bet_id});
     BOOST_REQUIRE_EQUAL(objects_from_bookie.size(), 1);
     BOOST_CHECK_MESSAGE(objects_from_bookie[0]["id"].as<bet_id_type>() == automatically_canceled_bet_id, "Bookie Plugin didn't return a deleted bet it");

     // lay 47 at 1.94 odds (50:47) -- this bet should go on the order books normally
     bet_id_type first_bet_on_books = place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(47, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
     BOOST_CHECK_MESSAGE(db.find(first_bet_on_books), "Bet should exist on the blockchain");
     objects_from_bookie = bookie_api.get_objects({first_bet_on_books});
     BOOST_REQUIRE_EQUAL(objects_from_bookie.size(), 1);
     BOOST_CHECK_MESSAGE(objects_from_bookie[0]["id"].as<bet_id_type>() == first_bet_on_books, "Bookie Plugin didn't return a bet that is currently on the books");

     // place a bet that exactly matches 'first_bet_on_books', should result in empty books (thus, no bet_objects from the blockchain)
     bet_id_type matching_bet = place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(50, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
     BOOST_CHECK_MESSAGE(!db.find(first_bet_on_books), "Bet should have been filled, but the blockchain still knows about it");
     BOOST_CHECK_MESSAGE(!db.find(matching_bet), "Bet should have been filled, but the blockchain still knows about it");

     objects_from_bookie = bookie_api.get_objects({first_bet_on_books, matching_bet});
     BOOST_REQUIRE_EQUAL(objects_from_bookie.size(), 2);
     BOOST_CHECK_MESSAGE(objects_from_bookie[0]["id"].as<bet_id_type>() == first_bet_on_books, "Bookie Plugin didn't return a bet that has been filled");
     BOOST_CHECK_MESSAGE(objects_from_bookie[1]["id"].as<bet_id_type>() == matching_bet, "Bookie Plugin didn't return a bet that has been filled");
  }
  FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_CASE( chained_market_create_test )
{
   // Often you will want to create several objects that reference each other at the same time.
   // To facilitate this, many of the betting market operations allow you to use "relative" object ids,
   // which let you can create, for example, an event in the 2nd operation in a transaction where the
   // event group id is set to the id of an event group created in the 1st operation in a tranasction.
   try
   {
      {
         const flat_set<witness_id_type>& active_witnesses = db.get_global_properties().active_witnesses;
         
         BOOST_TEST_MESSAGE("Creating a sport and competitors in the same proposal");
         {
            // operation 0 in the transaction
            sport_create_operation sport_create_op;
            sport_create_op.name.insert(internationalized_string_type::value_type("en", "Ice Hockey"));
            sport_create_op.name.insert(internationalized_string_type::value_type("zh_Hans", "冰球"));
            sport_create_op.name.insert(internationalized_string_type::value_type("ja", "アイスホッケー"));

            // operation 1
            event_group_create_operation event_group_create_op;
            event_group_create_op.name.insert(internationalized_string_type::value_type("en", "NHL"));
            event_group_create_op.name.insert(internationalized_string_type::value_type("zh_Hans", "國家冰球聯盟"));
            event_group_create_op.name.insert(internationalized_string_type::value_type("ja", "ナショナルホッケーリーグ"));
            event_group_create_op.sport_id = object_id_type(relative_protocol_ids, 0, 0);

            // operation 2
            // leave name and start time blank
            event_create_operation event_create_op;
            event_create_op.name = {{"en", "Washington Capitals/Chicago Blackhawks"}, {"zh_Hans", "華盛頓首都隊/芝加哥黑鷹"}, {"ja", "ワシントン・キャピタルズ/シカゴ・ブラックホークス"}};
            event_create_op.season.insert(internationalized_string_type::value_type("en", "2016-17"));
            event_create_op.event_group_id = object_id_type(relative_protocol_ids, 0, 1);

            // operation 3
            betting_market_rules_create_operation rules_create_op;
            rules_create_op.name = {{"en", "NHL Rules v1.0"}};
            rules_create_op.description = {{"en", "The winner will be the team with the most points at the end of the game.  The team with fewer points will not be the winner."}};

            // operation 4
            betting_market_group_create_operation betting_market_group_create_op;
            betting_market_group_create_op.description = {{"en", "Moneyline"}};
            betting_market_group_create_op.event_id = object_id_type(relative_protocol_ids, 0, 2);
            betting_market_group_create_op.rules_id = object_id_type(relative_protocol_ids, 0, 3);
            betting_market_group_create_op.asset_id = asset_id_type();

            // operation 5
            betting_market_create_operation caps_win_betting_market_create_op;
            caps_win_betting_market_create_op.group_id = object_id_type(relative_protocol_ids, 0, 4);
            caps_win_betting_market_create_op.payout_condition.insert(internationalized_string_type::value_type("en", "Washington Capitals win"));

            // operation 6
            betting_market_create_operation blackhawks_win_betting_market_create_op;
            blackhawks_win_betting_market_create_op.group_id = object_id_type(relative_protocol_ids, 0, 4);
            blackhawks_win_betting_market_create_op.payout_condition.insert(internationalized_string_type::value_type("en", "Chicago Blackhawks win"));


            proposal_create_operation proposal_op;
            proposal_op.fee_paying_account = (*active_witnesses.begin())(db).witness_account;
            proposal_op.proposed_ops.emplace_back(sport_create_op);
            proposal_op.proposed_ops.emplace_back(event_group_create_op);
            proposal_op.proposed_ops.emplace_back(event_create_op);
            proposal_op.proposed_ops.emplace_back(rules_create_op);
            proposal_op.proposed_ops.emplace_back(betting_market_group_create_op);
            proposal_op.proposed_ops.emplace_back(caps_win_betting_market_create_op);
            proposal_op.proposed_ops.emplace_back(blackhawks_win_betting_market_create_op);
            proposal_op.expiration_time =  db.head_block_time() + fc::days(1);

            signed_transaction tx;
            tx.operations.push_back(proposal_op);
            set_expiration(db, tx);
            sign(tx, init_account_priv_key);
            
            db.push_transaction(tx);
         }

         BOOST_REQUIRE_EQUAL(db.get_index_type<proposal_index>().indices().size(), 1);
         {
            const proposal_object& prop = *db.get_index_type<proposal_index>().indices().begin();

            for (const witness_id_type& witness_id : active_witnesses)
            {
               BOOST_TEST_MESSAGE("Approving sport+competitors creation from witness " << fc::variant(witness_id).as<std::string>());
               const witness_object& witness = witness_id(db);
               const account_object& witness_account = witness.witness_account(db);

               proposal_update_operation pup;
               pup.proposal = prop.id;
               pup.fee_paying_account = witness_account.id;
               //pup.key_approvals_to_add.insert(witness.signing_key);
               pup.active_approvals_to_add.insert(witness_account.id);

               signed_transaction tx;
               tx.operations.push_back( pup );
               set_expiration( db, tx );
               sign(tx, init_account_priv_key);

               db.push_transaction(tx, ~0);
               if (db.get_index_type<sport_object_index>().indices().size() == 1)
               {
                  //BOOST_TEST_MESSAGE("The sport creation operation has been approved, new sport object on the blockchain is " << fc::json::to_pretty_string(*db.get_index_type<sport_object_index>().indices().rbegin()));
                break;
               }
            }
         }

      }

   } FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_SUITE_END()

// set up a fixture that places a series of two matched bets, we'll use this fixture to verify
// the result in all three possible outcomes
struct simple_bet_test_fixture : database_fixture {
   betting_market_id_type capitals_win_betting_market_id;
   betting_market_id_type blackhawks_win_betting_market_id;
   betting_market_group_id_type moneyline_betting_markets_id;

   simple_bet_test_fixture()
   {
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET();

      // give alice and bob 10k each
      transfer(account_id_type(), alice_id, asset(10000));
      transfer(account_id_type(), bob_id, asset(10000));

      // place bets at 10:1
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(100, asset_id_type()), 11 * GRAPHENE_BETTING_ODDS_PRECISION);
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000, asset_id_type()), 11 * GRAPHENE_BETTING_ODDS_PRECISION);

      // reverse positions at 1:1
      place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(1100, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(1100, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      capitals_win_betting_market_id = capitals_win_market.id;
      blackhawks_win_betting_market_id = blackhawks_win_market.id;
      moneyline_betting_markets_id = moneyline_betting_markets.id;
   }
};

BOOST_FIXTURE_TEST_SUITE( simple_bet_tests, simple_bet_test_fixture )

BOOST_AUTO_TEST_CASE( win )
{
   try
   {
      resolve_betting_market_group(moneyline_betting_markets_id,
                                  {{capitals_win_betting_market_id, betting_market_resolution_type::win},
                                   {blackhawks_win_betting_market_id, betting_market_resolution_type::cancel}});

      GET_ACTOR(alice);
      GET_ACTOR(bob);

      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage;
      uint32_t rake_value;
      //rake_value = (-100 + 1100 - 1100) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      // alice starts with 10000, pays 100 (bet), wins 1100, then pays 1100 (bet), wins 0
      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000 - 100 + 1100 - 1100 + 0);

      rake_value = (-1000 - 1100 + 2200) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      // bob starts with 10000, pays 1000 (bet), wins 0, then pays 1100 (bet), wins 2200
      BOOST_TEST_MESSAGE("Rake value " +  std::to_string(rake_value));
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000 - 1000 + 0 - 1100 + 2200 - rake_value);
   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( not_win )
{
   try
   {
      resolve_betting_market_group(moneyline_betting_markets_id,
                                  {{capitals_win_betting_market_id, betting_market_resolution_type::not_win},
                                   {blackhawks_win_betting_market_id, betting_market_resolution_type::cancel}});

      GET_ACTOR(alice);
      GET_ACTOR(bob);

      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage;
      uint32_t rake_value = (-100 - 1100 + 2200) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      // alice starts with 10000, pays 100 (bet), wins 0, then pays 1100 (bet), wins 2200
      BOOST_TEST_MESSAGE("Rake value " +  std::to_string(rake_value));
      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000 - 100 + 0 - 1100 + 2200 - rake_value);

      //rake_value = (-1000 + 1100 - 1100) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      // bob starts with 10000, pays 1000 (bet), wins 1100, then pays 1100 (bet), wins 0
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000 - 1000 + 1100 - 1100 + 0);
   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( cancel )
{
   try
   {
      resolve_betting_market_group(moneyline_betting_markets_id,
                                  {{capitals_win_betting_market_id, betting_market_resolution_type::cancel},
                                   {blackhawks_win_betting_market_id, betting_market_resolution_type::cancel}});

      GET_ACTOR(alice);
      GET_ACTOR(bob);

      // alice and bob both start with 10000, they should end with 10000
      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000);
   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

struct simple_bet_test_fixture_2 : database_fixture {
   betting_market_id_type capitals_win_betting_market_id;
   simple_bet_test_fixture_2() 
   {
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET();

      // give alice and bob 10k each
      transfer(account_id_type(), alice_id, asset(10000));
      transfer(account_id_type(), bob_id, asset(10000));

      // alice backs 1000 at 1:1, matches
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      // now alice lays at 2500 at 1:1.  This should require a deposit of 500, with the remaining 200 being funded from exposure
      place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(2500, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      // match the bet bit by bit. bob matches 500 of alice's 2500 bet.  This effectively cancels half of bob's lay position
      // so he immediately gets 500 back.  It reduces alice's back position, but doesn't return any money to her (all 2000 of her exposure
      // was already "promised" to her lay bet, so the 500 she would have received is placed in her refundable_unmatched_bets)
      place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(500, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      // match another 500, which will fully cancel bob's lay position and return the other 500 he had locked up in his position.  
      // alice's back position is now canceled, 1500 remains of her unmatched lay bet, and the 500 from canceling her position has
      // been moved to her refundable_unmatched_bets
      place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(500, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      capitals_win_betting_market_id = capitals_win_market.id;
   }
};

BOOST_FIXTURE_TEST_SUITE( update_tests, database_fixture )

BOOST_AUTO_TEST_CASE(sport_update_test)
{
  try
  {
     ACTORS( (alice) );
     CREATE_ICE_HOCKEY_BETTING_MARKET();
     update_sport(ice_hockey.id, {{"en", "Hockey on Ice"}, {"zh_Hans", "冰"}, {"ja", "アイスホッケ"}});

     transfer(account_id_type(), alice_id, asset(10000000));
     place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

     BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(event_group_update_test)
{
  try
  {
     ACTORS( (alice)(bob) );
     CREATE_ICE_HOCKEY_BETTING_MARKET();

     transfer(account_id_type(), alice_id, asset(10000000));
     transfer(account_id_type(), bob_id, asset(10000000));

     place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

     const sport_object& ice_on_hockey = create_sport({{"en", "Hockey on Ice"}, {"zh_Hans", "冰球"}, {"ja", "アイスホッケー"}}); \
     fc::optional<object_id_type> sport_id = ice_on_hockey.id;

     fc::optional<internationalized_string_type> name =  internationalized_string_type({{"en", "IBM"}, {"zh_Hans", "國家冰球聯"}, {"ja", "ナショナルホッケーリー"}});

     update_event_group(nhl.id, fc::optional<object_id_type>(), name);
     update_event_group(nhl.id, sport_id, fc::optional<internationalized_string_type>());
     update_event_group(nhl.id, sport_id, name);

     place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

     BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
     BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

     // caps win
     resolve_betting_market_group(moneyline_betting_markets.id,
                                 {{capitals_win_market.id, betting_market_resolution_type::win},
                                  {blackhawks_win_market.id, betting_market_resolution_type::cancel}});


     uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage;
     uint32_t rake_value = (-1000000 + 2000000) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
     BOOST_TEST_MESSAGE("Rake value " +  std::to_string(rake_value));
     BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 + 2000000 - rake_value);
     BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

  } FC_LOG_AND_RETHROW()
}


BOOST_AUTO_TEST_CASE(event_update_test)
{
  try
  {
     ACTORS( (alice)(bob) );
     CREATE_ICE_HOCKEY_BETTING_MARKET();

     transfer(account_id_type(), alice_id, asset(10000000));
     transfer(account_id_type(), bob_id, asset(10000000));

     place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

     fc::optional<internationalized_string_type> empty;
     fc::optional<internationalized_string_type> name = internationalized_string_type({{"en", "Washington Capitals vs. Chicago Blackhawks"}, {"zh_Hans", "華盛頓首都隊/芝加哥黑"}, {"ja", "ワシントン・キャピタルズ/シカゴ・ブラックホーク"}});
     fc::optional<internationalized_string_type> season = internationalized_string_type({{"en", "2017-18"}});
     fc::optional<object_id_type> empty_object_id;
     fc::optional<bool> empty_bool;

     update_event(capitals_vs_blackhawks.id, empty_object_id, name, empty, empty_bool);
     update_event(capitals_vs_blackhawks.id, empty_object_id, empty, season, empty_bool);
     update_event(capitals_vs_blackhawks.id, empty_object_id, name, season, empty_bool);

     const sport_object& ice_on_hockey = create_sport({{"en", "Hockey on Ice"}, {"zh_Hans", "冰球"}, {"ja", "アイスホッケー"}}); \
     const event_group_object& nhl2 = create_event_group({{"en", "NHL2"}, {"zh_Hans", "國家冰球聯盟"}, {"ja", "ナショナルホッケーリーグ"}}, ice_on_hockey.id); \
     fc::optional<object_id_type> event_group_id = nhl2.id;

     update_event(capitals_vs_blackhawks.id, event_group_id , empty, empty, empty_bool);

     place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

     BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
     BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

     // caps win
     resolve_betting_market_group(moneyline_betting_markets.id,
                                 {{capitals_win_market.id, betting_market_resolution_type::win},
                                  {blackhawks_win_market.id, betting_market_resolution_type::cancel}});


     uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage;
     uint32_t rake_value = (-1000000 + 2000000) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
     BOOST_TEST_MESSAGE("Rake value " +  std::to_string(rake_value));
     BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 + 2000000 - rake_value);
     BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(betting_market_rules_update_test)
{
  try
  {
     ACTORS( (alice) );
     CREATE_ICE_HOCKEY_BETTING_MARKET();

     fc::optional<internationalized_string_type> empty;
     fc::optional<internationalized_string_type> name = internationalized_string_type({{"en", "NHL Rules v1.1"}});
     fc::optional<internationalized_string_type> desc = internationalized_string_type({{"en", "The winner will be the team with the most points at the end of the game. The team with fewer points will not be the winner."}});

     update_betting_market_rules(betting_market_rules.id, name, empty);
     update_betting_market_rules(betting_market_rules.id, empty, desc);
     update_betting_market_rules(betting_market_rules.id, name, desc);

     transfer(account_id_type(), alice_id, asset(10000000));
     place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

     BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(betting_market_group_update_test)
{
  try
  {
     ACTORS( (alice)(bob) );
     CREATE_ICE_HOCKEY_BETTING_MARKET();

     transfer(account_id_type(), alice_id, asset(10000000));
     place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

     fc::optional<internationalized_string_type> dempty;
     fc::optional<object_id_type> empty_object_id;

     fc::optional<internationalized_string_type> new_desc = internationalized_string_type({{"en", "Money line"}});

     const event_object& odd_vs_even = create_event({{"en", "Capitals vs. Blackhawks"}}, {{"en", "2017-18"}}, nhl.id); \
     fc::optional<object_id_type> new_event = odd_vs_even.id;

     const betting_market_rules_object& new_betting_market_rules = create_betting_market_rules({{"en", "NHL Rules v2.0"}}, {{"en", "The winner will be the team with the most points at the end of the game. The team with fewer points will not be the winner."}});
     fc::optional<object_id_type> new_rule = new_betting_market_rules.id;
     fc::optional<bool> empty_bool;

     update_betting_market_group(moneyline_betting_markets.id, new_desc, empty_object_id, empty_object_id, empty_bool, empty_bool);
     update_betting_market_group(moneyline_betting_markets.id, dempty, new_event, empty_object_id, empty_bool, empty_bool);
     update_betting_market_group(moneyline_betting_markets.id, dempty, empty_object_id, new_rule, empty_bool, empty_bool);
     update_betting_market_group(moneyline_betting_markets.id, new_desc, new_event, new_rule, empty_bool, empty_bool);

     transfer(account_id_type(), bob_id, asset(10000000));
     place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

     BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
     BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

     // caps win
     resolve_betting_market_group(moneyline_betting_markets.id,
                                 {{capitals_win_market.id, betting_market_resolution_type::win},
                                  {blackhawks_win_market.id, betting_market_resolution_type::cancel}});


     uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage;
     uint32_t rake_value = (-1000000 + 2000000) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
     BOOST_TEST_MESSAGE("Rake value " +  std::to_string(rake_value));
     BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 + 2000000 - rake_value);
     BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(betting_market_update_test)
{
  try
  {
     ACTORS( (alice)(bob) );
     CREATE_ICE_HOCKEY_BETTING_MARKET();

     transfer(account_id_type(), alice_id, asset(10000000));
     place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

     const betting_market_group_object& new_moneyline_betting_markets = create_betting_market_group({{"en", "New Moneyline"}}, capitals_vs_blackhawks.id, betting_market_rules.id, asset_id_type()); \
     fc::optional<object_id_type> betting_market_group = new_moneyline_betting_markets.id;

     fc::optional<internationalized_string_type> payout_condition = internationalized_string_type({{"en", "Washington Capitals lose"}});

     update_betting_market(capitals_win_market.id, betting_market_group, fc::optional<internationalized_string_type>());
     update_betting_market(capitals_win_market.id, fc::optional<object_id_type>(), payout_condition);
     update_betting_market(capitals_win_market.id, betting_market_group, payout_condition);

     update_betting_market(blackhawks_win_market.id, betting_market_group, fc::optional<internationalized_string_type>());

     transfer(account_id_type(), bob_id, asset(10000000));
     place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

     BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
     BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

     // caps win
     resolve_betting_market_group(new_moneyline_betting_markets.id,
                                 {{capitals_win_market.id, betting_market_resolution_type::win},
                                  {blackhawks_win_market.id, betting_market_resolution_type::cancel}});


     uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage;
     uint32_t rake_value = (-1000000 + 2000000) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
     BOOST_TEST_MESSAGE("Rake value " +  std::to_string(rake_value));
     BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 + 2000000 - rake_value);
     BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

// testing assertions
BOOST_AUTO_TEST_SUITE(other_betting_tests)

#define TRY_EXPECT_THROW( expr, exc_type, reason )          \
{                                                           \
   try                                                     \
   {                                                       \
       expr;                                               \
       FC_THROW("no error has occured");                   \
   }                                                       \
   catch (exc_type& e)                                     \
   {                                                       \
       if (1)                                              \
       {                                                   \
       elog("###");                                        \
       edump((e.to_detail_string()));                      \
       elog("###");                                        \
       }                                                   \
       FC_ASSERT(e.to_detail_string().find(reason) !=      \
       std::string::npos, "expected error hasn't occured");\
   }                                                       \
   catch (...)                                             \
   {                                                       \
       FC_THROW("expected throw hasn't occured");          \
   }                                                       \
}

BOOST_FIXTURE_TEST_CASE( another_event_group_update_test, database_fixture)
{
  try
  {
     ACTORS( (alice)(bob) );
     CREATE_ICE_HOCKEY_BETTING_MARKET();

     transfer(account_id_type(), alice_id, asset(10000000));
     transfer(account_id_type(), bob_id, asset(10000000));

     place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

     fc::optional<internationalized_string_type> name = internationalized_string_type({{"en", "IBM"}, {"zh_Hans", "國家冰球聯"}, {"ja", "ナショナルホッケーリー"}});

     const sport_object& ice_on_hockey = create_sport({{"en", "Hockey on Ice"}, {"zh_Hans", "冰球"}, {"ja", "アイスホッケー"}}); \
     fc::optional<object_id_type> sport_id = ice_on_hockey.id;

     update_event_group(nhl.id, fc::optional<object_id_type>(), name);
     update_event_group(nhl.id, sport_id, fc::optional<internationalized_string_type>());
     update_event_group(nhl.id, sport_id, name);

     // trx_state->_is_proposed_trx
     //GRAPHENE_REQUIRE_THROW(try_update_event_group(nhl.id, fc::optional<object_id_type>(), fc::optional<internationalized_string_type>(), true), fc::exception);
     TRY_EXPECT_THROW(try_update_event_group(nhl.id, fc::optional<object_id_type>(), fc::optional<internationalized_string_type>(), true), fc::exception, "_is_proposed_trx");

     // #! nothing to change
     //GRAPHENE_REQUIRE_THROW(try_update_event_group(nhl.id, fc::optional<object_id_type>(), fc::optional<internationalized_string_type>()), fc::exception);
     TRY_EXPECT_THROW(try_update_event_group(nhl.id, fc::optional<object_id_type>(), fc::optional<internationalized_string_type>()), fc::exception, "nothing to change");

     // #! sport_id must refer to a sport_id_type
     sport_id = capitals_win_market.id;
     //GRAPHENE_REQUIRE_THROW(try_update_event_group(nhl.id, sport_id, fc::optional<internationalized_string_type>()), fc::exception);
     TRY_EXPECT_THROW(try_update_event_group(nhl.id, sport_id, fc::optional<internationalized_string_type>()), fc::exception, "sport_id must refer to a sport_id_type");

     // #! invalid sport specified
     sport_id = sport_id_type(13);
     //GRAPHENE_REQUIRE_THROW(try_update_event_group(nhl.id, sport_id, fc::optional<internationalized_string_type>()), fc::exception);
     TRY_EXPECT_THROW(try_update_event_group(nhl.id, sport_id, fc::optional<internationalized_string_type>()), fc::exception, "invalid sport specified");

     place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

     BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
     BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

     // caps win
     resolve_betting_market_group(moneyline_betting_markets.id,
                                 {{capitals_win_market.id, betting_market_resolution_type::win},
                                  {blackhawks_win_market.id, betting_market_resolution_type::cancel}});


     uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage;
     uint32_t rake_value = (-1000000 + 2000000) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
     BOOST_TEST_MESSAGE("Rake value " +  std::to_string(rake_value));
     BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 + 2000000 - rake_value);
     BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( tennis_bet_tests, database_fixture )

#define CREATE_TENNIS_BETTING_MARKET() \
  const betting_market_rules_object& tennis_rules = create_betting_market_rules({{"en", "Tennis Rules v1.0"}}, {{"en", "The winner is the player who wins the last ball in the match."}}); \
  const sport_object& tennis = create_sport({{"en", "Tennis"}}); \
  const event_group_object& wimbledon = create_event_group({{"en", "Wimbledon"}}, tennis.id); \
  const event_object& berdych_vs_federer = create_event({{"en", "R. Federer/T. Berdych"}}, {{"en", "2017"}}, wimbledon.id); \
  const event_object& cilic_vs_querrey = create_event({{"en", "M. Cilic/S. Querrye"}}, {{"en", "2017"}}, wimbledon.id); \
  const betting_market_group_object& moneyline_berdych_vs_federer = create_betting_market_group({{"en", "Moneyline 1st sf"}}, berdych_vs_federer.id, tennis_rules.id, asset_id_type()); \
  const betting_market_group_object& moneyline_cilic_vs_querrey = create_betting_market_group({{"en", "Moneyline 2nd sf"}}, cilic_vs_querrey.id, tennis_rules.id, asset_id_type()); \
  const betting_market_object& berdych_wins_market = create_betting_market(moneyline_berdych_vs_federer.id, {{"en", "T. Berdych defeats R. Federer"}}); \
  const betting_market_object& federer_wins_market = create_betting_market(moneyline_berdych_vs_federer.id, {{"en", "R. Federer defeats T. Berdych"}}); \
  const betting_market_object& cilic_wins_market = create_betting_market(moneyline_cilic_vs_querrey.id, {{"en", "M. Cilic defeats S. Querrey"}}); \
  const betting_market_object& querrey_wins_market = create_betting_market(moneyline_cilic_vs_querrey.id, {{"en", "S. Querrey defeats M. Cilic"}});\
  \
  const event_object& cilic_vs_federer = create_event({{"en", "R. Federer/M. Cilic"}}, {{"en", "2017"}}, wimbledon.id); \
  const betting_market_group_object& moneyline_cilic_vs_federer = create_betting_market_group({{"en", "Moneyline final"}}, cilic_vs_federer.id, tennis_rules.id, asset_id_type()); \
  const betting_market_object& federer_wins_final_market = create_betting_market(moneyline_cilic_vs_federer.id, {{"en", "R. Federer defeats M. Cilic"}}); \
  const betting_market_object& cilic_wins_final_market = create_betting_market(moneyline_cilic_vs_federer.id, {{"en", "M. Cilic defeats R. Federer"}});


BOOST_AUTO_TEST_CASE( wimbledon_2017_gentelmen_singles_sf_test )
{
   try
   {
      ACTORS( (alice)(bob) );
      CREATE_TENNIS_BETTING_MARKET();

      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage;

      transfer(account_id_type(), alice_id, asset(10000000));
      transfer(account_id_type(), bob_id, asset(10000000));

      BOOST_TEST_MESSAGE("moneyline_berdych_vs_federer  " << fc::variant(moneyline_berdych_vs_federer.id).as<std::string>());
      BOOST_TEST_MESSAGE("moneyline_cilic_vs_querrey  " << fc::variant(moneyline_cilic_vs_querrey.id).as<std::string>());

      BOOST_TEST_MESSAGE("berdych_wins_market " << fc::variant(berdych_wins_market.id).as<std::string>());
      BOOST_TEST_MESSAGE("federer_wins_market " << fc::variant(federer_wins_market.id).as<std::string>());
      BOOST_TEST_MESSAGE("cilic_wins_market " << fc::variant(cilic_wins_market.id).as<std::string>());
      BOOST_TEST_MESSAGE("querrey_wins_market " << fc::variant(querrey_wins_market.id).as<std::string>());

      place_bet(alice_id, berdych_wins_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      place_bet(bob_id, berdych_wins_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()),   10000000 - 1000000);

      place_bet(alice_id, cilic_wins_market.id, bet_type::back, asset(100000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      place_bet(bob_id, cilic_wins_market.id, bet_type::lay, asset(100000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 - 100000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()),   10000000 - 1000000 - 100000);

      // federer wins
      resolve_betting_market_group(moneyline_berdych_vs_federer.id,
                                  {{berdych_wins_market.id, betting_market_resolution_type::/*don't use cancel - there are bets for berdych*/not_win},
                                   {federer_wins_market.id, betting_market_resolution_type::win}});

      uint32_t bob_rake_value =   (-1000000 + 2000000) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      BOOST_TEST_MESSAGE("Bob's rake value " +  std::to_string(bob_rake_value));

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 - 100000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()),   10000000 - 1000000 - 100000 + 2000000 - bob_rake_value);

      // cilic wins
      resolve_betting_market_group(moneyline_cilic_vs_querrey.id,
                                  {{cilic_wins_market.id, betting_market_resolution_type::win},
                                   {querrey_wins_market.id, betting_market_resolution_type::/*may use cancel - no bets for querrey*/not_win}});

      uint32_t alice_rake_value = (-100000  +  200000) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      BOOST_TEST_MESSAGE("Alice rake value " +  std::to_string(alice_rake_value));

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 - 100000 + 200000  - alice_rake_value);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()),   10000000 - 1000000 - 100000 + 2000000 - bob_rake_value);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( wimbledon_2017_gentelmen_singles_final_test )
{
   try
   {
      ACTORS( (alice)(bob) );
      CREATE_TENNIS_BETTING_MARKET();

      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage;

      transfer(account_id_type(), alice_id, asset(10000000));
      transfer(account_id_type(), bob_id, asset(10000000));

      BOOST_TEST_MESSAGE("moneyline_cilic_vs_federer  " << fc::variant(moneyline_cilic_vs_federer.id).as<std::string>());

      BOOST_TEST_MESSAGE("federer_wins_final_market " << fc::variant(federer_wins_final_market.id).as<std::string>());
      BOOST_TEST_MESSAGE("cilic_wins_final_market " << fc::variant(cilic_wins_final_market.id).as<std::string>());

      betting_market_group_id_type moneyline_cilic_vs_federer_id = moneyline_cilic_vs_federer.id;
      fc::optional<internationalized_string_type> empty_str;
      fc::optional<object_id_type> empty_obj;
      fc::optional<bool> empty_bool;
      fc::optional<bool> true_bool = true;
      update_betting_market_group(moneyline_cilic_vs_federer_id, empty_str,
                                  empty_obj, empty_obj,
                                  empty_bool, true_bool);

      place_bet(alice_id, cilic_wins_final_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      place_bet(bob_id, cilic_wins_final_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      auto cilic_wins_final_market_id = cilic_wins_final_market.id;
      auto federer_wins_final_market_id = federer_wins_final_market.id;

      update_event(cilic_vs_federer.id,
                   fc::optional<object_id_type>(),
                   internationalized_string_type({{"en", "R. Federer vs. M. Cilic"}}),
                   fc::optional<internationalized_string_type>(), fc::optional<bool>());

      generate_blocks(13);

      const betting_market_group_object& betting_market_group = moneyline_cilic_vs_federer_id(db);
      BOOST_CHECK_EQUAL(betting_market_group.total_matched_bets_amount.value, 2000000);

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()),   10000000 - 1000000);

      // federer wins
      resolve_betting_market_group(moneyline_cilic_vs_federer_id,
                                  {{cilic_wins_final_market_id, betting_market_resolution_type::/*don't use cancel - there are bets for berdych*/not_win},
                                   {federer_wins_final_market_id, betting_market_resolution_type::win}});

      uint32_t bob_rake_value =   (-1000000 + 2000000) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      BOOST_TEST_MESSAGE("Bob's rake value " +  std::to_string(bob_rake_value));

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()),   10000000 - 1000000 + 2000000 - bob_rake_value);

    } FC_LOG_AND_RETHROW()
 }

BOOST_AUTO_TEST_SUITE_END()



//#define BOOST_TEST_MODULE "C++ Unit Tests for Graphene Blockchain Database"
#include <cstdlib>
#include <iostream>
#include <boost/test/included/unit_test.hpp>

boost::unit_test::test_suite* init_unit_test_suite(int argc, char* argv[]) {
    std::srand(time(NULL));
    std::cout << "Random number generator seeded to " << time(NULL) << std::endl;
    return nullptr;
}

