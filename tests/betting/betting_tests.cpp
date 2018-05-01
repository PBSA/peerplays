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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
// disable auto_ptr deprecated warning, see https://svn.boost.org/trac10/ticket/11622
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#pragma GCC diagnostic pop

#include "../common/database_fixture.hpp"

#include <boost/test/unit_test.hpp>
#include <fc/crypto/openssl.hpp>
#include <fc/log/appender.hpp>
#include <openssl/rand.h>

#include <graphene/utilities/tempdir.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/sport_object.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/event_group_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/betting_market_object.hpp>

#include <graphene/bookie/bookie_api.hpp>
//#include <boost/algorithm/string/replace.hpp>

struct enable_betting_logging_config {
   enable_betting_logging_config()
   { 
      fc::logger::get("betting").add_appender(fc::appender::get("stdout"));
      fc::logger::get("betting").set_log_level(fc::log_level::debug);
   }
   ~enable_betting_logging_config()  { 
      fc::logger::get("betting").remove_appender(fc::appender::get("stdout"));
   }
};
BOOST_GLOBAL_FIXTURE( enable_betting_logging_config );

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace graphene::chain::keywords;

#define CREATE_ICE_HOCKEY_BETTING_MARKET(never_in_play, delay_before_settling) \
  create_sport({{"en", "Ice Hockey"}, {"zh_Hans", "冰球"}, {"ja", "アイスホッケー"}}); \
  generate_blocks(1); \
  const sport_object& ice_hockey = *db.get_index_type<sport_object_index>().indices().get<by_id>().rbegin(); \
  create_event_group({{"en", "NHL"}, {"zh_Hans", "國家冰球聯盟"}, {"ja", "ナショナルホッケーリーグ"}}, ice_hockey.id); \
  generate_blocks(1); \
  const event_group_object& nhl = *db.get_index_type<event_group_object_index>().indices().get<by_id>().rbegin(); \
  create_event({{"en", "Washington Capitals/Chicago Blackhawks"}, {"zh_Hans", "華盛頓首都隊/芝加哥黑鷹"}, {"ja", "ワシントン・キャピタルズ/シカゴ・ブラックホークス"}}, {{"en", "2016-17"}}, nhl.id); \
  generate_blocks(1); \
  const event_object& capitals_vs_blackhawks = *db.get_index_type<event_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market_rules({{"en", "NHL Rules v1.0"}}, {{"en", "The winner will be the team with the most points at the end of the game.  The team with fewer points will not be the winner."}}); \
  generate_blocks(1); \
  const betting_market_rules_object& betting_market_rules = *db.get_index_type<betting_market_rules_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market_group({{"en", "Moneyline"}}, capitals_vs_blackhawks.id, betting_market_rules.id, asset_id_type(), never_in_play, delay_before_settling); \
  generate_blocks(1); \
  const betting_market_group_object& moneyline_betting_markets = *db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(moneyline_betting_markets.id, {{"en", "Washington Capitals win"}}); \
  generate_blocks(1); \
  const betting_market_object& capitals_win_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(moneyline_betting_markets.id, {{"en", "Chicago Blackhawks win"}}); \
  generate_blocks(1); \
  const betting_market_object& blackhawks_win_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  (void)capitals_win_market; (void)blackhawks_win_market;

// create the basic betting market, plus groups for the first, second, and third period results
#define CREATE_EXTENDED_ICE_HOCKEY_BETTING_MARKET(never_in_play, delay_before_settling) \
  CREATE_ICE_HOCKEY_BETTING_MARKET(never_in_play, delay_before_settling) \
  create_betting_market_group({{"en", "First Period Result"}}, capitals_vs_blackhawks.id, betting_market_rules.id, asset_id_type(), never_in_play, delay_before_settling); \
  generate_blocks(1); \
  const betting_market_group_object& first_period_result_betting_markets = *db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(first_period_result_betting_markets.id, {{"en", "Washington Capitals win"}}); \
  generate_blocks(1); \
  const betting_market_object& first_period_capitals_win_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(first_period_result_betting_markets.id, {{"en", "Chicago Blackhawks win"}}); \
  generate_blocks(1); \
  const betting_market_object& first_period_blackhawks_win_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  (void)first_period_capitals_win_market; (void)first_period_blackhawks_win_market; \
  \
  create_betting_market_group({{"en", "Second Period Result"}}, capitals_vs_blackhawks.id, betting_market_rules.id, asset_id_type(), never_in_play, delay_before_settling); \
  generate_blocks(1); \
  const betting_market_group_object& second_period_result_betting_markets = *db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(second_period_result_betting_markets.id, {{"en", "Washington Capitals win"}}); \
  generate_blocks(1); \
  const betting_market_object& second_period_capitals_win_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(second_period_result_betting_markets.id, {{"en", "Chicago Blackhawks win"}}); \
  generate_blocks(1); \
  const betting_market_object& second_period_blackhawks_win_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  (void)second_period_capitals_win_market; (void)second_period_blackhawks_win_market; \
  \
  create_betting_market_group({{"en", "Third Period Result"}}, capitals_vs_blackhawks.id, betting_market_rules.id, asset_id_type(), never_in_play, delay_before_settling); \
  generate_blocks(1); \
  const betting_market_group_object& third_period_result_betting_markets = *db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(third_period_result_betting_markets.id, {{"en", "Washington Capitals win"}}); \
  generate_blocks(1); \
  const betting_market_object& third_period_capitals_win_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(third_period_result_betting_markets.id, {{"en", "Chicago Blackhawks win"}}); \
  generate_blocks(1); \
  const betting_market_object& third_period_blackhawks_win_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  (void)third_period_capitals_win_market; (void)third_period_blackhawks_win_market;

#define CREATE_TENNIS_BETTING_MARKET() \
  create_betting_market_rules({{"en", "Tennis Rules v1.0"}}, {{"en", "The winner is the player who wins the last ball in the match."}}); \
  generate_blocks(1); \
  const betting_market_rules_object& tennis_rules = *db.get_index_type<betting_market_rules_object_index>().indices().get<by_id>().rbegin(); \
  create_sport({{"en", "Tennis"}}); \
  generate_blocks(1); \
  const sport_object& tennis = *db.get_index_type<sport_object_index>().indices().get<by_id>().rbegin(); \
  create_event_group({{"en", "Wimbledon"}}, tennis.id); \
  generate_blocks(1); \
  const event_group_object& wimbledon = *db.get_index_type<event_group_object_index>().indices().get<by_id>().rbegin(); \
  create_event({{"en", "R. Federer/T. Berdych"}}, {{"en", "2017"}}, wimbledon.id); \
  generate_blocks(1); \
  const event_object& berdych_vs_federer = *db.get_index_type<event_object_index>().indices().get<by_id>().rbegin(); \
  create_event({{"en", "M. Cilic/S. Querrye"}}, {{"en", "2017"}}, wimbledon.id); \
  generate_blocks(1); \
  const event_object& cilic_vs_querrey = *db.get_index_type<event_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market_group({{"en", "Moneyline 1st sf"}}, berdych_vs_federer.id, tennis_rules.id, asset_id_type(), false, 0); \
  generate_blocks(1); \
  const betting_market_group_object& moneyline_berdych_vs_federer = *db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market_group({{"en", "Moneyline 2nd sf"}}, cilic_vs_querrey.id, tennis_rules.id, asset_id_type(), false, 0); \
  generate_blocks(1); \
  const betting_market_group_object& moneyline_cilic_vs_querrey = *db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(moneyline_berdych_vs_federer.id, {{"en", "T. Berdych defeats R. Federer"}}); \
  generate_blocks(1); \
  const betting_market_object& berdych_wins_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(moneyline_berdych_vs_federer.id, {{"en", "R. Federer defeats T. Berdych"}}); \
  generate_blocks(1); \
  const betting_market_object& federer_wins_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(moneyline_cilic_vs_querrey.id, {{"en", "M. Cilic defeats S. Querrey"}}); \
  generate_blocks(1); \
  const betting_market_object& cilic_wins_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(moneyline_cilic_vs_querrey.id, {{"en", "S. Querrey defeats M. Cilic"}});\
  generate_blocks(1); \
  const betting_market_object& querrey_wins_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  create_event({{"en", "R. Federer/M. Cilic"}}, {{"en", "2017"}}, wimbledon.id); \
  generate_blocks(1); \
  const event_object& cilic_vs_federer = *db.get_index_type<event_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market_group({{"en", "Moneyline final"}}, cilic_vs_federer.id, tennis_rules.id, asset_id_type(), false, 0); \
  generate_blocks(1); \
  const betting_market_group_object& moneyline_cilic_vs_federer = *db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(moneyline_cilic_vs_federer.id, {{"en", "R. Federer defeats M. Cilic"}}); \
  generate_blocks(1); \
  const betting_market_object& federer_wins_final_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  create_betting_market(moneyline_cilic_vs_federer.id, {{"en", "M. Cilic defeats R. Federer"}}); \
  generate_blocks(1); \
  const betting_market_object& cilic_wins_final_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin(); \
  (void)federer_wins_market;(void)cilic_wins_market;(void)federer_wins_final_market; (void)cilic_wins_final_market; (void)berdych_wins_market; (void)querrey_wins_market;

BOOST_FIXTURE_TEST_SUITE( betting_tests, database_fixture )

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
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

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
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

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
      BOOST_CHECK_EQUAL(binned_orders_point_one.aggregated_back_bets.size(), 2u);
      BOOST_CHECK_EQUAL(binned_orders_point_one.aggregated_lay_bets.size(), 0u);
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
      BOOST_CHECK_EQUAL(binned_orders_point_one.aggregated_back_bets.size(), 0u);
      BOOST_CHECK_EQUAL(binned_orders_point_one.aggregated_lay_bets.size(), 2u);
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
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      // give alice and bob 10M each
      transfer(account_id_type(), alice_id, asset(10000000));
      transfer(account_id_type(), bob_id, asset(10000000));

      // have bob lay a bet for 1M (+20k fees) at 1:1 odds                  
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      // have alice back a matching bet at 1:1 odds (also costing 1.02M) 
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::closed);

      // caps win
      resolve_betting_market_group(moneyline_betting_markets.id,
                                  {{capitals_win_market.id, betting_market_resolution_type::win},
                                   {blackhawks_win_market.id, betting_market_resolution_type::not_win}});

      generate_blocks(1);

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
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

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
      generate_blocks(1);
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

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

BOOST_AUTO_TEST_CASE(bet_reversal_test)
{
   // test whether we can bet our entire balance in one direction, then reverse our bet (while having zero balance)
   try
   {
      generate_blocks(1);
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      transfer(account_id_type(), alice_id, asset(10000000));
      share_type alice_expected_balance = 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      // back with our entire balance
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(10000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), 0);

      // reverse the bet
      place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(20000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), 0);

      // try to re-reverse it, but go too far
      BOOST_CHECK_THROW( place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(30000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION), fc::exception);
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), 0);
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(bet_against_exposure_test)
{
   // test whether we can bet our entire balance in one direction, have it match, then reverse our bet (while having zero balance)
   try
   {
      generate_blocks(1);
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      transfer(account_id_type(), alice_id, asset(10000000));
      transfer(account_id_type(), bob_id, asset(10000000));
      int64_t alice_expected_balance = 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance);
      int64_t bob_expected_balance = 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance);

      // back with alice's entire balance
      place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(10000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      alice_expected_balance -= 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance);

      // lay with bob's entire balance, which fully matches bob's bet
      place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(10000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      bob_expected_balance -= 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance);

      // reverse the bet
      place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(20000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance);

      // try to re-reverse it, but go too far
      BOOST_CHECK_THROW( place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(30000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION), fc::exception);
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance);
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(persistent_objects_test)
{
   try
   {
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      graphene::bookie::bookie_api bookie_api(app);

      transfer(account_id_type(), alice_id, asset(10000000));
      transfer(account_id_type(), bob_id, asset(10000000));
      share_type alice_expected_balance = 10000000;
      share_type bob_expected_balance = 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      idump((capitals_win_market.get_status())); 

      // lay 46 at 1.94 odds (50:47) -- this is too small to be placed on the books and there's
      // nothing for it to match, so it should be canceled
      bet_id_type automatically_canceled_bet_id = place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(46, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      generate_blocks(1);
      BOOST_CHECK_MESSAGE(!db.find(automatically_canceled_bet_id), "Bet should have been canceled, but the blockchain still knows about it");
      fc::variants objects_from_bookie = bookie_api.get_objects({automatically_canceled_bet_id});
      idump((objects_from_bookie));
      BOOST_REQUIRE_EQUAL(objects_from_bookie.size(), 1u);
      BOOST_CHECK_MESSAGE(objects_from_bookie[0]["id"].as<bet_id_type>() == automatically_canceled_bet_id, "Bookie Plugin didn't return a deleted bet it");

      // lay 47 at 1.94 odds (50:47) -- this bet should go on the order books normally
      bet_id_type first_bet_on_books = place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(47, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      generate_blocks(1);
      BOOST_CHECK_MESSAGE(db.find(first_bet_on_books), "Bet should exist on the blockchain");
      objects_from_bookie = bookie_api.get_objects({first_bet_on_books});
      idump((objects_from_bookie));
      BOOST_REQUIRE_EQUAL(objects_from_bookie.size(), 1u);
      BOOST_CHECK_MESSAGE(objects_from_bookie[0]["id"].as<bet_id_type>() == first_bet_on_books, "Bookie Plugin didn't return a bet that is currently on the books");

      // place a bet that exactly matches 'first_bet_on_books', should result in empty books (thus, no bet_objects from the blockchain)
      bet_id_type matching_bet = place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(50, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      BOOST_CHECK_MESSAGE(!db.find(first_bet_on_books), "Bet should have been filled, but the blockchain still knows about it");
      BOOST_CHECK_MESSAGE(!db.find(matching_bet), "Bet should have been filled, but the blockchain still knows about it");
      generate_blocks(1); // the bookie plugin doesn't detect matches until a block is generated

      objects_from_bookie = bookie_api.get_objects({first_bet_on_books, matching_bet});
      idump((objects_from_bookie));
      BOOST_REQUIRE_EQUAL(objects_from_bookie.size(), 2u);
      BOOST_CHECK_MESSAGE(objects_from_bookie[0]["id"].as<bet_id_type>() == first_bet_on_books, "Bookie Plugin didn't return a bet that has been filled");
      BOOST_CHECK_MESSAGE(objects_from_bookie[1]["id"].as<bet_id_type>() == matching_bet, "Bookie Plugin didn't return a bet that has been filled");

      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::closed);

      resolve_betting_market_group(moneyline_betting_markets.id,
            {{capitals_win_market.id, betting_market_resolution_type::cancel},
            {blackhawks_win_market.id, betting_market_resolution_type::cancel}});

      // as soon as the market is resolved during the generate_block(), these markets
      // should be deleted and our references will go out of scope.  Save the
      // market ids here so we can verify that they were really deleted
      betting_market_id_type capitals_win_market_id = capitals_win_market.id;
      betting_market_id_type blackhawks_win_market_id = blackhawks_win_market.id;

      generate_blocks(1);

      // test get_matched_bets_for_bettor
      std::vector<graphene::bookie::matched_bet_object> alice_matched_bets = bookie_api.get_matched_bets_for_bettor(alice_id);
      for (const graphene::bookie::matched_bet_object& matched_bet : alice_matched_bets)
      {
         idump((matched_bet));
         for (operation_history_id_type id : matched_bet.associated_operations)
            idump((id(db)));
      }
      BOOST_REQUIRE_EQUAL(alice_matched_bets.size(), 1u);
      BOOST_CHECK(alice_matched_bets[0].amount_matched == 47);
      std::vector<graphene::bookie::matched_bet_object> bob_matched_bets = bookie_api.get_matched_bets_for_bettor(bob_id);
      for (const graphene::bookie::matched_bet_object& matched_bet : bob_matched_bets)
      {
         idump((matched_bet));
         for (operation_history_id_type id : matched_bet.associated_operations)
            idump((id(db)));
      }
      BOOST_REQUIRE_EQUAL(bob_matched_bets.size(), 1u);
      BOOST_CHECK(bob_matched_bets[0].amount_matched == 50);

      // test getting markets
      //  test that we cannot get them from the database directly
      BOOST_CHECK_THROW(capitals_win_market_id(db), fc::exception);
      BOOST_CHECK_THROW(blackhawks_win_market_id(db), fc::exception);

      objects_from_bookie = bookie_api.get_objects({capitals_win_market_id, blackhawks_win_market_id});
      BOOST_REQUIRE_EQUAL(objects_from_bookie.size(), 2u);
      idump((objects_from_bookie));
      BOOST_CHECK(!objects_from_bookie[0].is_null());
      BOOST_CHECK(!objects_from_bookie[1].is_null());
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(delayed_bets_test) // test live betting
{
   try
   {
      const auto& bet_odds_idx = db.get_index_type<bet_object_index>().indices().get<by_odds>();
 
      ACTORS( (alice)(bob) );
 
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);
 
      generate_blocks(1);
 
      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::in_play);
      generate_blocks(1);
 
      transfer(account_id_type(), alice_id, asset(10000000));
      transfer(account_id_type(), bob_id, asset(10000000));
      share_type alice_expected_balance = 10000000;
      share_type bob_expected_balance = 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);
 
      generate_blocks(1);
 
      BOOST_TEST_MESSAGE("Testing basic delayed bet mechanics");
      // alice backs 100 at odds 2
      BOOST_TEST_MESSAGE("Alice places a back bet of 100 at odds 2.0");
      bet_id_type delayed_back_bet = place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(100, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      generate_blocks(1);
 
      // verify the bet hasn't been placed in the active book
      auto first_bet_in_market = bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id));
      BOOST_CHECK(first_bet_in_market == bet_odds_idx.end());
 
      // after 3 blocks, the delay should have expired and it will be promoted to the active book
      generate_blocks(2);
      first_bet_in_market = bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id));
      auto last_bet_in_market = bet_odds_idx.upper_bound(std::make_tuple(capitals_win_market.id));
      BOOST_CHECK(first_bet_in_market != bet_odds_idx.end());
      BOOST_CHECK(std::distance(first_bet_in_market, last_bet_in_market) == 1);
 
      for (const auto& bet : boost::make_iterator_range(first_bet_in_market, last_bet_in_market))
        edump((bet));
      // bob lays 100 at odds 2 to match alice's bet currently on the books
      BOOST_TEST_MESSAGE("Bob places a lay bet of 100 at odds 2.0");
      /* bet_id_type delayed_lay_bet = */ place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
 
      edump((db.get_global_properties().parameters.block_interval)(db.head_block_time()));
      // the bet should not enter the order books before a block has been generated
      first_bet_in_market = bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id));
      last_bet_in_market = bet_odds_idx.upper_bound(std::make_tuple(capitals_win_market.id));
      for (const auto& bet : bet_odds_idx)
        edump((bet));
      generate_blocks(1);
 
      // bob's bet will still be delayed, so the active order book will only contain alice's bet
      first_bet_in_market = bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id));
      last_bet_in_market = bet_odds_idx.upper_bound(std::make_tuple(capitals_win_market.id));
      edump((std::distance(first_bet_in_market, last_bet_in_market)));
      BOOST_CHECK(std::distance(first_bet_in_market, last_bet_in_market) == 1);
      for (const auto& bet : boost::make_iterator_range(first_bet_in_market, last_bet_in_market))
        edump((bet));
 
      // once bob's bet's delay has expired, the two bets will annihilate each other, leaving
      // an empty order book
      generate_blocks(2);
      BOOST_CHECK(bet_odds_idx.empty());
 
      // now test that when we cancel all bets on a market, delayed bets get canceled too
      BOOST_TEST_MESSAGE("Alice places a back bet of 100 at odds 2.0");
      delayed_back_bet = place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(100, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      generate_blocks(1);
      first_bet_in_market = bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id));
      BOOST_CHECK(!bet_odds_idx.empty());
      BOOST_CHECK(first_bet_in_market == bet_odds_idx.end());
      BOOST_TEST_MESSAGE("Cancel all bets");
      cancel_unmatched_bets(moneyline_betting_markets.id);
      BOOST_CHECK(bet_odds_idx.empty());
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

         BOOST_REQUIRE_EQUAL(db.get_index_type<proposal_index>().indices().size(), 1u);
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

BOOST_AUTO_TEST_CASE( testnet_witness_block_production_error )
{
   try
   {
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);
      create_betting_market_group({{"en", "Unused"}}, capitals_vs_blackhawks.id, betting_market_rules.id, asset_id_type(), false, 0);
      generate_blocks(1);
      const betting_market_group_object& unused_betting_markets = *db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin();

      BOOST_TEST_MESSAGE("setting the event in progress");
      update_event(capitals_vs_blackhawks.id, _status = event_status::in_progress);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::in_progress);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::in_play);
      BOOST_CHECK(unused_betting_markets.get_status() == betting_market_group_status::in_play);

      BOOST_TEST_MESSAGE("setting the event to finished");
      update_event(capitals_vs_blackhawks.id, _status = event_status::finished);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::finished);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(unused_betting_markets.get_status() == betting_market_group_status::closed);

      BOOST_TEST_MESSAGE("setting the event to canceled");
      update_event(capitals_vs_blackhawks.id, _status = event_status::canceled);
      generate_blocks(1);
   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( cancel_one_event_in_group )
{
   // test that creates an event group with two events in it.  We walk one event through the 
   // usual sequence and cancel it, verify that it doesn't alter the other event in the group
   try
   {
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      // create a second event in the same betting market group
      create_event({{"en", "Boston Bruins/Pittsburgh Penguins"}}, {{"en", "2016-17"}}, nhl.id);
      generate_blocks(1);
      const event_object& bruins_vs_penguins = *db.get_index_type<event_object_index>().indices().get<by_id>().rbegin();
      create_betting_market_group({{"en", "Moneyline"}}, bruins_vs_penguins.id, betting_market_rules.id, asset_id_type(), false, 0);
      generate_blocks(1);
      const betting_market_group_object& bruins_penguins_moneyline_betting_markets = *db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().rbegin();
      create_betting_market(bruins_penguins_moneyline_betting_markets.id, {{"en", "Boston Bruins win"}});
      generate_blocks(1);
      const betting_market_object& bruins_win_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin();
      create_betting_market(bruins_penguins_moneyline_betting_markets.id, {{"en", "Pittsburgh Penguins win"}});
      generate_blocks(1);
      const betting_market_object& penguins_win_market = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().rbegin();
      (void)bruins_win_market; (void)penguins_win_market;

      // check the initial state
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(bruins_vs_penguins.get_status() == event_status::upcoming);

      BOOST_TEST_MESSAGE("setting the capitals_vs_blackhawks event to in-progress, leaving bruins_vs_penguins in upcoming");
      update_event(capitals_vs_blackhawks.id, _status = event_status::in_progress);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::in_progress);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::in_play);

      BOOST_CHECK(bruins_vs_penguins.get_status() == event_status::upcoming);
      BOOST_CHECK(bruins_penguins_moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(bruins_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(penguins_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the capitals_vs_blackhawks event to finished");
      update_event(capitals_vs_blackhawks.id, _status = event_status::finished);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::finished);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(bruins_vs_penguins.get_status() == event_status::upcoming);
      BOOST_CHECK(bruins_penguins_moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(bruins_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(penguins_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the capitals_vs_blackhawks event to canceled");
      update_event(capitals_vs_blackhawks.id, _status = event_status::canceled);
      generate_blocks(1);
      BOOST_CHECK(bruins_vs_penguins.get_status() == event_status::upcoming);
      BOOST_CHECK(bruins_penguins_moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(bruins_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(penguins_win_market.get_status() == betting_market_status::unresolved);

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
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

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

      // close betting to prepare for the next operation which will be grading or cancel
      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::closed);
      generate_blocks(1);
   }
};

BOOST_FIXTURE_TEST_SUITE( simple_bet_tests, simple_bet_test_fixture )

BOOST_AUTO_TEST_CASE( win )
{
   try
   {
      resolve_betting_market_group(moneyline_betting_markets_id,
                                  {{capitals_win_betting_market_id, betting_market_resolution_type::win},
                                   {blackhawks_win_betting_market_id, betting_market_resolution_type::not_win}});
      generate_blocks(1);

      GET_ACTOR(alice);
      GET_ACTOR(bob);

      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage;
      uint32_t rake_value;
      //rake_value = (-100 + 1100 - 1100) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      // alice starts with 10000, pays 100 (bet), wins 1100, then pays 1100 (bet), wins 0
      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000 - 100 + 1100 - 1100 + 0);

      rake_value = (-1000 - 1100 + 2200) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      edump((rake_fee_percentage)(rake_value));
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
                                    {blackhawks_win_betting_market_id, betting_market_resolution_type::win}});
      generate_blocks(1);

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
      generate_blocks(1);

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
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

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
     CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);
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
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);
 
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
 
      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::closed);

      // caps win
      resolve_betting_market_group(moneyline_betting_markets.id,
                                  {{capitals_win_market.id, betting_market_resolution_type::win},
                                   {blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);
 
 
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
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      transfer(account_id_type(), alice_id, asset(10000000));
      transfer(account_id_type(), bob_id, asset(10000000));

      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      fc::optional<internationalized_string_type> name = internationalized_string_type({{"en", "Washington Capitals vs. Chicago Blackhawks"}, {"zh_Hans", "華盛頓首都隊/芝加哥黑"}, {"ja", "ワシントン・キャピタルズ/シカゴ・ブラックホーク"}});
      fc::optional<internationalized_string_type> season = internationalized_string_type({{"en", "2017-18"}});

      update_event(capitals_vs_blackhawks.id, _name = name);
      update_event(capitals_vs_blackhawks.id, _season = season);
      update_event(capitals_vs_blackhawks.id, _name = name, _season = season);

      const sport_object& ice_on_hockey = create_sport({{"en", "Hockey on Ice"}, {"zh_Hans", "冰球"}, {"ja", "アイスホッケー"}});
      const event_group_object& nhl2 = create_event_group({{"en", "NHL2"}, {"zh_Hans", "國家冰球聯盟"}, {"ja", "ナショナルホッケーリーグ"}}, ice_on_hockey.id);

      update_event(capitals_vs_blackhawks.id, _event_group_id = nhl2.id);

      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::closed);

      // caps win
      resolve_betting_market_group(moneyline_betting_markets.id,
                                  {{capitals_win_market.id, betting_market_resolution_type::win},
                                   {blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);


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
     CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

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
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      transfer(account_id_type(), alice_id, asset(10000000));
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      internationalized_string_type new_description = internationalized_string_type({{"en", "Money line"}});

      const betting_market_rules_object& new_betting_market_rules = create_betting_market_rules({{"en", "NHL Rules v2.0"}}, {{"en", "The winner will be the team with the most points at the end of the game. The team with fewer points will not be the winner."}});
      fc::optional<object_id_type> new_rule = new_betting_market_rules.id;

      update_betting_market_group(moneyline_betting_markets.id, _description = new_description);
      update_betting_market_group(moneyline_betting_markets.id, _rules_id =  new_betting_market_rules.id);
      update_betting_market_group(moneyline_betting_markets.id, _description = new_description, _rules_id = new_betting_market_rules.id);

      transfer(account_id_type(), bob_id, asset(10000000));
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::closed);
      // caps win
      resolve_betting_market_group(moneyline_betting_markets.id,
                                   {{capitals_win_market.id, betting_market_resolution_type::win},
                                    {blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);


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
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      transfer(account_id_type(), alice_id, asset(10000000));
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      fc::optional<internationalized_string_type> payout_condition = internationalized_string_type({{"en", "Washington Capitals lose"}});

      // update the payout condition
      update_betting_market(capitals_win_market.id, fc::optional<object_id_type>(), payout_condition);

      transfer(account_id_type(), bob_id, asset(10000000));
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::closed);
      // caps win
      resolve_betting_market_group(moneyline_betting_markets.id,
                                   {{capitals_win_market.id, betting_market_resolution_type::win},
                                    {blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);

      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage;
      uint32_t rake_value = (-1000000 + 2000000) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      BOOST_TEST_MESSAGE("Rake value " +  std::to_string(rake_value));
      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 + 2000000 - rake_value);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( event_status_tests, database_fixture )

// This tests a normal progression by setting the event state and
// letting it trickle down:
// - upcoming
// - finished
// - settled
BOOST_AUTO_TEST_CASE(event_driven_standard_progression_1)
{
   try
   {
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);
      graphene::bookie::bookie_api bookie_api(app);
      // save the event id for checking after it is deleted
      event_id_type capitals_vs_blackhawks_id = capitals_vs_blackhawks.id;

      BOOST_TEST_MESSAGE("verify everything is in the correct initial state");
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the event to finished");
      update_event(capitals_vs_blackhawks.id, _status = event_status::finished);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::finished);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("grading betting market");
      resolve_betting_market_group(moneyline_betting_markets.id,
                                   {{capitals_win_market.id, betting_market_resolution_type::win},
                                    {blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);

      // as soon as a block is generated, the betting market group will settle, and the market
      // and group will cease to exist.  The event should transition to "settled", then
      // removed.
      fc::variants objects_from_bookie = bookie_api.get_objects({capitals_vs_blackhawks_id});

      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(), "settled");
   } FC_LOG_AND_RETHROW()
}

// This tests a normal progression by setting the event state and
// letting it trickle down.  Like the above, with delayed settling:
// - upcoming
// - finished
// - settled
BOOST_AUTO_TEST_CASE(event_driven_standard_progression_1_with_delay)
{
   try
   {
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 60 /* seconds */);
      graphene::bookie::bookie_api bookie_api(app);
      // save the event id for checking after it is deleted
      event_id_type capitals_vs_blackhawks_id = capitals_vs_blackhawks.id;

      BOOST_TEST_MESSAGE("verify everything is in the correct initial state");
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the event to finished");
      update_event(capitals_vs_blackhawks.id, _status = event_status::finished);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::finished);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("grading betting market");
      resolve_betting_market_group(moneyline_betting_markets.id,
                                   {{capitals_win_market.id, betting_market_resolution_type::win},
                                    {blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);
      // it should be waiting 60 seconds before it settles
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::finished);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::graded);

      generate_blocks(60);
      // as soon as a block is generated, the betting market group will settle, and the market
      // and group will cease to exist.  The event should transition to "settled", then
      // removed.
      fc::variants objects_from_bookie = bookie_api.get_objects({capitals_vs_blackhawks_id});

      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(), "settled");
   } FC_LOG_AND_RETHROW()
}

// This tests a normal progression by setting the event state and
// letting it trickle down:
// - upcoming
// - frozen
// - upcoming
// - in_progress
// - frozen
// - in_progress
// - finished
// - settled
BOOST_AUTO_TEST_CASE(event_driven_standard_progression_2)
{
   try
   {
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      graphene::bookie::bookie_api bookie_api(app);
      // save the event id for checking after it is deleted
      event_id_type capitals_vs_blackhawks_id = capitals_vs_blackhawks.id;

      BOOST_TEST_MESSAGE("verify everything is in the correct initial state");
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the event frozen");
      update_event(capitals_vs_blackhawks.id, _status = event_status::frozen);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::frozen);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::frozen);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::frozen);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::frozen);

      BOOST_TEST_MESSAGE("setting the event back to upcoming");
      update_event(capitals_vs_blackhawks.id, _status = event_status::upcoming);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the event in-progress");
      update_event(capitals_vs_blackhawks.id, _status = event_status::in_progress);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::in_progress);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::in_play);

      BOOST_TEST_MESSAGE("setting the event frozen");
      update_event(capitals_vs_blackhawks.id, _status = event_status::frozen);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::frozen);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::frozen);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::frozen);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::frozen);

      BOOST_TEST_MESSAGE("setting the event back in-progress");
      update_event(capitals_vs_blackhawks.id, _status = event_status::in_progress);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::in_progress);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::in_play);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the event to finished");
      update_event(capitals_vs_blackhawks.id, _status = event_status::finished);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::finished);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("grading betting market");
      resolve_betting_market_group(moneyline_betting_markets.id,
                                   {{capitals_win_market.id, betting_market_resolution_type::win},
                                    {blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);

      // as soon as a block is generated, the betting market group will settle, and the market
      // and group will cease to exist.  The event should transition to "settled", then
      // removed.
      fc::variants objects_from_bookie = bookie_api.get_objects({capitals_vs_blackhawks_id});

      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(), "settled");
   } FC_LOG_AND_RETHROW()
}

// This tests a normal progression by setting the event state and
// letting it trickle down.  This is the same as the above test, but the
// never-in-play flag is set:
// - upcoming
// - frozen
// - upcoming
// - in_progress
// - frozen
// - in_progress
// - finished
// - settled
BOOST_AUTO_TEST_CASE(event_driven_standard_progression_2_never_in_play)
{
   try
   {
      CREATE_ICE_HOCKEY_BETTING_MARKET(true, 0);

      graphene::bookie::bookie_api bookie_api(app);
      // save the event id for checking after it is deleted
      event_id_type capitals_vs_blackhawks_id = capitals_vs_blackhawks.id;

      BOOST_TEST_MESSAGE("verify everything is in the correct initial state");
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the event frozen");
      update_event(capitals_vs_blackhawks.id, _status = event_status::frozen);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::frozen);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::frozen);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::frozen);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::frozen);

      BOOST_TEST_MESSAGE("setting the event back to upcoming");
      update_event(capitals_vs_blackhawks.id, _status = event_status::upcoming);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the event in-progress");
      update_event(capitals_vs_blackhawks.id, _status = event_status::in_progress);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::in_progress);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);

      BOOST_TEST_MESSAGE("setting the event frozen");
      update_event(capitals_vs_blackhawks.id, _status = event_status::frozen);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::frozen);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::frozen);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::frozen);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::frozen);

      BOOST_TEST_MESSAGE("setting the event back in-progress");
      update_event(capitals_vs_blackhawks.id, _status = event_status::in_progress);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::in_progress);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the event to finished");
      update_event(capitals_vs_blackhawks.id, _status = event_status::finished);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::finished);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("grading betting market");
      resolve_betting_market_group(moneyline_betting_markets.id,
                                   {{capitals_win_market.id, betting_market_resolution_type::win},
                                    {blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);

      // as soon as a block is generated, the betting market group will settle, and the market
      // and group will cease to exist.  The event should transition to "settled", then
      // removed.
      fc::variants objects_from_bookie = bookie_api.get_objects({capitals_vs_blackhawks_id});

      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(), "settled");
   } FC_LOG_AND_RETHROW()
}

// This tests a slightly different normal progression by setting the event state and
// letting it trickle down:
// - upcoming
// - frozen
// - in_progress
// - frozen
// - in_progress
// - finished
// - canceled
BOOST_AUTO_TEST_CASE(event_driven_standard_progression_3)
{
   try
   {
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      graphene::bookie::bookie_api bookie_api(app);
      // save the event id for checking after it is deleted
      event_id_type capitals_vs_blackhawks_id = capitals_vs_blackhawks.id;

      BOOST_TEST_MESSAGE("verify everything is in the correct initial state");
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the event frozen");
      update_event(capitals_vs_blackhawks.id, _status = event_status::frozen);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::frozen);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::frozen);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::frozen);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::frozen);

      BOOST_TEST_MESSAGE("setting the event in progress");
      update_event(capitals_vs_blackhawks.id, _status = event_status::in_progress);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::in_progress);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::in_play);

      BOOST_TEST_MESSAGE("setting the event frozen");
      update_event(capitals_vs_blackhawks.id, _status = event_status::frozen);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::frozen);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::frozen);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::frozen);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::frozen);

      BOOST_TEST_MESSAGE("setting the event back in-progress");
      update_event(capitals_vs_blackhawks.id, _status = event_status::in_progress);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::in_progress);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::in_play);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the event to finished");
      update_event(capitals_vs_blackhawks.id, _status = event_status::finished);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::finished);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the event to canceled");
      update_event(capitals_vs_blackhawks.id, _status = event_status::canceled);
      generate_blocks(1);

      // as soon as a block is generated, the betting market group will cancel, and the market
      // and group will cease to exist.  The event should transition to "canceled", then be removed
      fc::variants objects_from_bookie = bookie_api.get_objects({capitals_vs_blackhawks_id});

      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(), "canceled");
 
   } FC_LOG_AND_RETHROW()
}

// This tests that we reject illegal transitions
// - anything -> settled
// - in_progress -> upcoming
// - frozen after in_progress -> upcoming
// - finished -> upcoming, in_progress, frozen
// - canceled -> anything
BOOST_AUTO_TEST_CASE(event_driven_progression_errors_1)
{
   try
   {
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      graphene::bookie::bookie_api bookie_api(app);
      // save the event id for checking after it is deleted
      event_id_type capitals_vs_blackhawks_id = capitals_vs_blackhawks.id;

      BOOST_TEST_MESSAGE("verify everything is in the correct initial state");
      BOOST_REQUIRE(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_REQUIRE(moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_REQUIRE(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_REQUIRE(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      // settled is the only illegal transition from upcoming
      BOOST_TEST_MESSAGE("verifying we can't jump to settled");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks.id, _status = event_status::settled, _force = true), fc::exception);

      BOOST_TEST_MESSAGE("setting the event frozen");
      update_event(capitals_vs_blackhawks.id, _status = event_status::frozen);
      generate_blocks(1);
      BOOST_REQUIRE(capitals_vs_blackhawks.get_status() == event_status::frozen);
      BOOST_REQUIRE(moneyline_betting_markets.get_status() == betting_market_group_status::frozen);
      BOOST_REQUIRE(capitals_win_market.get_status() == betting_market_status::frozen);
      BOOST_REQUIRE(blackhawks_win_market.get_status() == betting_market_status::frozen);

      // settled is the only illegal transition from this frozen event
      BOOST_TEST_MESSAGE("verifying we can't jump to settled");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks.id, _status = event_status::settled, _force = true), fc::exception);

      BOOST_TEST_MESSAGE("setting the event in progress");
      update_event(capitals_vs_blackhawks.id, _status = event_status::in_progress);
      generate_blocks(1);
      BOOST_REQUIRE(capitals_vs_blackhawks.get_status() == event_status::in_progress);
      BOOST_REQUIRE(moneyline_betting_markets.get_status() == betting_market_group_status::in_play);

      // we can't go back to upcoming from in_progress.
      // settled is disallowed everywhere
      BOOST_TEST_MESSAGE("verifying we can't jump to upcoming");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks.id, _status = event_status::upcoming, _force = true), fc::exception);
      BOOST_TEST_MESSAGE("verifying we can't jump to settled");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks.id, _status = event_status::settled, _force = true), fc::exception);

      BOOST_TEST_MESSAGE("setting the event frozen");
      update_event(capitals_vs_blackhawks.id, _status = event_status::frozen);
      generate_blocks(1);
      BOOST_REQUIRE(capitals_vs_blackhawks.get_status() == event_status::frozen);
      BOOST_REQUIRE(moneyline_betting_markets.get_status() == betting_market_group_status::frozen);
      BOOST_REQUIRE(capitals_win_market.get_status() == betting_market_status::frozen);
      BOOST_REQUIRE(blackhawks_win_market.get_status() == betting_market_status::frozen);

      // we can't go back to upcoming from frozen once we've gone in_progress.
      // settled is disallowed everywhere
      BOOST_TEST_MESSAGE("verifying we can't jump to upcoming");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks.id, _status = event_status::upcoming, _force = true), fc::exception);
      BOOST_TEST_MESSAGE("verifying we can't jump to settled");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks.id, _status = event_status::settled, _force = true), fc::exception);

      BOOST_TEST_MESSAGE("setting the event to finished");
      update_event(capitals_vs_blackhawks.id, _status = event_status::finished);
      generate_blocks(1);
      BOOST_REQUIRE(capitals_vs_blackhawks.get_status() == event_status::finished);
      BOOST_REQUIRE(moneyline_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_REQUIRE(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_REQUIRE(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      // we can't go back to upcoming, in_progress, or frozen once we're finished.
      // settled is disallowed everywhere
      BOOST_TEST_MESSAGE("verifying we can't jump to upcoming");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks.id, _status = event_status::upcoming, _force = true), fc::exception);
      BOOST_TEST_MESSAGE("verifying we can't jump to in_progress");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks.id, _status = event_status::in_progress, _force = true), fc::exception);
      BOOST_TEST_MESSAGE("verifying we can't jump to frozen");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks.id, _status = event_status::frozen, _force = true), fc::exception);
      BOOST_TEST_MESSAGE("verifying we can't jump to settled");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks.id, _status = event_status::settled, _force = true), fc::exception);

      BOOST_TEST_MESSAGE("setting the event to canceled");
      update_event(capitals_vs_blackhawks.id, _status = event_status::canceled);
      generate_blocks(1);

      fc::variants objects_from_bookie = bookie_api.get_objects({capitals_vs_blackhawks_id});
      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(), "canceled");

      // we can't go back to upcoming, in_progress, frozen, or finished once we're canceled.
      // (this won't work because the event has been deleted)
      BOOST_TEST_MESSAGE("verifying we can't jump to upcoming");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks_id, _status = event_status::upcoming, _force = true), fc::exception);
      BOOST_TEST_MESSAGE("verifying we can't jump to in_progress");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks_id, _status = event_status::in_progress, _force = true), fc::exception);
      BOOST_TEST_MESSAGE("verifying we can't jump to frozen");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks_id, _status = event_status::frozen, _force = true), fc::exception);
      BOOST_TEST_MESSAGE("verifying we can't jump to finished");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks_id, _status = event_status::finished, _force = true), fc::exception);
      BOOST_TEST_MESSAGE("verifying we can't jump to settled");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks_id, _status = event_status::settled, _force = true), fc::exception);
   } FC_LOG_AND_RETHROW()
}

// This tests that we can't transition out of settled (all other transitions tested in the previous test)
// - settled -> anything
BOOST_AUTO_TEST_CASE(event_driven_progression_errors_2)
{
   try
   {
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      graphene::bookie::bookie_api bookie_api(app);
      // save the event id for checking after it is deleted
      event_id_type capitals_vs_blackhawks_id = capitals_vs_blackhawks.id;

      BOOST_TEST_MESSAGE("verify everything is in the correct initial state");
      BOOST_REQUIRE(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_REQUIRE(moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_REQUIRE(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_REQUIRE(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting the event to finished");
      update_event(capitals_vs_blackhawks.id, _status = event_status::finished);
      generate_blocks(1);
      BOOST_REQUIRE(capitals_vs_blackhawks.get_status() == event_status::finished);
      BOOST_REQUIRE(moneyline_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_REQUIRE(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_REQUIRE(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("grading betting market");
      resolve_betting_market_group(moneyline_betting_markets.id,
                                   {{capitals_win_market.id, betting_market_resolution_type::win},
                                    {blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);

      // as soon as a block is generated, the betting market group will settle, and the market
      // and group will cease to exist.  The event should transition to "settled", then removed
      fc::variants objects_from_bookie = bookie_api.get_objects({capitals_vs_blackhawks_id});
      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(), "settled");

      // we can't go back to upcoming, in_progress, frozen, or finished once we're canceled.
      // (this won't work because the event has been deleted)
      BOOST_TEST_MESSAGE("verifying we can't jump to upcoming");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks_id, _status = event_status::upcoming, _force = true), fc::exception);
      BOOST_TEST_MESSAGE("verifying we can't jump to in_progress");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks_id, _status = event_status::in_progress, _force = true), fc::exception);
      BOOST_TEST_MESSAGE("verifying we can't jump to frozen");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks_id, _status = event_status::frozen, _force = true), fc::exception);
      BOOST_TEST_MESSAGE("verifying we can't jump to finished");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks_id, _status = event_status::finished, _force = true), fc::exception);
      BOOST_TEST_MESSAGE("verifying we can't jump to canceled");
      BOOST_CHECK_THROW(update_event(capitals_vs_blackhawks_id, _status = event_status::canceled, _force = true), fc::exception);
   } FC_LOG_AND_RETHROW()
}

// This tests a normal progression by setting the betting_market_group state and
// letting it trickle up to the event:
BOOST_AUTO_TEST_CASE(betting_market_group_driven_standard_progression)
{
   try
   {
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      graphene::bookie::bookie_api bookie_api(app);
      // save the event id for checking after it is deleted
      event_id_type capitals_vs_blackhawks_id = capitals_vs_blackhawks.id;

      BOOST_TEST_MESSAGE("verify everything is in the correct initial state");
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("setting betting market to frozen");
      // the event should stay in the upcoming state
      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::frozen);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::frozen);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::frozen);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::frozen);

      BOOST_TEST_MESSAGE("setting the event frozen");
      // this should only change the status of the event, just verify that nothing weird happens when 
      // we try to set the bmg to frozen when it's already frozen
      update_event(capitals_vs_blackhawks.id, _status = event_status::frozen);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::frozen);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::frozen);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::frozen);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::frozen);

      BOOST_TEST_MESSAGE("setting betting market to closed");
      // the event should go to finished automatically
      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::closed);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::finished);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("grading betting market");
      resolve_betting_market_group(moneyline_betting_markets.id,
                                   {{capitals_win_market.id, betting_market_resolution_type::win},
                                    {blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);

      // as soon as a block is generated, the betting market group will settle, and the market
      // and group will cease to exist.  The event should transition to "settled"
      fc::variants objects_from_bookie = bookie_api.get_objects({capitals_vs_blackhawks_id});
      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(), "settled");
   } FC_LOG_AND_RETHROW()
}

// This tests a normal progression in an event with multiple betting market groups
// letting info it trickle up from the group to the event:
BOOST_AUTO_TEST_CASE(multi_betting_market_group_driven_standard_progression)
{
   try
   {
      CREATE_EXTENDED_ICE_HOCKEY_BETTING_MARKET(false, 0);

      graphene::bookie::bookie_api bookie_api(app);
      // save the event id for checking after it is deleted
      event_id_type capitals_vs_blackhawks_id = capitals_vs_blackhawks.id;

      BOOST_TEST_MESSAGE("verify everything is in the correct initial state");
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(first_period_result_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(first_period_capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(first_period_blackhawks_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(second_period_result_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(second_period_capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(second_period_blackhawks_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(third_period_result_betting_markets.get_status() == betting_market_group_status::upcoming);
      BOOST_CHECK(third_period_capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(third_period_blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("the game is starting, setting the main betting market and the first period to in_play");
      // the event should stay in the upcoming state
      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::in_play);
      update_betting_market_group(first_period_result_betting_markets.id, _status = betting_market_group_status::in_play);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::in_play);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(first_period_result_betting_markets.get_status() == betting_market_group_status::in_play);
      BOOST_CHECK(first_period_capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(first_period_blackhawks_win_market.get_status() == betting_market_status::unresolved);


      BOOST_TEST_MESSAGE("the first period is over, starting the second period");
      update_betting_market_group(first_period_result_betting_markets.id, _status = betting_market_group_status::closed);
      update_betting_market_group(second_period_result_betting_markets.id, _status = betting_market_group_status::in_play);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::in_play);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(first_period_result_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_CHECK(first_period_capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(first_period_blackhawks_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(second_period_result_betting_markets.get_status() == betting_market_group_status::in_play);
      BOOST_CHECK(second_period_capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(second_period_blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("grading the first period market");
      resolve_betting_market_group(first_period_result_betting_markets.id,
                                   {{first_period_capitals_win_market.id, betting_market_resolution_type::win},
                                    {first_period_blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);

      BOOST_TEST_MESSAGE("the second period is over, starting the third period");
      update_betting_market_group(second_period_result_betting_markets.id, _status = betting_market_group_status::closed);
      update_betting_market_group(third_period_result_betting_markets.id, _status = betting_market_group_status::in_play);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::upcoming);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::in_play);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(second_period_result_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_CHECK(second_period_capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(second_period_blackhawks_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(third_period_result_betting_markets.get_status() == betting_market_group_status::in_play);
      BOOST_CHECK(third_period_capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(third_period_blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("grading the second period market");
      resolve_betting_market_group(second_period_result_betting_markets.id,
                                   {{second_period_capitals_win_market.id, betting_market_resolution_type::win},
                                    {second_period_blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);

      BOOST_TEST_MESSAGE("the game is over, closing 3rd period and game");
      update_betting_market_group(third_period_result_betting_markets.id, _status = betting_market_group_status::closed);
      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::closed);
      generate_blocks(1);
      BOOST_CHECK(capitals_vs_blackhawks.get_status() == event_status::finished);
      BOOST_CHECK(moneyline_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(third_period_result_betting_markets.get_status() == betting_market_group_status::closed);
      BOOST_CHECK(third_period_capitals_win_market.get_status() == betting_market_status::unresolved);
      BOOST_CHECK(third_period_blackhawks_win_market.get_status() == betting_market_status::unresolved);

      BOOST_TEST_MESSAGE("grading the third period and game");
      resolve_betting_market_group(third_period_result_betting_markets.id,
                                   {{third_period_capitals_win_market.id, betting_market_resolution_type::win},
                                    {third_period_blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      resolve_betting_market_group(moneyline_betting_markets.id,
                                   {{capitals_win_market.id, betting_market_resolution_type::win},
                                    {blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);

      // as soon as a block is generated, the two betting market groups will settle, and the market
      // and group will cease to exist.  The event should transition to "settled"
      fc::variants objects_from_bookie = bookie_api.get_objects({capitals_vs_blackhawks_id});
      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(), "settled");
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
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);
 
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
 
      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::closed);
      // caps win
      resolve_betting_market_group(moneyline_betting_markets.id,
                                  {{capitals_win_market.id, betting_market_resolution_type::win},
                                   {blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);
 
      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage;
      uint32_t rake_value = (-1000000 + 2000000) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      BOOST_TEST_MESSAGE("Rake value " +  std::to_string(rake_value));
      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 + 2000000 - rake_value);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);
   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE( tennis_bet_tests, database_fixture )

BOOST_AUTO_TEST_CASE( wimbledon_2017_gentelmen_singles_sf_test )
{
   try
   {
      ACTORS( (alice)(bob) );
      CREATE_TENNIS_BETTING_MARKET();
      generate_blocks(1);

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

      update_betting_market_group(moneyline_berdych_vs_federer.id, _status = betting_market_group_status::closed);
      // federer wins
      resolve_betting_market_group(moneyline_berdych_vs_federer.id,
                                  {{berdych_wins_market.id, betting_market_resolution_type::not_win},
                                   {federer_wins_market.id, betting_market_resolution_type::win}});
      generate_blocks(1);

      uint32_t bob_rake_value =   (-1000000 + 2000000) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      BOOST_TEST_MESSAGE("Bob's rake value " +  std::to_string(bob_rake_value));

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 - 100000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()),   10000000 - 1000000 - 100000 + 2000000 - bob_rake_value);

      update_betting_market_group(moneyline_cilic_vs_querrey.id, _status = betting_market_group_status::closed);
      // cilic wins
      resolve_betting_market_group(moneyline_cilic_vs_querrey.id,
                                  {{cilic_wins_market.id, betting_market_resolution_type::win},
                                   {querrey_wins_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);

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
      update_betting_market_group(moneyline_cilic_vs_federer_id, _status = betting_market_group_status::in_play);

      place_bet(alice_id, cilic_wins_final_market.id, bet_type::back, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
      place_bet(bob_id, cilic_wins_final_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      auto cilic_wins_final_market_id = cilic_wins_final_market.id;
      auto federer_wins_final_market_id = federer_wins_final_market.id;

      update_event(cilic_vs_federer.id, _name = internationalized_string_type({{"en", "R. Federer vs. M. Cilic"}}));

      generate_blocks(13);

      const betting_market_group_object& betting_market_group = moneyline_cilic_vs_federer_id(db);
      BOOST_CHECK_EQUAL(betting_market_group.total_matched_bets_amount.value, 2000000);

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()),   10000000 - 1000000);

      update_betting_market_group(moneyline_cilic_vs_federer_id, _status = betting_market_group_status::closed);
      // federer wins
      resolve_betting_market_group(moneyline_cilic_vs_federer_id,
                                  {{cilic_wins_final_market_id, betting_market_resolution_type::/*don't use cancel - there are bets for berdych*/not_win},
                                   {federer_wins_final_market_id, betting_market_resolution_type::win}});
      generate_blocks(1);

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

    // betting operations don't take effect until HARDFORK 1000
    GRAPHENE_TESTING_GENESIS_TIMESTAMP = HARDFORK_1000_TIME.sec_since_epoch();

    return nullptr;
}

