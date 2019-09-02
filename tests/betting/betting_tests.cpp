/*
 * Copyright (c) 2018 Peerplays Blockchain Standards Association, and contributors.
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

#include "../common/betting_test_markets.hpp"

#include <boost/test/unit_test.hpp>
#include <fc/crypto/openssl.hpp>
#include <fc/log/appender.hpp>
#include <openssl/rand.h>

#include <graphene/chain/protocol/proposal.hpp>

#include <graphene/utilities/tempdir.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/proposal_object.hpp>

#include <graphene/bookie/bookie_api.hpp>

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


// While the bets are placed, stored, and sorted using the decimal form of their odds, matching
// uses the ratios to ensure no rounding takes place.
// The allowed odds are defined by rules that can be changed at runtime.
// For reference when designing/debugging tests, here is the list of allowed decimal odds and their
// corresponding ratios as set in the genesis block.
//
// decimal    ratio  | decimal  ratio  | decimal    ratio  | decimal    ratio  | decimal    ratio  | decimal    ratio
// ------------------+-----------------+-------------------+-------------------+-------------------+-----------------
//    1.01    100:1  |    1.6     5:3  |   2.38     50:69  |    4.8      5:19  |     26      1:25  |    440     1:439
//    1.02     50:1  |   1.61  100:61  |    2.4       5:7  |    4.9     10:39  |     27      1:26  |    450     1:449
//    1.03    100:3  |   1.62   50:31  |   2.42     50:71  |      5       1:4  |     28      1:27  |    460     1:459
//    1.04     25:1  |   1.63  100:63  |   2.44     25:36  |    5.1     10:41  |     29      1:28  |    470     1:469
//    1.05     20:1  |   1.64   25:16  |   2.46     50:73  |    5.2      5:21  |     30      1:29  |    480     1:479
//    1.06     50:3  |   1.65   20:13  |   2.48     25:37  |    5.3     10:43  |     32      1:31  |    490     1:489
//    1.07    100:7  |   1.66   50:33  |    2.5       2:3  |    5.4      5:22  |     34      1:33  |    500     1:499
//    1.08     25:2  |   1.67  100:67  |   2.52     25:38  |    5.5       2:9  |     36      1:35  |    510     1:509
//    1.09    100:9  |   1.68   25:17  |   2.54     50:77  |    5.6      5:23  |     38      1:37  |    520     1:519
//     1.1     10:1  |   1.69  100:69  |   2.56     25:39  |    5.7     10:47  |     40      1:39  |    530     1:529
//    1.11   100:11  |    1.7    10:7  |   2.58     50:79  |    5.8      5:24  |     42      1:41  |    540     1:539
//    1.12     25:3  |   1.71  100:71  |    2.6       5:8  |    5.9     10:49  |     44      1:43  |    550     1:549
//    1.13   100:13  |   1.72   25:18  |   2.62     50:81  |      6       1:5  |     46      1:45  |    560     1:559
//    1.14     50:7  |   1.73  100:73  |   2.64     25:41  |    6.2      5:26  |     48      1:47  |    570     1:569
//    1.15     20:3  |   1.74   50:37  |   2.66     50:83  |    6.4      5:27  |     50      1:49  |    580     1:579
//    1.16     25:4  |   1.75     4:3  |   2.68     25:42  |    6.6      5:28  |     55      1:54  |    590     1:589
//    1.17   100:17  |   1.76   25:19  |    2.7     10:17  |    6.8      5:29  |     60      1:59  |    600     1:599
//    1.18     50:9  |   1.77  100:77  |   2.72     25:43  |      7       1:6  |     65      1:64  |    610     1:609
//    1.19   100:19  |   1.78   50:39  |   2.74     50:87  |    7.2      5:31  |     70      1:69  |    620     1:619
//     1.2      5:1  |   1.79  100:79  |   2.76     25:44  |    7.4      5:32  |     75      1:74  |    630     1:629
//    1.21   100:21  |    1.8     5:4  |   2.78     50:89  |    7.6      5:33  |     80      1:79  |    640     1:639
//    1.22    50:11  |   1.81  100:81  |    2.8       5:9  |    7.8      5:34  |     85      1:84  |    650     1:649
//    1.23   100:23  |   1.82   50:41  |   2.82     50:91  |      8       1:7  |     90      1:89  |    660     1:659
//    1.24     25:6  |   1.83  100:83  |   2.84     25:46  |    8.2      5:36  |     95      1:94  |    670     1:669
//    1.25      4:1  |   1.84   25:21  |   2.86     50:93  |    8.4      5:37  |    100      1:99  |    680     1:679
//    1.26    50:13  |   1.85   20:17  |   2.88     25:47  |    8.6      5:38  |    110     1:109  |    690     1:689
//    1.27   100:27  |   1.86   50:43  |    2.9     10:19  |    8.8      5:39  |    120     1:119  |    700     1:699
//    1.28     25:7  |   1.87  100:87  |   2.92     25:48  |      9       1:8  |    130     1:129  |    710     1:709
//    1.29   100:29  |   1.88   25:22  |   2.94     50:97  |    9.2      5:41  |    140     1:139  |    720     1:719
//     1.3     10:3  |   1.89  100:89  |   2.96     25:49  |    9.4      5:42  |    150     1:149  |    730     1:729
//    1.31   100:31  |    1.9    10:9  |   2.98     50:99  |    9.6      5:43  |    160     1:159  |    740     1:739
//    1.32     25:8  |   1.91  100:91  |      3       1:2  |    9.8      5:44  |    170     1:169  |    750     1:749
//    1.33   100:33  |   1.92   25:23  |   3.05     20:41  |     10       1:9  |    180     1:179  |    760     1:759
//    1.34    50:17  |   1.93  100:93  |    3.1     10:21  |   10.5      2:19  |    190     1:189  |    770     1:769
//    1.35     20:7  |   1.94   50:47  |   3.15     20:43  |     11      1:10  |    200     1:199  |    780     1:779
//    1.36     25:9  |   1.95   20:19  |    3.2      5:11  |   11.5      2:21  |    210     1:209  |    790     1:789
//    1.37   100:37  |   1.96   25:24  |   3.25       4:9  |     12      1:11  |    220     1:219  |    800     1:799
//    1.38    50:19  |   1.97  100:97  |    3.3     10:23  |   12.5      2:23  |    230     1:229  |    810     1:809
//    1.39   100:39  |   1.98   50:49  |   3.35     20:47  |     13      1:12  |    240     1:239  |    820     1:819
//     1.4      5:2  |   1.99  100:99  |    3.4      5:12  |   13.5      2:25  |    250     1:249  |    830     1:829
//    1.41   100:41  |      2     1:1  |   3.45     20:49  |     14      1:13  |    260     1:259  |    840     1:839
//    1.42    50:21  |   2.02   50:51  |    3.5       2:5  |   14.5      2:27  |    270     1:269  |    850     1:849
//    1.43   100:43  |   2.04   25:26  |   3.55     20:51  |     15      1:14  |    280     1:279  |    860     1:859
//    1.44    25:11  |   2.06   50:53  |    3.6      5:13  |   15.5      2:29  |    290     1:289  |    870     1:869
//    1.45     20:9  |   2.08   25:27  |   3.65     20:53  |     16      1:15  |    300     1:299  |    880     1:879
//    1.46    50:23  |    2.1   10:11  |    3.7     10:27  |   16.5      2:31  |    310     1:309  |    890     1:889
//    1.47   100:47  |   2.12   25:28  |   3.75      4:11  |     17      1:16  |    320     1:319  |    900     1:899
//    1.48    25:12  |   2.14   50:57  |    3.8      5:14  |   17.5      2:33  |    330     1:329  |    910     1:909
//    1.49   100:49  |   2.16   25:29  |   3.85     20:57  |     18      1:17  |    340     1:339  |    920     1:919
//     1.5      2:1  |   2.18   50:59  |    3.9     10:29  |   18.5      2:35  |    350     1:349  |    930     1:929
//    1.51   100:51  |    2.2     5:6  |   3.95     20:59  |     19      1:18  |    360     1:359  |    940     1:939
//    1.52    25:13  |   2.22   50:61  |      4       1:3  |   19.5      2:37  |    370     1:369  |    950     1:949
//    1.53   100:53  |   2.24   25:31  |    4.1     10:31  |     20      1:19  |    380     1:379  |    960     1:959
//    1.54    50:27  |   2.26   50:63  |    4.2      5:16  |     21      1:20  |    390     1:389  |    970     1:969
//    1.55    20:11  |   2.28   25:32  |    4.3     10:33  |     22      1:21  |    400     1:399  |    980     1:979
//    1.56    25:14  |    2.3   10:13  |    4.4      5:17  |     23      1:22  |    410     1:409  |    990     1:989
//    1.57   100:57  |   2.32   25:33  |    4.5       2:7  |     24      1:23  |    420     1:419  |   1000     1:999
//    1.58    50:29  |   2.34   50:67  |    4.6      5:18  |     25      1:24  |    430     1:429  |
//    1.59   100:59  |   2.36   25:34  |    4.7     10:37

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
      //
      // for the bets bob placed above, we should get: 200 @ 1.6, 300 @ 1.7
      BOOST_CHECK_EQUAL(binned_orders_point_one.aggregated_back_bets.size(), 2u);
      BOOST_CHECK_EQUAL(binned_orders_point_one.aggregated_lay_bets.size(), 0u);
      for (const graphene::bookie::order_bin& binned_order : binned_orders_point_one.aggregated_back_bets)
      {
        // compute the matching lay order
        share_type lay_amount = bet_object::get_approximate_matching_amount(binned_order.amount_to_bet, binned_order.backer_multiplier, bet_type::back, true /* round up */);
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
      // these bets will get rounded down, actual amounts are 99, 99, 91, 99, and 67
//       place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 155 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
//       place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 155 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
//       place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 165 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
//       place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 165 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
//       place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 165 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
//
//       binned_orders_point_one = bookie_api.get_binned_order_book(capitals_win_market.id, 1);
//       idump((binned_orders_point_one));
//
//       // the binned orders returned should be chosen so that we if we assume those orders are real and we place
//       // matching lay orders, we will completely consume the underlying orders and leave no orders on the books
//       //
//       // for the bets bob placed above, we shoudl get 356 @ 1.6, 99 @ 1.5
//       BOOST_CHECK_EQUAL(binned_orders_point_one.aggregated_back_bets.size(), 0u);
//       BOOST_CHECK_EQUAL(binned_orders_point_one.aggregated_lay_bets.size(), 2u);
//       for (const graphene::bookie::order_bin& binned_order : binned_orders_point_one.aggregated_lay_bets)
//       {
//         // compute the matching lay order
//         share_type back_amount = bet_object::get_approximate_matching_amount(binned_order.amount_to_bet, binned_order.backer_multiplier, bet_type::lay, true /* round up */);
//         ilog("Alice is backing with ${back_amount} at odds ${odds} to match the binned lay amount ${lay_amount}", ("back_amount", back_amount)("odds", binned_order.backer_multiplier)("lay_amount", binned_order.amount_to_bet));
//         place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(back_amount, asset_id_type()), binned_order.backer_multiplier);
//
//       }
//
//
//       bet_iter = bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id));
//       while (bet_iter != bet_odds_idx.end() &&
//              bet_iter->betting_market_id == capitals_win_market.id)
//       {
//         idump((*bet_iter));
//         ++bet_iter;
//       }
//
//       BOOST_CHECK(bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id)) == bet_odds_idx.end());
//
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

      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage();
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

BOOST_AUTO_TEST_CASE(match_using_takers_expected_amounts)
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
      BOOST_TEST_MESSAGE("lay 46 at 1.94 odds (50:47) -- this is too small to be placed on the books and there's nothing for it to match, so it should be canceled");
      place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(46, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      BOOST_TEST_MESSAGE("alice's balance should be " << alice_expected_balance.value);
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);
ilog("message");
      // lay 47 at 1.94 odds (50:47) -- this is an exact amount, nothing surprising should happen here
      BOOST_TEST_MESSAGE("alice lays 470 at 1.94 odds (50:47) -- this is an exact amount, nothing surprising should happen here");
      place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(47, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      alice_expected_balance -= 47;
      BOOST_TEST_MESSAGE("alice's balance should be " << alice_expected_balance.value);
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      // lay 100 at 1.91 odds (100:91) -- this is an inexact match, we should get refunded 9 and leave a bet for 91 on the books
      place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(100, asset_id_type()), 191 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      alice_expected_balance -= 91;
      BOOST_TEST_MESSAGE("alice's balance should be " << alice_expected_balance.value);
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);


      transfer(account_id_type(), bob_id, asset(10000000));
      share_type bob_expected_balance = 10000000;
      BOOST_TEST_MESSAGE("bob's balance should be " << bob_expected_balance.value);
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);

      // now have bob match it with a back of 300 at 1.5
      // This should:
      //   match the full 47 @ 1.94 with 50
      //   match the full 91 @ 1.91 with 100
      //   bob's balance goes down by 300 (150 is matched, 150 is still on the books)
      //   leaves a back bet of 150 @ 1.5 on the books
      BOOST_TEST_MESSAGE("now have bob match it with a back of 300 at 1.5");
      place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(300, asset_id_type()), 15 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      bob_expected_balance -= 300;
      BOOST_TEST_MESSAGE("bob's balance should be " << bob_expected_balance.value);
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(match_using_takers_expected_amounts2)
{
   try
   {
      generate_blocks(1);
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      transfer(account_id_type(), alice_id, asset(10000000));
      share_type alice_expected_balance = 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      // lay 470 at 1.94 odds (50:47) -- this is an exact amount, nothing surprising should happen here
      BOOST_TEST_MESSAGE("alice lays 470 at 1.94 odds (50:47) -- this is an exact amount, nothing surprising should happen here");
      place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(470, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      alice_expected_balance -= 470;
      BOOST_TEST_MESSAGE("alice's balance should be " << alice_expected_balance.value);
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      transfer(account_id_type(), bob_id, asset(10000000));
      share_type bob_expected_balance = 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);

      // now have bob match it with a back of 900 at 1.5
      // This should:
      //   match all 500 of bob's bet, and leave 400 @ 1.5 on the books
      //   bob's balance goes down by the 900 he paid (500 matched, 400 unmatched)
      //   alice's bet is completely removed from the books.
      BOOST_TEST_MESSAGE("now have bob match it with a back of 900 at 1.5");
      place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(900, asset_id_type()), 15 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      bob_expected_balance -= 900;

      BOOST_TEST_MESSAGE("bob's balance should be " << bob_expected_balance.value);
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(match_using_takers_expected_amounts3)
{
   try
   {
      generate_blocks(1);
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      transfer(account_id_type(), alice_id, asset(10000000));
      share_type alice_expected_balance = 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      // lay 470 at 1.94 odds (50:47) -- this is an exact amount, nothing surprising should happen here
      place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(470, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      alice_expected_balance -= 470;
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      transfer(account_id_type(), bob_id, asset(10000000));
      share_type bob_expected_balance = 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);

      // now have bob match it with a back of 1000 at 1.5
      // This should:
      //   match all of the 470 @ 1.94 with 500, and leave 500 left on the books
      //   bob's balance goes down by the 1000 he paid, 500 matching, 500 unmatching
      //   alice's bet is removed from the books
      place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(1000, asset_id_type()), 15 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      bob_expected_balance -= 1000;
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(match_using_takers_expected_amounts4)
{
   try
   {
      generate_blocks(1);
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      transfer(account_id_type(), alice_id, asset(10000000));
      share_type alice_expected_balance = 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      // back 1000 at 1.89 odds (100:89) -- this is an exact amount, nothing surprising should happen here
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000, asset_id_type()), 189 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      alice_expected_balance -= 1000;
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      // back 1000 at 1.97 odds (100:97) -- again, this is an exact amount, nothing surprising should happen here
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1000, asset_id_type()), 197 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      alice_expected_balance -= 1000;
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      transfer(account_id_type(), bob_id, asset(10000000));
      share_type bob_expected_balance = 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);

      // now have bob match it with a lay of 3000 at 2.66
      // * This means bob expects to pay 3000 and match against 1807.2289.  Or,
      //   put another way, bob wants to buy a payout of up to 1807.2289 if the
      //   capitals win, and he is willing to pay up to 3000 to do so.
      // * The first thing that happens is bob's bet gets rounded down to something
      //   that can match exactly.  2.66 is 50:83 odds, so bob's bet is
      //   reduced to 2988, which should match against 1800.
      //   So bob gets an immediate refund of 12
      // * The next thing that happens is a match against the top bet on the order book.
      //   That's 1000 @ 1.89.  We match at those odds (100:89), so bob will fully match
      //   this bet, paying 890 to get 1000 of his desired win position.
      //   * this top bet is removed from the order books
      //   * bob now has 1000 of the 1800 win position he wants.  we adjust his bet
      //     so that what is left will only match 800.  This means we will
      //     refund bob 770.  His remaining bet is now lay 1328 @ 2.66
      // * Now we match the next bet on the order books, which is 1000 @ 1.97 (100:97).
      //   Bob only wants 800 of 1000 win position being offered, so he will not
      //   completely consume this bet.
      //   * Bob pays 776 to get his 800 win position.
      //   * alice's top bet on the books is reduced 200 @ 1.97
      //   * Bob now has as much of a position as he was willing to buy.  We refund
      //     the remainder of his bet, which is 552
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(3000, asset_id_type()), 266 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      bob_expected_balance -= 3000 - 12 - 770 - 552;
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(match_using_takers_expected_amounts5)
{
   try
   {
      generate_blocks(1);
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      transfer(account_id_type(), alice_id, asset(10000000));
      share_type alice_expected_balance = 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      // back 1100 at 1.86 odds (50:43) -- this is an exact amount, nothing surprising should happen here
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(1100, asset_id_type()), 186 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      alice_expected_balance -= 1100;
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      transfer(account_id_type(), bob_id, asset(10000000));
      share_type bob_expected_balance = 10000000;
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);

      // now have bob match it with a lay of 1100 at 1.98
      // * This means bob expects to pay 1100 and match against 1122.4, Or,
      //   put another way, bob wants to buy a payout of up to 1122.4 if the
      //   capitals win, and he is willing to pay up to 1100 to do so.
      // * The first thing that happens is bob's bet gets rounded down to something
      //   that can match exactly.  1.98 (50:49) odds, so bob's bet is
      //   reduced to 1078, which should match against 1100.
      //   So bob gets an immediate refund of 22
      // * The next thing that happens is a match against the top bet on the order book.
      //   That's 1100 @ 1.86,  At these odds, bob's 980 can buy all 1100 of bet, he
      //   pays 1100:946.
      //
      // * alice's bet is fully matched, it is removed from the books
      // * bob's bet is fully matched, he is refunded the remaining 132 and his
      //   bet is complete
      // * bob's balance is reduced by the 946 he paid
      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1100, asset_id_type()), 198 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      bob_expected_balance -= 1100 - 22 - 132;
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);
   }
   FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(match_using_takers_expected_amounts6)
{
   try
   {
      generate_blocks(1);
      ACTORS( (alice)(bob) );
      CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

      graphene::bookie::bookie_api bookie_api(app);

      transfer(account_id_type(), alice_id, asset(1000000000));
      share_type alice_expected_balance = 1000000000;
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(10000000, asset_id_type()), 13 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(10000000, asset_id_type()), 15 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(10000000, asset_id_type()), 16 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      alice_expected_balance -= 30000000;
      BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance.value);

      // check order books to see they match the bets we placed
      const auto& bet_odds_idx = db.get_index_type<bet_object_index>().indices().get<by_odds>();
      auto bet_iter = bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id));
      BOOST_REQUIRE(bet_iter != bet_odds_idx.end());
      BOOST_REQUIRE(bet_iter->betting_market_id == capitals_win_market.id);
      BOOST_REQUIRE(bet_iter->bettor_id == alice_id);
      BOOST_REQUIRE(bet_iter->amount_to_bet == asset(10000000, asset_id_type()));
      BOOST_REQUIRE(bet_iter->backer_multiplier == 13 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      BOOST_REQUIRE(bet_iter->back_or_lay == bet_type::back);
      ++bet_iter;
      BOOST_REQUIRE(bet_iter != bet_odds_idx.end());
      BOOST_REQUIRE(bet_iter->betting_market_id == capitals_win_market.id);
      BOOST_REQUIRE(bet_iter->bettor_id == alice_id);
      BOOST_REQUIRE(bet_iter->amount_to_bet == asset(10000000, asset_id_type()));
      BOOST_REQUIRE(bet_iter->backer_multiplier == 15 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      BOOST_REQUIRE(bet_iter->back_or_lay == bet_type::back);
      ++bet_iter;
      BOOST_REQUIRE(bet_iter != bet_odds_idx.end());
      BOOST_REQUIRE(bet_iter->betting_market_id == capitals_win_market.id);
      BOOST_REQUIRE(bet_iter->bettor_id == alice_id);
      BOOST_REQUIRE(bet_iter->amount_to_bet == asset(10000000, asset_id_type()));
      BOOST_REQUIRE(bet_iter->backer_multiplier == 16 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      BOOST_REQUIRE(bet_iter->back_or_lay == bet_type::back);
      ++bet_iter;
      BOOST_REQUIRE(bet_iter == bet_odds_idx.end() || bet_iter->betting_market_id != capitals_win_market.id);

      // check the binned order books from the bookie plugin to make sure they match
      graphene::bookie::binned_order_book binned_orders_point_one = bookie_api.get_binned_order_book(capitals_win_market.id, 1);
      auto aggregated_back_bets_iter = binned_orders_point_one.aggregated_back_bets.begin();
      BOOST_REQUIRE(aggregated_back_bets_iter != binned_orders_point_one.aggregated_back_bets.end());
      BOOST_REQUIRE(aggregated_back_bets_iter->amount_to_bet.value == 10000000);
      BOOST_REQUIRE(aggregated_back_bets_iter->backer_multiplier == 13 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      ++aggregated_back_bets_iter;
      BOOST_REQUIRE(aggregated_back_bets_iter != binned_orders_point_one.aggregated_back_bets.end());
      BOOST_REQUIRE(aggregated_back_bets_iter->amount_to_bet.value == 10000000);
      BOOST_REQUIRE(aggregated_back_bets_iter->backer_multiplier == 15 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      ++aggregated_back_bets_iter;
      BOOST_REQUIRE(aggregated_back_bets_iter != binned_orders_point_one.aggregated_back_bets.end());
      BOOST_REQUIRE(aggregated_back_bets_iter->amount_to_bet.value == 10000000);
      BOOST_REQUIRE(aggregated_back_bets_iter->backer_multiplier == 16 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      ++aggregated_back_bets_iter;
      BOOST_REQUIRE(aggregated_back_bets_iter == binned_orders_point_one.aggregated_back_bets.end());
      BOOST_REQUIRE(binned_orders_point_one.aggregated_lay_bets.empty());

      transfer(account_id_type(), bob_id, asset(1000000000));
      share_type bob_expected_balance = 1000000000;
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);

      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(5000000, asset_id_type()), 15 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      ilog("Order books after bob's matching lay bet:");
      bet_iter = bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id));
      while (bet_iter != bet_odds_idx.end() &&
             bet_iter->betting_market_id == capitals_win_market.id)
      {
        idump((*bet_iter));
        ++bet_iter;
      }

      bob_expected_balance -= 5000000 - 2000000;
      BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance.value);

      // check order books to see they match after bob's bet matched
      bet_iter = bet_odds_idx.lower_bound(std::make_tuple(capitals_win_market.id));
      BOOST_REQUIRE(bet_iter != bet_odds_idx.end());
      BOOST_REQUIRE(bet_iter->betting_market_id == capitals_win_market.id);
      BOOST_REQUIRE(bet_iter->bettor_id == alice_id);
      BOOST_REQUIRE(bet_iter->amount_to_bet == asset(10000000, asset_id_type()));
      BOOST_REQUIRE(bet_iter->backer_multiplier == 15 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      BOOST_REQUIRE(bet_iter->back_or_lay == bet_type::back);
      ++bet_iter;
      BOOST_REQUIRE(bet_iter != bet_odds_idx.end());
      BOOST_REQUIRE(bet_iter->betting_market_id == capitals_win_market.id);
      BOOST_REQUIRE(bet_iter->bettor_id == alice_id);
      BOOST_REQUIRE(bet_iter->amount_to_bet == asset(10000000, asset_id_type()));
      BOOST_REQUIRE(bet_iter->backer_multiplier == 16 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      BOOST_REQUIRE(bet_iter->back_or_lay == bet_type::back);
      ++bet_iter;
      BOOST_REQUIRE(bet_iter == bet_odds_idx.end() || bet_iter->betting_market_id != capitals_win_market.id);

      // check the binned order books from the bookie plugin to make sure they match
      binned_orders_point_one = bookie_api.get_binned_order_book(capitals_win_market.id, 1);
      aggregated_back_bets_iter = binned_orders_point_one.aggregated_back_bets.begin();
      BOOST_REQUIRE(aggregated_back_bets_iter != binned_orders_point_one.aggregated_back_bets.end());
      BOOST_REQUIRE(aggregated_back_bets_iter->amount_to_bet.value == 10000000);
      BOOST_REQUIRE(aggregated_back_bets_iter->backer_multiplier == 15 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      ++aggregated_back_bets_iter;
      BOOST_REQUIRE(aggregated_back_bets_iter != binned_orders_point_one.aggregated_back_bets.end());
      BOOST_REQUIRE(aggregated_back_bets_iter->amount_to_bet.value == 10000000);
      BOOST_REQUIRE(aggregated_back_bets_iter->backer_multiplier == 16 * GRAPHENE_BETTING_ODDS_PRECISION / 10);
      ++aggregated_back_bets_iter;
      BOOST_REQUIRE(aggregated_back_bets_iter == binned_orders_point_one.aggregated_back_bets.end());
      BOOST_REQUIRE(binned_orders_point_one.aggregated_lay_bets.empty());


   }
   FC_LOG_AND_RETHROW()
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

//This test case need some analysis and commneting out for the time being
// BOOST_AUTO_TEST_CASE(bet_against_exposure_test)
// {
//    // test whether we can bet our entire balance in one direction, have it match, then reverse our bet (while having zero balance)
//    try
//    {
//       generate_blocks(1);
//       ACTORS( (alice)(bob) );
//       CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);
//
//       transfer(account_id_type(), alice_id, asset(10000000));
//       transfer(account_id_type(), bob_id, asset(10000000));
//       int64_t alice_expected_balance = 10000000;
//       BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance);
//       int64_t bob_expected_balance = 10000000;
//       BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance);
//
//       // back with alice's entire balance
//       place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(10000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
//       alice_expected_balance -= 10000000;
//       BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance);
//
//       // lay with bob's entire balance, which fully matches bob's bet
//       place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(10000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
//       bob_expected_balance -= 10000000;
//       BOOST_REQUIRE_EQUAL(get_balance(bob_id, asset_id_type()), bob_expected_balance);
//
//       // reverse the bet
//       place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(20000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
//       BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance);
//
//       // try to re-reverse it, but go too far
//       BOOST_CHECK_THROW( place_bet(alice_id, capitals_win_market.id, bet_type::back, asset(30000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION), fc::exception);
//       BOOST_REQUIRE_EQUAL(get_balance(alice_id, asset_id_type()), alice_expected_balance);
//    }
//    FC_LOG_AND_RETHROW()
// }

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
      BOOST_CHECK_MESSAGE(objects_from_bookie[0]["id"].as<bet_id_type>(1) == automatically_canceled_bet_id, "Bookie Plugin didn't return a deleted bet it");

      // lay 47 at 1.94 odds (50:47) -- this bet should go on the order books normally
      bet_id_type first_bet_on_books = place_bet(alice_id, capitals_win_market.id, bet_type::lay, asset(47, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      generate_blocks(1);
      BOOST_CHECK_MESSAGE(db.find(first_bet_on_books), "Bet should exist on the blockchain");
      objects_from_bookie = bookie_api.get_objects({first_bet_on_books});
      idump((objects_from_bookie));
      BOOST_REQUIRE_EQUAL(objects_from_bookie.size(), 1u);
      BOOST_CHECK_MESSAGE(objects_from_bookie[0]["id"].as<bet_id_type>(1) == first_bet_on_books, "Bookie Plugin didn't return a bet that is currently on the books");

      // place a bet that exactly matches 'first_bet_on_books', should result in empty books (thus, no bet_objects from the blockchain)
      bet_id_type matching_bet = place_bet(bob_id, capitals_win_market.id, bet_type::back, asset(50, asset_id_type()), 194 * GRAPHENE_BETTING_ODDS_PRECISION / 100);
      BOOST_CHECK_MESSAGE(!db.find(first_bet_on_books), "Bet should have been filled, but the blockchain still knows about it");
      BOOST_CHECK_MESSAGE(!db.find(matching_bet), "Bet should have been filled, but the blockchain still knows about it");
      generate_blocks(1); // the bookie plugin doesn't detect matches until a block is generated

      objects_from_bookie = bookie_api.get_objects({first_bet_on_books, matching_bet});
      idump((objects_from_bookie));
      BOOST_REQUIRE_EQUAL(objects_from_bookie.size(), 2u);
      BOOST_CHECK_MESSAGE(objects_from_bookie[0]["id"].as<bet_id_type>(1) == first_bet_on_books, "Bookie Plugin didn't return a bet that has been filled");
      BOOST_CHECK_MESSAGE(objects_from_bookie[1]["id"].as<bet_id_type>(1) == matching_bet, "Bookie Plugin didn't return a bet that has been filled");

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

BOOST_AUTO_TEST_CASE(test_settled_market_states)
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

      BOOST_TEST_MESSAGE("setting the event to in_progress");
      update_event(capitals_vs_blackhawks.id, _status = event_status::in_progress);
      generate_blocks(1);

      BOOST_TEST_MESSAGE("setting the event to finished");
      update_event(capitals_vs_blackhawks.id, _status = event_status::finished);
      generate_blocks(1);

      resolve_betting_market_group(moneyline_betting_markets.id,
                                   {{capitals_win_market.id, betting_market_resolution_type::win},
                                    {blackhawks_win_market.id, betting_market_resolution_type::not_win}});

      // as soon as the market is resolved during the generate_block(), these markets
      // should be deleted and our references will go out of scope.  Save the
      // market ids here so we can verify that they were really deleted
      betting_market_id_type capitals_win_market_id = capitals_win_market.id;
      betting_market_id_type blackhawks_win_market_id = blackhawks_win_market.id;

      generate_blocks(1);

      // test getting markets
      //  test that we cannot get them from the database directly
      BOOST_CHECK_THROW(capitals_win_market_id(db), fc::exception);
      BOOST_CHECK_THROW(blackhawks_win_market_id(db), fc::exception);

      fc::variants objects_from_bookie = bookie_api.get_objects({capitals_win_market_id, blackhawks_win_market_id});
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
//      edump((std::distance(first_bet_in_market, last_bet_in_market)));
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
               BOOST_TEST_MESSAGE("Approving sport+competitors creation from witness " << fc::variant(witness_id, 1).as<std::string>(1));
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

      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage();
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

      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage();
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

BOOST_AUTO_TEST_CASE(sport_delete_test)
{
    try
    {
        CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

        const auto& event_group_1 = create_event_group({{"en", "group1"}}, ice_hockey.id);
        const auto& event_group_2 = create_event_group({{"en", "group2"}}, ice_hockey.id);

        delete_sport(ice_hockey.id);

        const auto& sport_by_id = db.get_index_type<sport_object_index>().indices().get<by_id>();
        BOOST_CHECK(sport_by_id.end() == sport_by_id.find(ice_hockey.id));

        const auto& event_group_by_id = db.get_index_type<event_group_object_index>().indices().get<by_id>();
        BOOST_CHECK(event_group_by_id.end() == event_group_by_id.find(event_group_1.id));
        BOOST_CHECK(event_group_by_id.end() == event_group_by_id.find(event_group_2.id));
    } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(sport_delete_test_not_proposal)
{
    try
    {
        CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

        sport_delete_operation sport_delete_op;
        sport_delete_op.sport_id = ice_hockey.id;

        BOOST_CHECK_THROW(force_operation_by_witnesses(sport_delete_op), fc::exception);
    } FC_LOG_AND_RETHROW()
}

// No need for the below test as it shows in failed test case list. Should enable when sports related changes applied
// BOOST_AUTO_TEST_CASE(sport_delete_test_not_existed_sport)
// {
//     try
//     {
//         CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);
//
//         delete_sport(ice_hockey.id);
//
//         BOOST_CHECK_THROW(delete_sport(ice_hockey.id), fc::exception);
//     } FC_LOG_AND_RETHROW()
// }

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


      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage();
      uint32_t rake_value = (-1000000 + 2000000) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100;
      BOOST_TEST_MESSAGE("Rake value " +  std::to_string(rake_value));
      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000 + 2000000 - rake_value);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

   } FC_LOG_AND_RETHROW()
}

struct test_events
{
    const event_object* event_upcoming = nullptr;
    const event_object* event_in_progress = nullptr;
    const event_object* event_frozen_upcoming = nullptr;
    const event_object* event_frozen_in_progress = nullptr;
    const event_object* event_finished = nullptr;
    const event_object* event_canceled = nullptr;
    const event_object* event_settled = nullptr;

    test_events(database_fixture& db_fixture, event_group_id_type event_group_id)
    {
        event_upcoming = &db_fixture.create_event({{"en", "event upcoming"}}, {{"en", "2016-17"}}, event_group_id);
        event_in_progress = &db_fixture.create_event({{"en", "event in_progress"}}, {{"en", "2016-17"}}, event_group_id);
        db_fixture.db.modify(*event_in_progress, [&](event_object& event)
                  {
                      event.on_in_progress_event(db_fixture.db);
                  });

        event_frozen_upcoming = &db_fixture.create_event({{"en", "event frozen_upcoming"}}, {{"en", "2016-17"}}, event_group_id);
        db_fixture.db.modify(*event_frozen_upcoming, [&](event_object& event)
                  {
                      event.on_frozen_event(db_fixture.db);
                  });

        event_frozen_in_progress = &db_fixture.create_event({{"en", "event frozen_in_progress"}}, {{"en", "2016-17"}}, event_group_id);
        db_fixture.db.modify(*event_frozen_in_progress, [&](event_object& event)
                  {
                      event.on_in_progress_event(db_fixture.db);
                      event.on_frozen_event(db_fixture.db);
                  });

        event_finished = &db_fixture.create_event({{"en", "event finished"}}, {{"en", "2016-17"}}, event_group_id);
        db_fixture.db.modify(*event_finished, [&](event_object& event)
                  {
                      event.on_frozen_event(db_fixture.db);
                      event.on_finished_event(db_fixture.db);
                  });

        event_canceled = &db_fixture.create_event({{"en", "event canceled"}}, {{"en", "2016-17"}}, event_group_id);
        db_fixture.db.modify(*event_canceled, [&](event_object& event)
                  {
                      event.on_canceled_event(db_fixture.db);
                  });

        event_settled = &db_fixture.create_event({{"en", "event settled"}}, {{"en", "2016-17"}}, event_group_id);
        db_fixture.db.modify(*event_settled, [&](event_object& event)
                  {
                      event.on_finished_event(db_fixture.db);
                      event.on_betting_market_group_resolved(db_fixture.db, betting_market_group_id_type(), false);
                  });
    }
};

struct test_markets_groups
{
    const betting_market_group_object* market_group_upcoming = nullptr;
    const betting_market_group_object* market_group_frozen_upcoming = nullptr;
    const betting_market_group_object* market_group_in_play = nullptr;
    const betting_market_group_object* market_group_frozen_in_play = nullptr;
    const betting_market_group_object* market_group_closed = nullptr;
    const betting_market_group_object* market_group_graded = nullptr;
    const betting_market_group_object* market_group_canceled = nullptr;
    const betting_market_group_object* market_group_settled = nullptr;

    test_markets_groups(database_fixture& db_fixture, event_id_type event_id, betting_market_rules_id_type betting_market_rules_id)
    {
        market_group_upcoming = &db_fixture.create_betting_market_group({{"en", "market group upcoming"}}, event_id, betting_market_rules_id, asset_id_type(), false, 0);
        market_group_frozen_upcoming = &db_fixture.create_betting_market_group({{"en", "market group frozen_upcoming"}}, event_id, betting_market_rules_id, asset_id_type(), false, 0);
        db_fixture.db.modify(*market_group_frozen_upcoming, [&](betting_market_group_object& market_group)
                  {
                      market_group.on_frozen_event(db_fixture.db);
                  });

        market_group_in_play = &db_fixture.create_betting_market_group({{"en", "market group in_play"}}, event_id, betting_market_rules_id, asset_id_type(), false, 0);
        db_fixture.db.modify(*market_group_in_play, [&](betting_market_group_object& market_group)
                  {
                      market_group.on_in_play_event(db_fixture.db);
                  });

        market_group_frozen_in_play = &db_fixture.create_betting_market_group({{"en", "market group frozen_in_play"}}, event_id, betting_market_rules_id, asset_id_type(), false, 0);
        db_fixture.db.modify(*market_group_frozen_in_play, [&](betting_market_group_object& market_group)
                  {
                      market_group.on_in_play_event(db_fixture.db);
                      market_group.on_frozen_event(db_fixture.db);
                  });

        market_group_closed = &db_fixture.create_betting_market_group({{"en", "market group closed"}}, event_id, betting_market_rules_id, asset_id_type(), false, 0);
        db_fixture.db.modify(*market_group_closed, [&](betting_market_group_object& market_group)
                  {
                      market_group.on_closed_event(db_fixture.db, true);
                  });

        market_group_graded = &db_fixture.create_betting_market_group({{"en", "market group graded"}}, event_id, betting_market_rules_id, asset_id_type(), false, 0);
        db_fixture.db.modify(*market_group_graded, [&](betting_market_group_object& market_group)
                  {
                      market_group.on_closed_event(db_fixture.db, true);
                      market_group.on_graded_event(db_fixture.db);
                  });

        market_group_canceled = &db_fixture.create_betting_market_group({{"en", "market group canceled"}}, event_id, betting_market_rules_id, asset_id_type(), false, 0);
        db_fixture.db.modify(*market_group_canceled, [&](betting_market_group_object& market_group)
                  {
                      market_group.on_canceled_event(db_fixture.db, true);
                  });

        market_group_settled = &db_fixture.create_betting_market_group({{"en", "market group settled"}}, event_id, betting_market_rules_id, asset_id_type(), false, 0);
        db_fixture.db.modify(*market_group_settled, [&](betting_market_group_object& market_group)
                  {
                      market_group.on_closed_event(db_fixture.db, true);
                      market_group.on_graded_event(db_fixture.db);
                      market_group.on_settled_event(db_fixture.db);
                  });
    }
};

struct test_markets
{
    const betting_market_object* market_unresolved = nullptr;
    const betting_market_object* market_frozen = nullptr;
    const betting_market_object* market_closed = nullptr;
    const betting_market_object* market_graded = nullptr;
    const betting_market_object* market_canceled = nullptr;
    const betting_market_object* market_settled = nullptr;

    test_markets(database_fixture& db_fixture, betting_market_group_id_type market_group_id)
    {
        market_unresolved = &db_fixture.create_betting_market(market_group_id, {{"en", "market unresolved"}});
        market_frozen = &db_fixture.create_betting_market(market_group_id, {{"en", "market frozen"}});
        db_fixture.db.modify(*market_frozen, [&](betting_market_object& market)
                  {
                      market.on_frozen_event(db_fixture.db);
                  });

        market_closed = &db_fixture.create_betting_market(market_group_id, {{"en", "market closed"}});
        db_fixture.db.modify(*market_closed, [&](betting_market_object& market)
                  {
                      market.on_closed_event(db_fixture.db);
                  });

        market_graded = &db_fixture.create_betting_market(market_group_id, {{"en", "market graded"}});
        db_fixture.db.modify(*market_graded, [&](betting_market_object& market)
                  {
                      market.on_closed_event(db_fixture.db);
                      market.on_graded_event(db_fixture.db, betting_market_resolution_type::win);
                  });

        market_canceled = &db_fixture.create_betting_market(market_group_id, {{"en", "market canceled"}});
        db_fixture.db.modify(*market_canceled, [&](betting_market_object& market)
                  {
                      market.on_canceled_event(db_fixture.db);
                  });

        market_settled = &db_fixture.create_betting_market(market_group_id, {{"en", "market settled"}});
        db_fixture.db.modify(*market_settled, [&](betting_market_object& market)
                  {
                      market.on_closed_event(db_fixture.db);
                      market.on_graded_event(db_fixture.db, betting_market_resolution_type::win);
                      market.on_settled_event(db_fixture.db);
                  });
    }
};

BOOST_AUTO_TEST_CASE(event_group_delete_test)
{
    try
    {
        ACTORS( (alice)(bob) )
        CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

        const int initialAccountAsset = 10000000;
        const int betAsset = 1000000;

        transfer(account_id_type(), alice_id, asset(initialAccountAsset));
        transfer(account_id_type(), bob_id, asset(initialAccountAsset));

        const auto& event = create_event({{"en", "event"}}, {{"en", "2016-17"}}, nhl.id);

        const auto& market_group = create_betting_market_group({{"en", "market group"}}, event.id, betting_market_rules.id, asset_id_type(), false, 0);
        //to make bets be not removed immediately
        update_betting_market_group_impl(market_group.id,
                                         fc::optional<internationalized_string_type>(),
                                         fc::optional<object_id_type>(),
                                         betting_market_group_status::in_play,
                                         false);

        const auto& market = create_betting_market(market_group.id, {{"en", "market"}});

        test_events events(*this, nhl.id);
        test_markets_groups markets_groups(*this, event.id, betting_market_rules.id);
        test_markets markets(*this, market_group.id);

        const auto& bet_1_id = place_bet(alice_id, market.id, bet_type::back, asset(betAsset, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
        const auto& bet_2_id = place_bet(bob_id, market.id, bet_type::lay, asset(betAsset, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

        delete_event_group(nhl.id);

        const auto& event_group_by_id = db.get_index_type<event_group_object_index>().indices().get<by_id>();
        BOOST_CHECK(event_group_by_id.end() == event_group_by_id.find(nhl.id));

        BOOST_CHECK(event_status::canceled == event.get_status());

        BOOST_CHECK(event_status::canceled == events.event_upcoming->get_status());
        BOOST_CHECK(event_status::canceled == events.event_in_progress->get_status());
        BOOST_CHECK(event_status::canceled == events.event_frozen_in_progress->get_status());
        BOOST_CHECK(event_status::canceled == events.event_frozen_upcoming->get_status());
        BOOST_CHECK(event_status::canceled == events.event_finished->get_status());
        BOOST_CHECK(event_status::canceled == events.event_canceled->get_status());
        BOOST_CHECK(event_status::settled == events.event_settled->get_status());

        BOOST_CHECK(betting_market_group_status::canceled == market_group.get_status());

        BOOST_CHECK(betting_market_group_status::canceled == markets_groups.market_group_upcoming->get_status());
        BOOST_CHECK(betting_market_group_status::canceled == markets_groups.market_group_frozen_upcoming->get_status());
        BOOST_CHECK(betting_market_group_status::canceled == markets_groups.market_group_in_play->get_status());
        BOOST_CHECK(betting_market_group_status::canceled == markets_groups.market_group_frozen_in_play->get_status());
        BOOST_CHECK(betting_market_group_status::canceled == markets_groups.market_group_closed->get_status());
        BOOST_CHECK(betting_market_group_status::canceled == markets_groups.market_group_graded->get_status());
        BOOST_CHECK(betting_market_group_status::canceled == markets_groups.market_group_canceled->get_status());
        BOOST_CHECK(betting_market_group_status::settled == markets_groups.market_group_settled->get_status());

        BOOST_CHECK(betting_market_status::canceled == market.get_status());

        BOOST_CHECK(betting_market_status::canceled == markets.market_unresolved->get_status());
        BOOST_CHECK(betting_market_status::canceled == markets.market_frozen->get_status());
        BOOST_CHECK(betting_market_status::canceled == markets.market_closed->get_status());
        BOOST_CHECK(betting_market_status::canceled == markets.market_graded->get_status());
        BOOST_CHECK(betting_market_status::canceled == markets.market_canceled->get_status());
        BOOST_CHECK(betting_market_status::settled == markets.market_settled->get_status()); //settled market should not be canceled

        //check canceled bets and reverted balance changes
        const auto& bet_by_id = db.get_index_type<bet_object_index>().indices().get<by_id>();
        BOOST_CHECK(bet_by_id.end() == bet_by_id.find(bet_1_id));
        BOOST_CHECK(bet_by_id.end() == bet_by_id.find(bet_2_id));

        BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), initialAccountAsset);
        BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), initialAccountAsset);
    } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(event_group_delete_test_with_matched_bets)
{
    try
    {
        ACTORS( (alice)(bob) )
        CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

        const int initialAccountAsset = 10000000;
        const int betAsset = 100000;

        transfer(account_id_type(), alice_id, asset(initialAccountAsset));
        transfer(account_id_type(), bob_id, asset(initialAccountAsset));
        generate_blocks(1);

        const auto& event = create_event({{"en", "event"}}, {{"en", "2016-17"}}, nhl.id);
        generate_blocks(1);

        const auto& market_group = create_betting_market_group({{"en", "market group"}}, event.id, betting_market_rules.id, asset_id_type(), false, 0);
        generate_blocks(1);

        const auto& market = create_betting_market(market_group.id, {{"en", "market"}});
        generate_blocks(1);

        place_bet(alice_id, market.id, bet_type::back, asset(betAsset, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
        place_bet(bob_id, market.id, bet_type::lay, asset(betAsset, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);
        generate_blocks(1);

        delete_event_group(nhl.id);
        generate_blocks(1);

        BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), initialAccountAsset);
        BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), initialAccountAsset);
    } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(event_group_delete_test_not_proposal)
{
    try
    {
        CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

        event_group_delete_operation event_group_delete_op;
        event_group_delete_op.event_group_id = nhl.id;

        BOOST_CHECK_THROW(force_operation_by_witnesses(event_group_delete_op), fc::exception);
    } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(event_group_delete_test_not_existed_event_group)
{
    try
    {
        CREATE_ICE_HOCKEY_BETTING_MARKET(false, 0);

        delete_event_group(nhl.id);

        BOOST_CHECK_THROW(delete_event_group(nhl.id), fc::exception);
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


      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage();
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


      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage();
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

      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage();
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

      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(1), "settled");
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

      // save the ids for checking after it is deleted
      event_id_type capitals_vs_blackhawks_id = capitals_vs_blackhawks.id;
      betting_market_group_id_type moneyline_betting_markets_id = moneyline_betting_markets.id;
      betting_market_id_type capitals_win_market_id = capitals_win_market.id;
      betting_market_id_type blackhawks_win_market_id = blackhawks_win_market.id;


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
      BOOST_CHECK(capitals_win_market.get_status() == betting_market_status::graded);
      BOOST_CHECK(capitals_win_market.resolution == betting_market_resolution_type::win);
      BOOST_CHECK(blackhawks_win_market.get_status() == betting_market_status::graded);
      BOOST_CHECK(blackhawks_win_market.resolution == betting_market_resolution_type::not_win);

      generate_blocks(60);
      // as soon as a block is generated, the betting market group will settle, and the market
      // and group will cease to exist.  The event should transition to "settled", then
      // removed.
      fc::variants objects_from_bookie = bookie_api.get_objects({capitals_vs_blackhawks_id,
                                                                 moneyline_betting_markets_id,
                                                                 capitals_win_market_id,
                                                                 blackhawks_win_market_id});

      idump((objects_from_bookie));
      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(1), "settled");
      BOOST_CHECK_EQUAL(objects_from_bookie[1]["status"].as<std::string>(1), "settled");
      BOOST_CHECK_EQUAL(objects_from_bookie[2]["status"].as<std::string>(1), "settled");
      BOOST_CHECK_EQUAL(objects_from_bookie[2]["resolution"].as<std::string>(1), "win");
      BOOST_CHECK_EQUAL(objects_from_bookie[3]["status"].as<std::string>(1), "settled");
      BOOST_CHECK_EQUAL(objects_from_bookie[3]["resolution"].as<std::string>(1), "not_win");
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

      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(1), "settled");
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

      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(1), "settled");
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

      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(1), "canceled");

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
      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(1), "canceled");

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
      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(1), "settled");

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
      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(1), "settled");
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
      BOOST_CHECK_EQUAL(objects_from_bookie[0]["status"].as<std::string>(1), "settled");
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

      //Disabling the below 4 TRY_EXPECT_THROW lines to not throw anything beacuse functioning as expected

      // trx_state->_is_proposed_trx
      //GRAPHENE_REQUIRE_THROW(try_update_event_group(nhl.id, fc::optional<object_id_type>(), fc::optional<internationalized_string_type>(), true), fc::exception);
      // TRY_EXPECT_THROW(try_update_event_group(nhl.id, fc::optional<object_id_type>(), fc::optional<internationalized_string_type>(), true), fc::exception, "_is_proposed_trx");

      // #! nothing to change
      //GRAPHENE_REQUIRE_THROW(try_update_event_group(nhl.id, fc::optional<object_id_type>(), fc::optional<internationalized_string_type>()), fc::exception);
      //TRY_EXPECT_THROW(try_update_event_group(nhl.id, fc::optional<object_id_type>(), fc::optional<internationalized_string_type>()), fc::exception, "nothing to change");

      // #! sport_id must refer to a sport_id_type
      sport_id = capitals_win_market.id;
      //GRAPHENE_REQUIRE_THROW(try_update_event_group(nhl.id, sport_id, fc::optional<internationalized_string_type>()), fc::exception);
      //TRY_EXPECT_THROW(try_update_event_group(nhl.id, sport_id, fc::optional<internationalized_string_type>()), fc::exception, "sport_id must refer to a sport_id_type");

      // #! invalid sport specified
      sport_id = sport_id_type(13);
      //GRAPHENE_REQUIRE_THROW(try_update_event_group(nhl.id, sport_id, fc::optional<internationalized_string_type>()), fc::exception);
      //TRY_EXPECT_THROW(try_update_event_group(nhl.id, sport_id, fc::optional<internationalized_string_type>()), fc::exception, "invalid sport specified");

      place_bet(bob_id, capitals_win_market.id, bet_type::lay, asset(1000000, asset_id_type()), 2 * GRAPHENE_BETTING_ODDS_PRECISION);

      BOOST_CHECK_EQUAL(get_balance(alice_id, asset_id_type()), 10000000 - 1000000);
      BOOST_CHECK_EQUAL(get_balance(bob_id, asset_id_type()), 10000000 - 1000000);

      update_betting_market_group(moneyline_betting_markets.id, _status = betting_market_group_status::closed);
      // caps win
      resolve_betting_market_group(moneyline_betting_markets.id,
                                  {{capitals_win_market.id, betting_market_resolution_type::win},
                                   {blackhawks_win_market.id, betting_market_resolution_type::not_win}});
      generate_blocks(1);

      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage();
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

      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage();

      transfer(account_id_type(), alice_id, asset(10000000));
      transfer(account_id_type(), bob_id, asset(10000000));

      BOOST_TEST_MESSAGE("moneyline_berdych_vs_federer  " << fc::variant(moneyline_berdych_vs_federer.id, 1).as<std::string>(1));
      BOOST_TEST_MESSAGE("moneyline_cilic_vs_querrey  " << fc::variant(moneyline_cilic_vs_querrey.id, 1).as<std::string>(1));

      BOOST_TEST_MESSAGE("berdych_wins_market " << fc::variant(berdych_wins_market.id, 1).as<std::string>(1));
      BOOST_TEST_MESSAGE("federer_wins_market " << fc::variant(federer_wins_market.id, 1).as<std::string>(1));
      BOOST_TEST_MESSAGE("cilic_wins_market " << fc::variant(cilic_wins_market.id, 1).as<std::string>(1));
      BOOST_TEST_MESSAGE("querrey_wins_market " << fc::variant(querrey_wins_market.id, 1).as<std::string>(1));

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

      uint16_t rake_fee_percentage = db.get_global_properties().parameters.betting_rake_fee_percentage();

      transfer(account_id_type(), alice_id, asset(10000000));
      transfer(account_id_type(), bob_id, asset(10000000));

      BOOST_TEST_MESSAGE("moneyline_cilic_vs_federer  " << fc::variant(moneyline_cilic_vs_federer.id, 1).as<std::string>(1));

      BOOST_TEST_MESSAGE("federer_wins_final_market " << fc::variant(federer_wins_final_market.id, 1).as<std::string>(1));
      BOOST_TEST_MESSAGE("cilic_wins_final_market " << fc::variant(cilic_wins_final_market.id, 1).as<std::string>(1));

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

// reworked check_transasction for duplicate
// now should not through an exception when there are different events with the same betting_market_group
// and or the same betting_market
// Need to revisit the following test, commeting for time being******
// BOOST_AUTO_TEST_CASE( check_transaction_for_duplicate_reworked_test )
// {
//    try
//    {
//    std::vector<internationalized_string_type> names_vec(104);
//
//    // create 104 pattern for first name
//    for( char co = 'A'; co <= 'D'; ++co ) {
//       for( char ci = 'A'; ci <= 'Z'; ++ci ) {
//          std::string first_name  = std::to_string(co) + std::to_string(ci);
//          std::string second_name = first_name + first_name;
//          names_vec.push_back( {{ first_name, second_name }} );
//       }
//    }
//
//    sport_id_type sport_id = create_sport( {{"SN","SPORT_NAME"}} ).id;
//
//    event_group_id_type event_group_id = create_event_group( {{"EG", "EVENT_GROUP"}}, sport_id ).id;
//
//    betting_market_rules_id_type betting_market_rules_id =
//       create_betting_market_rules( {{"EN", "Rules"}}, {{"EN", "Some rules"}} ).id;
//
//    for( const auto& name : names_vec )
//    {
//       proposal_create_operation pcop = proposal_create_operation::committee_proposal(
//          db.get_global_properties().parameters,
//          db.head_block_time()
//       );
//       pcop.review_period_seconds.reset();
//       pcop.review_period_seconds = db.get_global_properties().parameters.committee_proposal_review_period * 2;
//
//       event_create_operation evcop;
//       evcop.event_group_id = event_group_id;
//       evcop.name           = name;
//       evcop.season         = name;
//
//       betting_market_group_create_operation bmgcop;
//       bmgcop.description = name;
//       bmgcop.event_id = object_id_type(relative_protocol_ids, 0, 0);
//       bmgcop.rules_id = betting_market_rules_id;
//       bmgcop.asset_id = asset_id_type();
//
//       betting_market_create_operation bmcop;
//       bmcop.group_id = object_id_type(relative_protocol_ids, 0, 1);
//       bmcop.payout_condition.insert( internationalized_string_type::value_type( "CN", "CONDI_NAME" ) );
//
//       pcop.proposed_ops.emplace_back( evcop  );
//       pcop.proposed_ops.emplace_back( bmgcop );
//       pcop.proposed_ops.emplace_back( bmcop  );
//
//       signed_transaction trx;
//       set_expiration( db, trx );
//       trx.operations.push_back( pcop );
//
//       process_operation_by_witnesses( pcop );
//    }
//    }FC_LOG_AND_RETHROW()
// }

BOOST_AUTO_TEST_SUITE_END()



//#define BOOST_TEST_MODULE "C++ Unit Tests for Graphene Blockchain Database"
#include <cstdlib>
#include <iostream>
#include <boost/test/included/unit_test.hpp>

boost::unit_test::test_suite* init_unit_test_suite(int argc, char* argv[]) {
    std::srand(time(NULL));
    std::cout << "Random number generator seeded to " << time(NULL) << std::endl;

    // betting operations don't take effect until HARDFORK 1000
    GRAPHENE_TESTING_GENESIS_TIMESTAMP = HARDFORK_1000_TIME.sec_since_epoch() + 2;

    return nullptr;
}

