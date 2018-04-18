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
#include "database_fixture.hpp"

#include <graphene/chain/sport_object.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/event_group_object.hpp>
#include <graphene/chain/betting_market_object.hpp>

using namespace graphene::chain;

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
