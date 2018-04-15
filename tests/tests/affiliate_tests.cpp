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

#include <boost/test/unit_test.hpp>

#include "../common/tournament_helper.hpp"

#include <graphene/chain/affiliate_payout.hpp>
#include <graphene/chain/game_object.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/match_object.hpp>
#include <graphene/chain/tournament_object.hpp>

using namespace graphene::chain;
using namespace graphene::db;

BOOST_FIXTURE_TEST_SUITE( affiliate_tests, database_fixture )

BOOST_AUTO_TEST_CASE( account_test )
{
   ACTORS( (alice)(ann)(audrey) );
   fund( alice );

   const fc::ecc::private_key paula_private_key = generate_private_key( "paula" );

   account_create_operation aco = make_account( "paula", paula_private_key.get_public_key() );
   aco.extensions.value.affiliate_distributions = affiliate_reward_distributions();
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[alice_id] = GRAPHENE_100_PERCENT;
   test::set_expiration( db, trx );
   trx.clear();
   trx.operations.push_back( aco );
   // not allowed before hardfork
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   proposal_create_operation pco;
   pco.fee_paying_account = alice_id;
   aco.registrar = ann_id;
   pco.proposed_ops.emplace_back( aco );
   aco.registrar = account_id_type();
   pco.expiration_time = db.head_block_time() + fc::days(1);
   trx.clear();
   trx.operations.push_back( pco );
   // not allowed before hardfork
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   generate_blocks( HARDFORK_999_TIME );
   generate_block();

   // Proposal is now allowed
   pco.expiration_time = db.head_block_time() + fc::days(1);
   trx.clear();
   trx.operations.push_back( pco );
   test::set_expiration( db, trx );
   db.push_transaction(trx, ~0);

   // Must contain at least one app-tag
   aco.extensions.value.affiliate_distributions->_dists.clear();
   trx.clear();
   trx.operations.push_back( aco );
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   // Distribution for app-tag must be non-empty
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist.clear();
   trx.clear();
   trx.operations.push_back( aco );
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   // If more than one app-tag given, neither can be empty
   aco.extensions.value.affiliate_distributions->_dists[rps]._dist[alice_id] = GRAPHENE_100_PERCENT;
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist.clear();
   trx.clear();
   trx.operations.push_back( aco );
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   // Sum of percentage must be 100%
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[ann_id] = 1;
   trx.clear();
   trx.operations.push_back( aco );
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   // Individual percentages cannot exceed 100%
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[ann_id] = -1;
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[audrey_id] = 1 + GRAPHENE_100_PERCENT;
   trx.clear();
   trx.operations.push_back( aco );
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   // Sum of percentage must be 100%
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[ann_id] = GRAPHENE_100_PERCENT - 10;
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[audrey_id] = 9;
   trx.clear();
   trx.operations.push_back( aco );
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   // Ok
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[audrey_id] = 10;
   trx.clear();
   trx.operations.push_back( aco );
   db.push_transaction(trx, ~0);

   const auto& paula = get_account( aco.name );
   BOOST_CHECK( paula.affiliate_distributions.valid() );
   BOOST_CHECK_EQUAL( 2, paula.affiliate_distributions->_dists.size() );

   const auto& app_rps = paula.affiliate_distributions->_dists.find( rps );
   BOOST_CHECK( app_rps != paula.affiliate_distributions->_dists.end() );
   BOOST_CHECK_EQUAL( 1, app_rps->second._dist.size() );
   const auto& share_alice = app_rps->second._dist.find( alice_id );
   BOOST_CHECK( share_alice != app_rps->second._dist.end() );
   BOOST_CHECK_EQUAL( GRAPHENE_100_PERCENT, share_alice->second );

   const auto& app_bookie = paula.affiliate_distributions->_dists.find( bookie );
   BOOST_CHECK( app_bookie != paula.affiliate_distributions->_dists.end() );
   BOOST_CHECK_EQUAL( 2, app_bookie->second._dist.size() );
   const auto& share_ann = app_bookie->second._dist.find( ann_id );
   BOOST_CHECK( share_ann != app_bookie->second._dist.end() );
   BOOST_CHECK_EQUAL( GRAPHENE_100_PERCENT - 10, share_ann->second );
   const auto& share_audrey = app_bookie->second._dist.find( audrey_id );
   BOOST_CHECK( share_audrey != app_bookie->second._dist.end() );
   BOOST_CHECK_EQUAL( 10, share_audrey->second );
}

BOOST_AUTO_TEST_CASE( affiliate_payout_helper_test )
{
   ACTORS( (alice)(ann)(audrey)(irene) );

   const asset_id_type btc_id = create_user_issued_asset( "BTC", irene, 0 ).id;
   issue_uia( irene, asset( 100000, btc_id ) );

   generate_blocks( HARDFORK_999_TIME );
   generate_block();

   test::set_expiration( db, trx );
   trx.clear();

   // Paula: 100% to Alice for Bookie / nothing for RPS
   const fc::ecc::private_key paula_private_key = generate_private_key( "paula" );
   account_create_operation aco = make_account( "paula", paula_private_key.get_public_key() );
   aco.extensions.value.affiliate_distributions = affiliate_reward_distributions();
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[alice_id] = GRAPHENE_100_PERCENT;
   trx.operations.push_back( aco );

   // Penny: For Bookie 60% to Alice + 40% to Ann / For RPS 20% to Alice, 30% to Ann, 50% to Audrey
   const fc::ecc::private_key penny_private_key = generate_private_key( "penny" );
   aco = make_account( "penny", penny_private_key.get_public_key() );
   aco.extensions.value.affiliate_distributions = affiliate_reward_distributions();
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[alice_id] = GRAPHENE_100_PERCENT * 3 / 5;
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[ann_id]   = GRAPHENE_100_PERCENT
                                                                                  - aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[alice_id];
   aco.extensions.value.affiliate_distributions->_dists[rps]._dist[alice_id]    = GRAPHENE_100_PERCENT / 5;
   aco.extensions.value.affiliate_distributions->_dists[rps]._dist[audrey_id]   = GRAPHENE_100_PERCENT / 2;
   aco.extensions.value.affiliate_distributions->_dists[rps]._dist[ann_id]      = GRAPHENE_100_PERCENT
                                                                                  - aco.extensions.value.affiliate_distributions->_dists[rps]._dist[alice_id]
                                                                                  - aco.extensions.value.affiliate_distributions->_dists[rps]._dist[audrey_id];
   trx.operations.push_back( aco );

   // Petra: nothing for Bookie / For RPS 10% to Ann, 90% to Audrey
   const fc::ecc::private_key petra_private_key = generate_private_key( "petra" );
   aco = make_account( "petra", petra_private_key.get_public_key() );
   aco.extensions.value.affiliate_distributions = affiliate_reward_distributions();
   aco.extensions.value.affiliate_distributions->_dists[rps]._dist[ann_id]    = GRAPHENE_100_PERCENT / 10;
   aco.extensions.value.affiliate_distributions->_dists[rps]._dist[audrey_id] = GRAPHENE_100_PERCENT
                                                                                - aco.extensions.value.affiliate_distributions->_dists[rps]._dist[ann_id];
   trx.operations.push_back( aco );
   processed_transaction result = db.push_transaction(trx, ~0);

   account_id_type paula_id = result.operation_results[0].get<object_id_type>();
   account_id_type penny_id = result.operation_results[1].get<object_id_type>();
   account_id_type petra_id = result.operation_results[2].get<object_id_type>();

   int64_t alice_ppy  = 0;
   int64_t ann_ppy    = 0;
   int64_t audrey_ppy = 0;
   int64_t alice_btc  = 0;
   int64_t ann_btc    = 0;
   int64_t audrey_btc = 0;

   {
      const tournament_object& game = db.create<tournament_object>( []( tournament_object& t ) {
         t.options.game_options = rock_paper_scissors_game_options();
         t.options.buy_in = asset( 10 );
      });
      affiliate_payout_helper helper = affiliate_payout_helper( db, game );
      // Alice has no distribution set
      BOOST_CHECK_EQUAL( 0, helper.payout( alice_id, 1000 ).value );
      // Paula has nothing for Bookie
      BOOST_CHECK_EQUAL( 0, helper.payout( paula_id, 1000 ).value );
      // 20% of 4 gets rounded down to 0
      BOOST_CHECK_EQUAL( 0, helper.payout( penny_id, 4 ).value );

      // 20% of 5 = 1 is paid to Audrey
      BOOST_CHECK_EQUAL( 1, helper.payout( penny_id, 5 ).value );
      audrey_ppy++;

      // 20% of 50 = 10: 2 to Alice, 3 to Ann, 5 to Audrey
      BOOST_CHECK_EQUAL( 10, helper.payout( penny_id, 50 ).value );
      alice_ppy += 2;
      ann_ppy += 3;
      audrey_ppy += 5;

      // 20% of 59 = 11: 1 to Ann, 10 to Audrey
      BOOST_CHECK_EQUAL( 11, helper.payout( petra_id, 59 ).value );
      audrey_ppy += 10;
      ann_ppy += 1;

      helper.commit();

      BOOST_CHECK_EQUAL( alice_ppy,  get_balance( alice_id, asset_id_type() ) );
      BOOST_CHECK_EQUAL( ann_ppy,    get_balance( ann_id, asset_id_type() ) );
      BOOST_CHECK_EQUAL( audrey_ppy, get_balance( audrey_id, asset_id_type() ) );
   }

   {
      const tournament_object& game = db.create<tournament_object>( [btc_id]( tournament_object& t ) {
         t.options.game_options = rock_paper_scissors_game_options();
         t.options.buy_in = asset( 10, btc_id );
      });
      affiliate_payout_helper helper = affiliate_payout_helper( db, game );
      // 20% of 60 = 12: 2 to Alice, 3 to Ann, 7 to Audrey
      BOOST_CHECK_EQUAL( 12, helper.payout( penny_id, 60 ).value );
      alice_btc += 2;
      ann_btc += 3;
      audrey_btc += 7;
      helper.commit();
      BOOST_CHECK_EQUAL( alice_btc,  get_balance( alice_id, btc_id ) );
      BOOST_CHECK_EQUAL( ann_btc,    get_balance( ann_id, btc_id ) );
      BOOST_CHECK_EQUAL( audrey_btc, get_balance( audrey_id, btc_id ) );
   }

   {
      const betting_market_group_object& game = db.create<betting_market_group_object>( []( betting_market_group_object& b ) {
         b.asset_id = asset_id_type();
      } );
      affiliate_payout_helper helper = affiliate_payout_helper( db, game );
      // Alice has no distribution set
      BOOST_CHECK_EQUAL( 0, helper.payout( alice_id, 1000 ).value );
      // Petra has nothing for Bookie
      BOOST_CHECK_EQUAL( 0, helper.payout( petra_id, 1000 ).value );
      // 20% of 4 gets rounded down to 0
      BOOST_CHECK_EQUAL( 0, helper.payout( penny_id, 4 ).value );

      // 20% of 5 = 1 is paid to Ann
      BOOST_CHECK_EQUAL( 1, helper.payout( penny_id, 5 ).value );
      ann_ppy++;

      // 20% of 40 = 8: 8 to Alice
      BOOST_CHECK_EQUAL( 8, helper.payout( paula_id, 40 ).value );
      alice_ppy += 8;

      // intermediate commit should clear internal accumulator
      helper.commit();

      // 20% of 59 = 11: 6 to Alice, 5 to Ann
      BOOST_CHECK_EQUAL( 11, helper.payout( penny_id, 59 ).value );
      alice_ppy += 6;
      ann_ppy += 5;

      helper.commit();

      BOOST_CHECK_EQUAL( alice_ppy,  get_balance( alice_id, asset_id_type() ) );
      BOOST_CHECK_EQUAL( ann_ppy,    get_balance( ann_id, asset_id_type() ) );
      BOOST_CHECK_EQUAL( audrey_ppy, get_balance( audrey_id, asset_id_type() ) );
   }

   {
      const betting_market_group_object& game = db.create<betting_market_group_object>( [btc_id]( betting_market_group_object& b ) {
         b.asset_id = btc_id;
      } );
      affiliate_payout_helper helper = affiliate_payout_helper( db, game );
      // 20% of 60 = 12: 7 to Alice, 5 to Ann
      BOOST_CHECK_EQUAL( 12, helper.payout( penny_id, 60 ).value );
      alice_btc += 7;
      ann_btc += 5;
      helper.commit();
      BOOST_CHECK_EQUAL( alice_btc,  get_balance( alice_id, btc_id ) );
      BOOST_CHECK_EQUAL( ann_btc,    get_balance( ann_id, btc_id ) );
      BOOST_CHECK_EQUAL( audrey_btc, get_balance( audrey_id, btc_id ) );
   }

   {
      // Fix total supply
      auto& index = db.get_index_type<account_balance_index>().indices().get<by_account_asset>();
      auto itr = index.find( boost::make_tuple( account_id_type(), asset_id_type() ) );
      BOOST_CHECK( itr != index.end() );
      db.modify( *itr, [alice_ppy,ann_ppy,audrey_ppy]( account_balance_object& bal ) {
         bal.balance -= alice_ppy + ann_ppy + audrey_ppy;
      });

      itr = index.find( boost::make_tuple( irene_id, btc_id ) );
      BOOST_CHECK( itr != index.end() );
      db.modify( *itr, [alice_btc,ann_btc,audrey_btc]( account_balance_object& bal ) {
         bal.balance -= alice_btc + ann_btc + audrey_btc;
      });
   }
}

BOOST_AUTO_TEST_CASE( rps_tournament_payout_test )
{ try {
   ACTORS( (alice)(ann)(audrey)(martha) );

   generate_blocks( HARDFORK_999_TIME );
   generate_block();

   test::set_expiration( db, trx );
   trx.clear();

   // Paula: 100% to Alice for Bookie / nothing for RPS
   const fc::ecc::private_key paula_private_key = generate_private_key( "paula" );
   account_create_operation aco = make_account( "paula", paula_private_key.get_public_key() );
   aco.extensions.value.affiliate_distributions = affiliate_reward_distributions();
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[alice_id] = GRAPHENE_100_PERCENT;
   trx.operations.push_back( aco );

   // Penny: For Bookie 60% to Alice + 40% to Ann / For RPS 20% to Alice, 30% to Ann, 50% to Audrey
   const fc::ecc::private_key penny_private_key = generate_private_key( "penny" );
   aco = make_account( "penny", penny_private_key.get_public_key() );
   aco.extensions.value.affiliate_distributions = affiliate_reward_distributions();
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[alice_id] = GRAPHENE_100_PERCENT * 3 / 5;
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[ann_id]   = GRAPHENE_100_PERCENT
                                                                                  - aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[alice_id];
   aco.extensions.value.affiliate_distributions->_dists[rps]._dist[alice_id]    = GRAPHENE_100_PERCENT / 5;
   aco.extensions.value.affiliate_distributions->_dists[rps]._dist[audrey_id]   = GRAPHENE_100_PERCENT / 2;
   aco.extensions.value.affiliate_distributions->_dists[rps]._dist[ann_id]      = GRAPHENE_100_PERCENT
                                                                                  - aco.extensions.value.affiliate_distributions->_dists[rps]._dist[alice_id]
                                                                                  - aco.extensions.value.affiliate_distributions->_dists[rps]._dist[audrey_id];
   trx.operations.push_back( aco );

   // Petra: nothing for Bookie / For RPS 10% to Ann, 90% to Audrey
   const fc::ecc::private_key petra_private_key = generate_private_key( "petra" );
   aco = make_account( "petra", petra_private_key.get_public_key() );
   aco.extensions.value.affiliate_distributions = affiliate_reward_distributions();
   aco.extensions.value.affiliate_distributions->_dists[rps]._dist[ann_id]    = GRAPHENE_100_PERCENT / 10;
   aco.extensions.value.affiliate_distributions->_dists[rps]._dist[audrey_id] = GRAPHENE_100_PERCENT
                                                                                - aco.extensions.value.affiliate_distributions->_dists[rps]._dist[ann_id];
   trx.operations.push_back( aco );
   processed_transaction result = db.push_transaction(trx, ~0);
   trx.clear();

   account_id_type paula_id = result.operation_results[0].get<object_id_type>();
   account_id_type penny_id = result.operation_results[1].get<object_id_type>();
   account_id_type petra_id = result.operation_results[2].get<object_id_type>();

   fund( martha_id(db), asset(1000000000) );
   fund( paula_id(db), asset(20000000) );
   fund( penny_id(db), asset(30000000) );
   fund( petra_id(db), asset(40000000) );

   upgrade_to_lifetime_member( martha );

   tournaments_helper helper(*this);
   account_id_type dividend_id = *helper.get_asset_dividend_account();
   int64_t div_ppy  = get_balance( dividend_id, asset_id_type() );
   const asset buy_in = asset(12000);
   tournament_id_type tournament_id = helper.create_tournament( martha_id, martha_private_key, buy_in, 3, 3, 1, 1 );
   BOOST_REQUIRE(tournament_id == tournament_id_type());

   helper.join_tournament( tournament_id, paula_id, paula_id, paula_private_key, buy_in);
   helper.join_tournament( tournament_id, penny_id, penny_id, penny_private_key, buy_in);
   helper.join_tournament( tournament_id, petra_id, petra_id, petra_private_key, buy_in);

   generate_block();
   const tournament_object& tournament = db.get<tournament_object>( tournament_id );

   BOOST_CHECK_EQUAL( 19988000, get_balance( paula_id, asset_id_type() ) );
   BOOST_CHECK_EQUAL( 29988000, get_balance( penny_id, asset_id_type() ) );
   BOOST_CHECK_EQUAL( 39988000, get_balance( petra_id, asset_id_type() ) );

   const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
   BOOST_CHECK_EQUAL( 3, tournament_details.matches.size() );

   { // 3 players, match 1 is a bye for penny
      const match_object& match = tournament_details.matches[0](db);
      BOOST_CHECK_EQUAL( int64_t(match_state::match_complete), int64_t(match.get_state()) );
      BOOST_CHECK_EQUAL( 1, match.players.size() );
      BOOST_CHECK_EQUAL( object_id_type(penny_id).instance(), object_id_type(match.players[0]).instance() );
   }
   { // match 2 is paula vs. petra: paula wins
      const match_object& match = tournament_details.matches[1](db);
      BOOST_CHECK_EQUAL( int64_t(match_state::match_in_progress), int64_t(match.get_state()) );
      BOOST_CHECK_EQUAL( 2, match.players.size() );
      BOOST_CHECK_EQUAL( object_id_type(paula_id).instance(), object_id_type(match.players[0]).instance() );
      BOOST_CHECK_EQUAL( object_id_type(petra_id).instance(), object_id_type(match.players[1]).instance() );
      BOOST_CHECK_EQUAL( 1, match.games.size() );

      const game_object& game = match.games[0](db);
      BOOST_CHECK_EQUAL( int64_t(game_state::expecting_commit_moves), int64_t(game.get_state()) );
      helper.rps_throw( game.id, paula_id, rock_paper_scissors_gesture::paper, paula_private_key );
      helper.rps_throw( game.id, petra_id, rock_paper_scissors_gesture::rock,  petra_private_key );

      BOOST_CHECK_EQUAL( int64_t(game_state::expecting_reveal_moves), int64_t(game.get_state()) );
      helper.rps_reveal( game.id, paula_id, rock_paper_scissors_gesture::paper, paula_private_key );
      helper.rps_reveal( game.id, petra_id, rock_paper_scissors_gesture::rock,  petra_private_key );

      BOOST_CHECK_EQUAL( int64_t(game_state::game_complete), int64_t(game.get_state()) );
      BOOST_CHECK_EQUAL( 1, match.games.size() );
      BOOST_CHECK_EQUAL( int64_t(match_state::match_complete), int64_t(match.get_state()) );
   }
   { // match 3 is penny vs. paula: penny wins
      const match_object& match = tournament_details.matches[2](db);
      BOOST_CHECK_EQUAL( int64_t(match_state::match_in_progress), int64_t(match.get_state()) );
      BOOST_CHECK_EQUAL( 2, match.players.size() );
      BOOST_CHECK_EQUAL( object_id_type(penny_id).instance(), object_id_type(match.players[0]).instance() );
      BOOST_CHECK_EQUAL( object_id_type(paula_id).instance(), object_id_type(match.players[1]).instance() );
      BOOST_CHECK_EQUAL( 1, match.games.size() );

      const game_object& game = match.games[0](db);
      BOOST_CHECK_EQUAL( int64_t(game_state::expecting_commit_moves), int64_t(game.get_state()) );
      helper.rps_throw( game.id, paula_id, rock_paper_scissors_gesture::paper,    paula_private_key );
      helper.rps_throw( game.id, penny_id, rock_paper_scissors_gesture::scissors, penny_private_key );

      BOOST_CHECK_EQUAL( int64_t(game_state::expecting_reveal_moves), int64_t(game.get_state()) );
      helper.rps_reveal( game.id, paula_id, rock_paper_scissors_gesture::paper,    paula_private_key );
      helper.rps_reveal( game.id, penny_id, rock_paper_scissors_gesture::scissors, penny_private_key );

      BOOST_CHECK_EQUAL( int64_t(game_state::game_complete), int64_t(game.get_state()) );
      BOOST_CHECK_EQUAL( int64_t(match_state::match_complete), int64_t(match.get_state()) );
   }

   BOOST_CHECK_EQUAL( 3*GRAPHENE_1_PERCENT, db.get_global_properties().parameters.rake_fee_percentage );
   // Penny wins net 3*buy_in minus rake = 36000 - 1080 = 34920
   BOOST_CHECK_EQUAL( 19988000, get_balance( paula_id, asset_id_type() ) );
   BOOST_CHECK_EQUAL( 29988000 + 34920, get_balance( penny_id, asset_id_type() ) );
   BOOST_CHECK_EQUAL( 39988000, get_balance( petra_id, asset_id_type() ) );

   // Dividend account receives 80% of rake = 864
   BOOST_CHECK_EQUAL( div_ppy + 864, get_balance( dividend_id, asset_id_type() ) );

   // 20% of rake = 216 is paid to Penny's affiliates: 43 to Alice, 64 to Ann, 109 to Audrey
   BOOST_CHECK_EQUAL( 43,  get_balance( alice_id,  asset_id_type() ) );
   BOOST_CHECK_EQUAL( 64,  get_balance( ann_id,    asset_id_type() ) );
   BOOST_CHECK_EQUAL( 109, get_balance( audrey_id, asset_id_type() ) );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
