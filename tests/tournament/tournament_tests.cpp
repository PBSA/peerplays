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

#include <graphene/chain/tournament_object.hpp>
#include <graphene/chain/match_object.hpp>
#include <graphene/chain/game_object.hpp>
#include "../common/database_fixture.hpp"
#include <graphene/utilities/tempdir.hpp>

using namespace graphene::chain;

// defined if "bye" matches fix available
#define BYE_MATCHES_FIXED

#define RAND_MAX_MIN(MAX, MIN) (std::rand() % ((MAX) - (MIN) + 1) + (MIN))

BOOST_AUTO_TEST_SUITE(tournament_tests)

// class performing operations necessary for creating tournaments,
// having players join the tournaments and playing tournaments to completion.
class tournaments_helper
{
public:

    tournaments_helper(database_fixture& df) : df(df)
    {
        assets.insert(asset_id_type());
        current_asset_idx = 0;
        optional<account_id_type> dividend_account = get_asset_dividend_account(asset_id_type());
        if (dividend_account.valid())
            players.insert(*dividend_account);
    }

    const std::set<tournament_id_type>& list_tournaments()
    {
        return tournaments;
    }

    std::map<account_id_type, std::map<asset_id_type, share_type>> list_players_balances()
    {
        std::map<account_id_type, std::map<asset_id_type, share_type>> result;
        for (account_id_type player_id: players)
        {
            for( asset_id_type asset_id: assets)
            {
                asset a = df.db.get_balance(player_id, asset_id);
                result[player_id][a.asset_id] = a.amount;
             }
        }
        return result;
    }

    std::map<account_id_type, std::map<asset_id_type, share_type>> get_players_fees()
    {
        return players_fees;
    }

    void reset_players_fees()
    {
        for (account_id_type player_id: players)
        {
            for( asset_id_type asset_id: assets)
            {
                players_fees[player_id][asset_id] = 0;
            }
        }
    }

    void create_asset(const account_id_type& issuer_account_id,
                      const string& symbol,
                      uint8_t precision,
                      asset_options& common,
                      const fc::ecc::private_key& sig_priv_key)
    {
        graphene::chain::database& db = df.db;
        const chain_parameters& params = db.get_global_properties().parameters;
        signed_transaction tx;
        asset_create_operation op;
        op.issuer = issuer_account_id;
        op.symbol = symbol;
        op.precision = precision;
        op.common_options = common;

        tx.operations = {op};
        for( auto& op : tx.operations )
            db.current_fee_schedule().set_fee(op);
        tx.validate();
        tx.set_expiration(db.head_block_time() + fc::seconds( params.block_interval * (params.maintenance_skip_slots + 1) * 3));
        df.sign(tx, sig_priv_key);
        PUSH_TX(db, tx);

        assets.insert(asset_id_type(++current_asset_idx));
    }

    void update_dividend_asset(const asset_id_type asset_to_update_id,
                               dividend_asset_options new_options,
                               const fc::ecc::private_key& sig_priv_key)
    {
        graphene::chain::database& db = df.db;
        const chain_parameters& params = db.get_global_properties().parameters;
        signed_transaction tx;
        asset_update_dividend_operation update_op;

        update_op.issuer = asset_to_update_id(db).issuer;
        update_op.asset_to_update = asset_to_update_id;
        update_op.new_options = new_options;

        tx.operations = {update_op};
        for( auto& op : tx.operations )
           db.current_fee_schedule().set_fee(op);
        tx.validate();
        tx.set_expiration(db.head_block_time() + fc::seconds( params.block_interval * (params.maintenance_skip_slots + 1) * 3));
        df.sign(tx, sig_priv_key);
        PUSH_TX(db, tx);

        optional<account_id_type> dividend_account = get_asset_dividend_account(asset_to_update_id);
        if (dividend_account.valid())
          players.insert(*dividend_account);
    }

    optional<account_id_type> get_asset_dividend_account(const asset_id_type& asset_id)
    {
        graphene::chain::database& db = df.db;
        optional<account_id_type> result;
        const asset_object& asset_obj = asset_id(db);

        if (asset_obj.dividend_data_id.valid())
        {
           const asset_dividend_data_object& dividend_data = (*asset_obj.dividend_data_id)(db);
           result = dividend_data.dividend_distribution_account;
        }
        return result;
    }

    const tournament_id_type create_tournament (const account_id_type& creator,
                                                const fc::ecc::private_key& sig_priv_key,
                                                asset buy_in,
                                                uint32_t number_of_players = 2,
                                                uint32_t time_per_commit_move = 3,
                                                uint32_t time_per_reveal_move = 1,
                                                uint32_t number_of_wins = 3,
                                                uint32_t registration_deadline = 3600,
                                                uint32_t start_delay = 3,
                                                uint32_t round_delay = 3,
                                                bool insurance_enabled = false,
                                                fc::optional<flat_set<account_id_type> > whitelist = fc::optional<flat_set<account_id_type> >()
                                                )
    {
        if (current_tournament_idx.valid())
            current_tournament_idx = *current_tournament_idx + 1;
        else
            current_tournament_idx = 0;

        graphene::chain::database& db = df.db;
        const chain_parameters& params = db.get_global_properties().parameters;
        signed_transaction trx;
        tournament_options options;
        tournament_create_operation op;
        rock_paper_scissors_game_options& game_options = options.game_options.get<rock_paper_scissors_game_options>();

        game_options.number_of_gestures = 3;
        game_options.time_per_commit_move = time_per_commit_move;
        game_options.time_per_reveal_move = time_per_reveal_move;
        game_options.insurance_enabled = insurance_enabled;

        options.registration_deadline = db.head_block_time() + fc::seconds(registration_deadline + *current_tournament_idx);
        options.buy_in = buy_in;
        options.number_of_players = number_of_players;
        options.start_delay = start_delay;
        options.round_delay = round_delay;
        options.number_of_wins = number_of_wins;
        if (whitelist.valid())
            options.whitelist = *whitelist;

        op.creator = creator;
        op.options = options;
        trx.operations = {op};
        for( auto& op : trx.operations )
            db.current_fee_schedule().set_fee(op);
        trx.validate();
        trx.set_expiration(db.head_block_time() + fc::seconds( params.block_interval * (params.maintenance_skip_slots + 1) * 3));
        df.sign(trx, sig_priv_key);
        PUSH_TX(db, trx);

        tournament_id_type tournament_id = tournament_id_type(*current_tournament_idx);
        tournaments.insert(tournament_id);
        return tournament_id;
    }

    void join_tournament(const tournament_id_type & tournament_id,
                         const account_id_type& player_id,
                         const account_id_type& payer_id,
                         const fc::ecc::private_key& sig_priv_key,
                         asset buy_in
                         )
    {
        graphene::chain::database& db = df.db;
        const chain_parameters& params = db.get_global_properties().parameters;
        signed_transaction tx;
        tournament_join_operation op;

        op.payer_account_id = payer_id;
        op.buy_in = buy_in;
        op.player_account_id = player_id;
        op.tournament_id = tournament_id;
        tx.operations = {op};
        for( auto& op : tx.operations )
            db.current_fee_schedule().set_fee(op);
        tx.validate();
        tx.set_expiration(db.head_block_time() + fc::seconds( params.block_interval * (params.maintenance_skip_slots + 1) * 3));
        df.sign(tx, sig_priv_key);
        PUSH_TX(db, tx);

        players.insert(player_id);
        players_keys[player_id] = sig_priv_key;
   }

   void leave_tournament(const tournament_id_type & tournament_id,
                         const account_id_type& player_id,
                         const account_id_type& canceling_account_id,
                         const fc::ecc::private_key& sig_priv_key
                        )
   {
       graphene::chain::database& db = df.db;
       const chain_parameters& params = db.get_global_properties().parameters;
       signed_transaction tx;
       tournament_leave_operation op;

       op.canceling_account_id = canceling_account_id;
       op.player_account_id = player_id;
       op.tournament_id = tournament_id;
       tx.operations = {op};
       for( auto& op : tx.operations )
           db.current_fee_schedule().set_fee(op);
       tx.validate();
       tx.set_expiration(db.head_block_time() + fc::seconds( params.block_interval * (params.maintenance_skip_slots + 1) * 3));
       df.sign(tx, sig_priv_key);
       PUSH_TX(db, tx);

       //players.erase(player_id);
   }



    // stolen from cli_wallet
    void rps_throw(const game_id_type& game_id,
                   const account_id_type& player_id,
                   rock_paper_scissors_gesture gesture,
                   const fc::ecc::private_key& sig_priv_key
                   )
    {

       graphene::chain::database& db = df.db;
       const chain_parameters& params = db.get_global_properties().parameters;

       // check whether the gesture is appropriate for the game we're playing
       game_object game_obj = game_id(db);
       match_object match_obj = game_obj.match_id(db);
       tournament_object tournament_obj = match_obj.tournament_id(db);
       rock_paper_scissors_game_options game_options = tournament_obj.options.game_options.get<rock_paper_scissors_game_options>();
       assert((int)gesture < game_options.number_of_gestures);

       account_object player_account_obj = player_id(db);

       // construct the complete throw, the commit, and reveal
       rock_paper_scissors_throw full_throw;
       rand_bytes((char*)&full_throw.nonce1, sizeof(full_throw.nonce1));
       rand_bytes((char*)&full_throw.nonce2, sizeof(full_throw.nonce2));
       full_throw.gesture = gesture;

       rock_paper_scissors_throw_commit commit_throw;
       commit_throw.nonce1 = full_throw.nonce1;
       std::vector<char> full_throw_packed(fc::raw::pack(full_throw));
       commit_throw.throw_hash = fc::sha256::hash(full_throw_packed.data(), full_throw_packed.size());

       rock_paper_scissors_throw_reveal reveal_throw;
       reveal_throw.nonce2 = full_throw.nonce2;
       reveal_throw.gesture = full_throw.gesture;

       // store off the reveal for applying after both players commit
       committed_game_moves[commit_throw] = reveal_throw;

       signed_transaction tx;
       game_move_operation move_operation;
       move_operation.game_id = game_id;
       move_operation.player_account_id = player_account_obj.id;
       move_operation.move = commit_throw;
       tx.operations = {move_operation};
       for( operation& op : tx.operations )
       {
           asset f = db.current_fee_schedule().set_fee(op);
           players_fees[player_id][f.asset_id] -= f.amount;
       }
       tx.validate();
       tx.set_expiration(db.head_block_time() + fc::seconds( params.block_interval * (params.maintenance_skip_slots + 1) * 3));
       df.sign(tx, sig_priv_key);
       if (/*match_obj.match_winners.empty() &&*/ game_obj.get_state() == game_state::expecting_commit_moves) // checking again
          PUSH_TX(db, tx);
    }

    // spaghetti programming
    // walking through all tournaments, matches and games and throwing random moves
    // optionaly skip generting randomly selected moves
    // every_move_is >= 0 : every game is tie
    void play_games(unsigned skip_some_commits = 0, unsigned skip_some_reveals = 0, int every_move_is = -1)
    {
      //try
      //{
        graphene::chain::database& db = df.db;
        const chain_parameters& params = db.get_global_properties().parameters;

        for(const auto& tournament_id: tournaments)
        {
            const tournament_object& tournament = tournament_id(db);
            const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
            rock_paper_scissors_game_options game_options = tournament.options.game_options.get<rock_paper_scissors_game_options>();
            for(const auto& match_id: tournament_details.matches)
            {
                const match_object& match = match_id(db);
                for(const auto& game_id: match.games )
                {
                    const game_object& game = game_id(db);
                    const rock_paper_scissors_game_details& rps_details = game.game_details.get<rock_paper_scissors_game_details>();
                    if (game.get_state() == game_state::expecting_commit_moves)
                    {
                        for(const auto& player_id: game.players)
                        {
                            if ( players_keys.find(player_id) != players_keys.end())
                            {
                                if (!skip_some_commits || player_id.instance.value % skip_some_commits != game_id.instance.value % skip_some_commits)
                                {
                                    auto iter = std::find(game.players.begin(), game.players.end(), player_id);
                                    unsigned player_index = std::distance(game.players.begin(), iter);
                                    if (!rps_details.commit_moves.at(player_index))
                                        rps_throw(game_id, player_id,
                                                  (rock_paper_scissors_gesture) (every_move_is >= 0 ? every_move_is : (std::rand() % game_options.number_of_gestures)), players_keys[player_id]);
                                }
                            }
                        }
                    }
                    else if (game.get_state() == game_state::expecting_reveal_moves)
                    {
                        for (unsigned i = 0; i < 2; ++i)
                        {
                           if (rps_details.commit_moves.at(i) &&
                               !rps_details.reveal_moves.at(i))
                           {
                              const account_id_type& player_id = game.players[i];
                              if (players_keys.find(player_id) != players_keys.end())
                              {
                                 {

                                    auto iter = committed_game_moves.find(*rps_details.commit_moves.at(i));
                                    if (iter != committed_game_moves.end())
                                    {
                                       if (!skip_some_reveals || player_id.instance.value % skip_some_reveals != game_id.instance.value % skip_some_reveals)
                                       {
                                           const rock_paper_scissors_throw_reveal& reveal = iter->second;

                                           game_move_operation move_operation;
                                           move_operation.game_id = game.id;
                                           move_operation.player_account_id = player_id;
                                           move_operation.move = reveal;

                                           signed_transaction tx;
                                           tx.operations = {move_operation};
                                           for( auto& op : tx.operations )
                                           {
                                               asset f = db.current_fee_schedule().set_fee(op);
                                               players_fees[player_id][f.asset_id] -= f.amount;
                                           }
                                           tx.validate();
                                           tx.set_expiration(db.head_block_time() + fc::seconds( params.block_interval * (params.maintenance_skip_slots + 1) * 3));
                                           df.sign(tx, players_keys[player_id]);
                                           if (game.get_state() == game_state::expecting_reveal_moves) // check again
                                               PUSH_TX(db, tx);
                                       }
                                    }
                                 }
                              }
                           }
                        }
                    }
                }
            }
        }
      //}
      //catch (fc::exception& e)
      //{
      //    edump((e.to_detail_string()));
      //    throw;
      //}
    }

private:
    database_fixture& df;
    // index of last created tournament
    fc::optional<uint64_t> current_tournament_idx;
    // index of last asset
    uint64_t current_asset_idx;
    // assets : core and maybe others
    std::set<asset_id_type> assets;
    // tournaments to be played
    std::set<tournament_id_type> tournaments;
    // all players registered in tournaments
    std::set<account_id_type> players;
    // players' private keys
    std::map<account_id_type, fc::ecc::private_key> players_keys;
    // total charges for moves made by every player
    std::map<account_id_type, std::map<asset_id_type, share_type>> players_fees;
    // store of commits and reveals
    std::map<rock_paper_scissors_throw_commit, rock_paper_scissors_throw_reveal> committed_game_moves;

    // taken from rand.cpp
    void rand_bytes(char* buf, int count)
    {
      fc::init_openssl();

      int result = RAND_bytes((unsigned char*)buf, count);
      if (result != 1)
        FC_THROW("Error calling OpenSSL's RAND_bytes(): ${code}", ("code", (uint32_t)ERR_get_error()));
    }
};


/// Expecting failure

//  creating tournament

//  joining tournament

BOOST_FIXTURE_TEST_CASE( can_only_join, database_fixture )
{
    try
    {       std::string reason("Can only join a tournament during registration period");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            //transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 3, 1, 3, 1);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            //tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            sleep(2);
            generate_block();
            try
            {
                tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

#if 0
// it is unlikely that tournament_join_evaluator will reach the assertion
BOOST_FIXTURE_TEST_CASE( tournament_is_already, database_fixture )
{
    try
    {       std::string reason("Tournament is already full");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob)(carol));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));
            transfer(committee_account, carol_id,    asset(4000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
            try
            {
                tournament_helper.join_tournament(tournament_id, carol_id, carol_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("carol"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}
#endif

#if 0
// it is unlikely that tournament_join_evaluator will reach the assertion
BOOST_FIXTURE_TEST_CASE( registration_deadline_has_already_passed, database_fixture )
{
    try
    {       std::string reason("Registration deadline has already passed");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 3, 1, 3, 2);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            //tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            sleep(3);
            generate_block();
            try
            {
                tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}
#endif

BOOST_FIXTURE_TEST_CASE( player_is_not_on_the_whitelist, database_fixture )
{
    try
    {       std::string reason("Player is not on the whitelist for this tournament");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob)(carol));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));
            transfer(committee_account, carol_id,  asset(4000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            flat_set<account_id_type> whitelist{ alice_id, carol_id };
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2,
                                                                 30, 30, 3, 60, 3, 3, true, whitelist);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            try
            {
                tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( player_is_already_registered, database_fixture )
{
    try
    {       std::string reason("Player is already registered for this tournament");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 3);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
            try
            {
                tournament_helper.join_tournament(tournament_id, alice_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( buy_in_incorrect, database_fixture )
{
    try
    {       std::string reason("Buy-in is incorrect");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            asset buy_in_1 = asset(10001);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 3);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            try
            {
                tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in_1);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

//issue_uia(nathan_id, asset(GRAPHENE_MAX_SHARE_SUPPLY/2, asset_id_type(1)));
//asset_object ao = asset_id_type(1)(db);
//ao.options.flags |= transfer_restricted;
// "Asset {asset} has transfer_restricted flag enabled"

// "player account ${player} is not whitelisted for asset ${asset}",

// "payer account ${payer} is not whitelisted for asset ${asset}",

//  leaving tournament_

BOOST_FIXTURE_TEST_CASE( player_is_not_registered, database_fixture )
{
    try
    {       std::string reason("Player is not registered for this tournament");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            try
            {
                tournament_helper.leave_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))));
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( only_player_or_payer, database_fixture )
{
    try
    {       std::string reason("Only player or payer can unregister the player from a tournament");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob)(carol));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));
            transfer(committee_account, carol_id,  asset(4000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            // player may unregister
            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.leave_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))));

            // payer may unregister
            tournament_helper.join_tournament(tournament_id, alice_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
            tournament_helper.leave_tournament(tournament_id, alice_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))));

            // no one else
            tournament_helper.join_tournament(tournament_id, bob_id, carol_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("carol"))), buy_in);
            try
            {
                tournament_helper.leave_tournament(tournament_id, bob_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))));
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( can_only_leave, database_fixture )
{
    try
    {       std::string reason("Can only leave a tournament during registration period");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
            try
            {
                tournament_helper.leave_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))));
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

/// Expecting success

// Test of basic functionality creating two tournamenst, joinig players,
// playing tournaments to completion, distributing prize.
// Testing of "bye" matches handling can be performed if "bye" matches fix is available.
// Numbers of players 2+1 4+1 8+1 ... seem to be most critical for handling of "bye" matches.
// Moves are generated automatically.
BOOST_FIXTURE_TEST_CASE( simple, database_fixture )
{
    try
    {
#ifdef BYE_MATCHES_FIXED
    #define TEST1_NR_OF_PLAYERS_NUMBER 3
    #define TEST2_NR_OF_PLAYERS_NUMBER 5
#else
     #define TEST1_NR_OF_PLAYERS_NUMBER 2
     #define TEST2_NR_OF_PLAYERS_NUMBER 4
#endif
            BOOST_TEST_MESSAGE("Hello simple tournament test");
            ACTORS((nathan)(alice)(bob)(carol)(dave)(ed)(frank)(george)(harry)(ike)(romek));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            BOOST_TEST_MESSAGE( "Giving folks some money" );
            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, romek_id,  asset(2000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));
            transfer(committee_account, carol_id,  asset(4000000));
            transfer(committee_account, dave_id,   asset(5000000));
#if TEST2_NR_OF_PLAYERS_NUMBER > 4
            transfer(committee_account, ed_id,     asset(6000000));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 5
            transfer(committee_account, frank_id,  asset(7000000));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 6
            transfer(committee_account, george_id, asset(8000000));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 7
            transfer(committee_account, harry_id,  asset(9000000));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 8
            transfer(committee_account, ike_id,  asset(1000000));
#endif

            BOOST_TEST_MESSAGE( "Preparing nathan" );
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            uint16_t tournaments_to_complete = 0;
            asset buy_in = asset(12000);
            tournament_id_type tournament_id;

            BOOST_TEST_MESSAGE( "Preparing a tournament, insurance disabled" );
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, TEST1_NR_OF_PLAYERS_NUMBER);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
#if TEST1_NR_OF_PLAYERS_NUMBER > 2
            tournament_helper.join_tournament(tournament_id, carol_id, carol_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("carol"))), buy_in);
#endif
            ++tournaments_to_complete;

            BOOST_TEST_MESSAGE( "Preparing another one, insurance enabled" );
            buy_in = asset(13000);
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, TEST2_NR_OF_PLAYERS_NUMBER,
                                                                 3, 1, 3, 3600, 3, 3, true);
            BOOST_REQUIRE(tournament_id == tournament_id_type(1));
            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            // romek joins but will leave
            tournament_helper.join_tournament(tournament_id, romek_id, romek_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("romek"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
            tournament_helper.join_tournament(tournament_id, carol_id, carol_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("carol"))), buy_in);
            // romek leaves
            tournament_helper.leave_tournament(tournament_id, romek_id, romek_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("romek"))));
            tournament_helper.join_tournament(tournament_id, dave_id, dave_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("dave"))), buy_in);
#if TEST2_NR_OF_PLAYERS_NUMBER > 4
            tournament_helper.join_tournament(tournament_id, ed_id, ed_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("ed"))), buy_in);
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 5
            tournament_helper.join_tournament(tournament_id, frank_id, frank_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("frank"))), buy_in);
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 6
            tournament_helper.join_tournament(tournament_id, george_id, george_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("george"))), buy_in);
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 7
            tournament_helper.join_tournament(tournament_id, harry_id, harry_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("harry"))), buy_in);
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 8
            tournament_helper.join_tournament(tournament_id, ike_id, ike_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("ike"))), buy_in);
#endif
            ++tournaments_to_complete;

            auto abc = [&] (string s)
            {
#if 0
                wlog(s);
                auto a = db.get_balance(alice_id, asset_id_type()); wdump(("# alice's balance") (a));
                auto b = db.get_balance(bob_id, asset_id_type()); wdump(("# bob's balance") (b));
                auto c = db.get_balance(carol_id, asset_id_type()); wdump(("# carol's balance") (c));
                auto d = db.get_balance(dave_id, asset_id_type()); wdump(("# dave's balance") (d));
#if TEST2_NR_OF_PLAYERS_NUMBER > 4
                auto e = db.get_balance(ed_id, asset_id_type()); wdump(("# ed's balance") (e));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 5
                auto f = db.get_balance(frank_id, asset_id_type()); wdump(("# frank's balance") (f));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 6
                auto g = db.get_balance(george_id, asset_id_type()); wdump(("# george's balance") (g));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 7
                auto h = db.get_balance(harry_id, asset_id_type()); wdump(("# harry's balance") (f));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 8
                auto i = db.get_balance(ike_id, asset_id_type()); wdump(("# ike's balance") (i));
#endif

                auto n = db.get_balance(nathan_id, asset_id_type()); wdump(("# nathan's balance") (n));
                auto r = db.get_balance(TOURNAMENT_RAKE_FEE_ACCOUNT_ID, asset_id_type()); wdump(("# rake's balance") (r));
#endif
            };


            abc("@ tournament awaiting start");
            BOOST_TEST_MESSAGE( "Generating blocks, waiting for tournaments' completion");
            generate_block();
            abc("@ after first generated block");
            std::set<tournament_id_type> tournaments = tournament_helper.list_tournaments();
            std::map<account_id_type, std::map<asset_id_type, share_type>> players_balances = tournament_helper.list_players_balances();
            uint16_t rake_fee_percentage = db.get_global_properties().parameters.rake_fee_percentage;

            while(tournaments_to_complete > 0)
            {
                for(const auto& tournament_id: tournaments)
                {
                    const tournament_object& tournament = tournament_id(db);
                    if (tournament.get_state() == tournament_state::concluded) {
                        const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
                        const match_object& final_match = (tournament_details.matches[tournament_details.matches.size() - 1])(db);

                        assert(final_match.match_winners.size() == 1);
                        const account_id_type& winner_id = *final_match.match_winners.begin();

                        const account_object winner = winner_id(db);
                        BOOST_TEST_MESSAGE( "The winner is " + winner.name );

                        share_type rake_amount = (fc::uint128_t(tournament.prize_pool.value) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100).to_uint64();
                        optional<account_id_type> dividend_account = tournament_helper.get_asset_dividend_account(tournament.options.buy_in.asset_id);
                        if (dividend_account.valid())
                            players_balances[*dividend_account][tournament.options.buy_in.asset_id] += rake_amount;
                        players_balances[winner_id][tournament.options.buy_in.asset_id] += tournament.prize_pool - rake_amount;

                        tournaments.erase(tournament_id);
                        --tournaments_to_complete;
                        break;
                    }
                }
                generate_block();
                sleep(1);
            }

            abc("@ tournament concluded");
            // checking if prizes were distributed correctly
            BOOST_CHECK(tournaments.size() == 0);
            std::map<account_id_type, std::map<asset_id_type, share_type>> last_players_balances = tournament_helper.list_players_balances();
            for (auto a: last_players_balances)
            {
                BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s balance " + std::to_string((uint64_t)(a.second[asset_id_type()].value)) );
                BOOST_CHECK(a.second[asset_id_type()] == players_balances[a.first][asset_id_type()]);
            }
            BOOST_TEST_MESSAGE("Bye simple tournament test\n");

    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}


// Test of handling ties, creating two tournamenst, joinig players,
// All generated moves are identical.
BOOST_FIXTURE_TEST_CASE( ties, database_fixture )
{
    try
    {
#ifdef BYE_MATCHES_FIXED
    #define TEST1_NR_OF_PLAYERS_NUMBER 3
    #define TEST2_NR_OF_PLAYERS_NUMBER 5
#else
     #define TEST1_NR_OF_PLAYERS_NUMBER 2
     #define TEST2_NR_OF_PLAYERS_NUMBER 4
#endif
            BOOST_TEST_MESSAGE("Hello ties tournament test");
            ACTORS((nathan)(alice)(bob)(carol)(dave)(ed)(frank)(george)(harry)(ike));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            BOOST_TEST_MESSAGE( "Giving folks some money" );
            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));
            transfer(committee_account, carol_id,  asset(4000000));
            transfer(committee_account, dave_id,   asset(5000000));
            transfer(committee_account, ed_id,     asset(6000000));

            BOOST_TEST_MESSAGE( "Preparing nathan" );
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            uint16_t tournaments_to_complete = 0;
            asset buy_in = asset(12000);
            tournament_id_type tournament_id;

            BOOST_TEST_MESSAGE( "Preparing a tournament, insurance disabled" );
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, TEST1_NR_OF_PLAYERS_NUMBER, 30, 30);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
#if TEST1_NR_OF_PLAYERS_NUMBER > 2
            tournament_helper.join_tournament(tournament_id, carol_id, carol_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("carol"))), buy_in);
#endif
            ++tournaments_to_complete;

            BOOST_TEST_MESSAGE( "Preparing another one, insurance enabled" );
            buy_in = asset(13000);
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, TEST2_NR_OF_PLAYERS_NUMBER,
                                                                 30, 30, 3, 3600, 3, 3, true);
            BOOST_REQUIRE(tournament_id == tournament_id_type(1));
            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
            tournament_helper.join_tournament(tournament_id, carol_id, carol_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("carol"))), buy_in);
            tournament_helper.join_tournament(tournament_id, dave_id, dave_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("dave"))), buy_in);
#if TEST2_NR_OF_PLAYERS_NUMBER > 4
            tournament_helper.join_tournament(tournament_id, ed_id, ed_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("ed"))), buy_in);
#endif
            ++tournaments_to_complete;


            std::set<tournament_id_type> tournaments = tournament_helper.list_tournaments();
            std::map<account_id_type, std::map<asset_id_type, share_type>> players_balances = tournament_helper.list_players_balances();
            uint16_t rake_fee_percentage = db.get_global_properties().parameters.rake_fee_percentage;


            BOOST_TEST_MESSAGE( "Generating blocks, waiting for tournaments' completion");
            while(tournaments_to_complete > 0)
            {
                generate_block();
                //tournament_helper.play_games(3, 4, 1);
                tournament_helper.play_games(0, 0, 1);
                for(const auto& tournament_id: tournaments)
                {
                    const tournament_object& tournament = tournament_id(db);
                    if (tournament.get_state() == tournament_state::concluded) {
                        const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
                        const match_object& final_match = (tournament_details.matches[tournament_details.matches.size() - 1])(db);

                        assert(final_match.match_winners.size() == 1);
                        const account_id_type& winner_id = *final_match.match_winners.begin();
                        BOOST_TEST_MESSAGE( "The winner of " + std::string(object_id_type(tournament_id))  + " is " + winner_id(db).name + " " +  std::string(object_id_type(winner_id)));
                        share_type rake_amount = (fc::uint128_t(tournament.prize_pool.value) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100).to_uint64();
                        optional<account_id_type> dividend_account = tournament_helper.get_asset_dividend_account(tournament.options.buy_in.asset_id);
                        if (dividend_account.valid())
                            players_balances[*dividend_account][tournament.options.buy_in.asset_id] += rake_amount;                    players_balances[winner_id][tournament.options.buy_in.asset_id] += tournament.prize_pool - rake_amount;

                        tournaments.erase(tournament_id);
                        --tournaments_to_complete;
                        break;
                    }
                }
                sleep(1);
            }

            // checking if prizes were distributed correctly
            BOOST_CHECK(tournaments.size() == 0);
            std::map<account_id_type, std::map<asset_id_type, share_type>> last_players_balances = tournament_helper.list_players_balances();
            for (auto a: last_players_balances)
            {
                BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s balance " + std::to_string((uint64_t)(a.second[asset_id_type()].value)) );
                BOOST_CHECK(a.second[asset_id_type()] == players_balances[a.first][asset_id_type()]);
            }
            BOOST_TEST_MESSAGE("Bye ties tournament test\n");

    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

// Test of canceled tournament
// Checking buyin refund.
BOOST_FIXTURE_TEST_CASE( canceled, database_fixture )
{
    try
    {
        BOOST_TEST_MESSAGE("Hello canceled tournament test");
        ACTORS((nathan)(alice)(bob));

        tournaments_helper tournament_helper(*this);
        fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

        BOOST_TEST_MESSAGE( "Giving folks some money" );
        transfer(committee_account, nathan_id, asset(1000000000));
        transfer(committee_account, alice_id,  asset(2000000));
        transfer(committee_account, bob_id,    asset(3000000));

        BOOST_TEST_MESSAGE( "Preparing nathan" );
        upgrade_to_lifetime_member(nathan);

        uint16_t tournaments_to_complete = 0;
        asset buy_in = asset(12340);
        tournament_id_type tournament_id;

        BOOST_TEST_MESSAGE( "Preparing a tournament" );
        tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 3, 30, 30, 3, 5);
        BOOST_REQUIRE(tournament_id == tournament_id_type());

        tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
        tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
        ++tournaments_to_complete;
        BOOST_TEST_MESSAGE( "Generating blocks, waiting for tournament's completion");
        std::set<tournament_id_type> tournaments = tournament_helper.list_tournaments();
        std::map<account_id_type, std::map<asset_id_type, share_type>> players_balances = tournament_helper.list_players_balances();

        while(tournaments_to_complete > 0)
        {
            for(const auto& tournament_id: tournaments)
            {
                const tournament_object& tournament = tournament_id(db);
                BOOST_REQUIRE(tournament.get_state() != tournament_state::concluded);

                if (tournament.get_state() == tournament_state::registration_period_expired) {


                    const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
                    for(auto payer : tournament_details.payers)
                    {
                        players_balances[payer.first][tournament.options.buy_in.asset_id] += payer.second;
                    }

                    tournaments.erase(tournament_id);
                    --tournaments_to_complete;
                    break;
                }
            }
            generate_block();
            sleep(1);
        }

        // checking if buyins were refunded correctly
        BOOST_CHECK(tournaments.size() == 0);
        std::map<account_id_type, std::map<asset_id_type, share_type>> last_players_balances = tournament_helper.list_players_balances();
        for (auto a: last_players_balances)
        {
            BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s balance " + std::to_string((uint64_t)(a.second[asset_id_type()].value)) );
            BOOST_CHECK(a.second[asset_id_type()] == players_balances[a.first][asset_id_type()]);
        }
        BOOST_TEST_MESSAGE("Bye canceled tournament test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

// Test of few concurrently played tournaments having the same constant number of players.
// Tournament/s having even number use the core asset.
// Tournament/s having odd number use another asset.
// Moves are generated randomly.
// Checking prizes distribution for both assets is performed.
BOOST_FIXTURE_TEST_CASE( assets, database_fixture )
{
    try
    {
        #define PLAYERS_NUMBER 8
        #define TOURNAMENTS_NUMBER 3
        #define DEF_SYMBOL "NEXT"

        BOOST_TEST_MESSAGE("Hello two assets tournament test");

        ACTORS((nathan));
        fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
        transfer(committee_account, nathan_id, asset(1000000000));
        upgrade_to_lifetime_member(nathan);
        BOOST_CHECK(nathan.is_lifetime_member());

        tournaments_helper tournament_helper(*this);
        // creating new asset
        asset_options aoptions;
        aoptions.max_market_fee = aoptions.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
        aoptions.flags = 0;
        aoptions.issuer_permissions = 79;
        aoptions.core_exchange_rate.base.amount = 1;
        aoptions.core_exchange_rate.base.asset_id = asset_id_type(0);
        aoptions.core_exchange_rate.quote.amount = 1;
        aoptions.core_exchange_rate.quote.asset_id = asset_id_type(1);
        tournament_helper.create_asset(nathan_id, DEF_SYMBOL, 5, aoptions, nathan_priv_key);

        issue_uia(nathan_id, asset(GRAPHENE_MAX_SHARE_SUPPLY/2, asset_id_type(1)));

        dividend_asset_options doptions;
        doptions.minimum_distribution_interval = 3*24*60*60;
        doptions.minimum_fee_percentage = 10*GRAPHENE_1_PERCENT;
        doptions.next_payout_time = db.head_block_time() + fc::hours(1);
        doptions.payout_interval = 7*24*60*60;
        tournament_helper.update_dividend_asset(asset_id_type(1), doptions, nathan_priv_key);

#if 0
        auto tas = get_asset(GRAPHENE_SYMBOL); wdump((tas));
        auto das = get_asset(DEF_SYMBOL); wdump((das));

        if (das.dividend_data_id.valid())
        {
            auto div = (*das.dividend_data_id)(db); wdump((div));
            auto dda = div.dividend_distribution_account(db); wdump((dda));
        }

        auto nac = nathan_id(db); wdump(("# nathan's account") (nac));

        auto nab0 = db.get_balance(nathan_id, asset_id_type(0)); wdump(("# nathans's balance 0") (nab0));
        auto nab1 = db.get_balance(nathan_id, asset_id_type(1)); wdump(("# nathans's balance 1") (nab1));
#endif

        // creating actors
        std::vector<std::tuple<std::string, account_id_type, fc::ecc::private_key>> actors;
        for(unsigned i = 0; i < PLAYERS_NUMBER; ++i)
        {
            std::string name = "account" + std::to_string(i);
            auto priv_key = generate_private_key(name);
            const auto& account = create_account(name, priv_key.get_public_key());
            actors.emplace_back(name, account.id, priv_key);
            transfer(committee_account, account.id,  asset((uint64_t)100000000 * PLAYERS_NUMBER + 10000000 * (i+1)));
            transfer(nathan_id, account.id, asset((uint64_t)200000000 * PLAYERS_NUMBER + 20000000 * (i+1), asset_id_type(1)));
        }

        // creating tournaments, registering players
        for(unsigned i = 0; i < TOURNAMENTS_NUMBER; ++i)
        {
            asset buy_in = asset(1000 * PLAYERS_NUMBER + 100 * i, asset_id_type(i%2));
            tournament_id_type tournament_id;
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, PLAYERS_NUMBER, 30, 30);

            for (unsigned j = 0; j < PLAYERS_NUMBER; ++j)
            {
                auto a = actors[j];
                tournament_helper.join_tournament(tournament_id, std::get<1>(a), std::get<1>(a), std::get<2>(a), buy_in);
            }
        }

        uint16_t tournaments_to_complete = TOURNAMENTS_NUMBER;
        std::set<tournament_id_type> tournaments = tournament_helper.list_tournaments();
        std::map<account_id_type, std::map<asset_id_type, share_type>> players_balances = tournament_helper.list_players_balances();
        uint16_t rake_fee_percentage = db.get_global_properties().parameters.rake_fee_percentage;

        BOOST_TEST_MESSAGE( "Generating blocks, waiting for tournaments' completion");
        while(tournaments_to_complete > 0)
        {
            generate_block();
            tournament_helper.play_games(3, 4);
            for(const auto& tournament_id: tournaments)
            {
                const tournament_object& tournament = tournament_id(db);
                if (tournament.get_state() == tournament_state::concluded) {
                    const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
                    const match_object& final_match = (tournament_details.matches[tournament_details.matches.size() - 1])(db);

                    assert(final_match.match_winners.size() == 1);
                    const account_id_type& winner_id = *final_match.match_winners.begin();
                    BOOST_TEST_MESSAGE( "The winner of " + std::string(object_id_type(tournament_id))  + " is " + winner_id(db).name + " " +  std::string(object_id_type(winner_id)));
                    share_type rake_amount = (fc::uint128_t(tournament.prize_pool.value) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100).to_uint64();
                    optional<account_id_type> dividend_account = tournament_helper.get_asset_dividend_account(tournament.options.buy_in.asset_id);
                    if (dividend_account.valid())
                        players_balances[*dividend_account][tournament.options.buy_in.asset_id] += rake_amount;                    players_balances[winner_id][tournament.options.buy_in.asset_id] += tournament.prize_pool - rake_amount;

                    tournaments.erase(tournament_id);
                    --tournaments_to_complete;
                    break;
                }
            }
            sleep(1);
        }
        BOOST_CHECK(tournaments.size() == 0);
        // checking if prizes were distributed correctly
        std::map<account_id_type, std::map<asset_id_type, share_type>> last_players_balances = tournament_helper.list_players_balances();
        for (auto a: last_players_balances)
        {
            BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s " + GRAPHENE_SYMBOL + " balance " + std::to_string((uint64_t) (a.second[asset_id_type()].value)));
            BOOST_CHECK(a.second[asset_id_type()] == players_balances[a.first][asset_id_type()]);
            BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s " + DEF_SYMBOL + " balance " + std::to_string((uint64_t) (a.second[asset_id_type(1)].value)));
            BOOST_CHECK(a.second[asset_id_type(1)] == players_balances[a.first][asset_id_type(1)]);
        }

        BOOST_TEST_MESSAGE("Bye two assets tournament test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

// Test of concurrently played tournaments having
// 2, 4, 8 ... 64 players randomly registered from global pool
// and randomized number of wins,
// generates random moves,
// checks prizes distribution and fees calculation.
// No "bye" matches.
BOOST_FIXTURE_TEST_CASE( basic, database_fixture )
{
    try
    {
        #define MIN_PLAYERS_NUMBER 2
        #define MAX_PLAYERS_NUMBER 64

        #define MIN_WINS_NUMBER 2
        #define MAX_WINS_NUMBER 5

        BOOST_TEST_MESSAGE("Hello basic tournament test");

        ACTORS((nathan));
        fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
        transfer(committee_account, nathan_id, asset(1000000000));
        upgrade_to_lifetime_member(nathan);
        BOOST_CHECK(nathan.is_lifetime_member());

        // creating random order of numbers of players joining tournaments
        std::vector<unsigned> players;
        for(unsigned i = MIN_PLAYERS_NUMBER; i <= MAX_PLAYERS_NUMBER; i*=2)
        {
            players.emplace_back(i);
        }
        for (unsigned i = players.size() - 1; i >= 1; --i)
        {
           if (std::rand() % 2 == 0) continue;
           unsigned j = std::rand() % i;
           std::swap(players[i], players[j]);
        }

        // creating a pool of actors
        std::vector<std::tuple<std::string, account_id_type, fc::ecc::private_key>> actors;
        for(unsigned i = 0; i < 3 * MAX_PLAYERS_NUMBER; ++i)
        {
            std::string name = "account" + std::to_string(i);
            auto priv_key = generate_private_key(name);
            const auto& account = create_account(name, priv_key.get_public_key());
            actors.emplace_back(name, account.id, priv_key);
            transfer(committee_account, account.id,  asset((uint64_t)1000000000 * players.size() + 100000000 * (i+1)));
        }
#if 0
        enable_fees();
        wdump((db.get_global_properties()));
#endif
        // creating tournaments, registering players
        tournaments_helper tournament_helper(*this);
        for (unsigned i = 0; i < players.size(); ++i)
        {
            auto number_of_players = players[i];
            auto number_of_wins = RAND_MAX_MIN(MAX_WINS_NUMBER,  MIN_WINS_NUMBER);
            BOOST_TEST_MESSAGE( "Preparing tournament with " + std::to_string(number_of_players)  + " players and " + std::to_string(number_of_wins) + " wins" );

            asset buy_in = asset(1000 * number_of_players + 100 * i);
            tournament_id_type tournament_id;
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, number_of_players, 30, 30, number_of_wins);

            for (unsigned j = 0; j < actors.size() && number_of_players > 0; ++j)
            {
                if (number_of_players < actors.size() - j && std::rand() % 2 == 0) continue;
                auto a = actors[j];
                --number_of_players;
                tournament_helper.join_tournament(tournament_id, std::get<1>(a), std::get<1>(a), std::get<2>(a), buy_in);
            }
        }

        uint16_t tournaments_to_complete = players.size();
        std::set<tournament_id_type> tournaments = tournament_helper.list_tournaments();
        std::map<account_id_type, std::map<asset_id_type, share_type>> players_initial_balances = tournament_helper.list_players_balances();
        tournament_helper.reset_players_fees();
        uint16_t rake_fee_percentage = db.get_global_properties().parameters.rake_fee_percentage;
#if 0
        wlog( "Prepared tournaments:");
        for(const tournament_id_type& tid: tournament_helper.list_tournaments())
        {
            const tournament_object tournament = tid(db);
            //const tournament_details_object details = tournament.tournament_details_id(db);
            wlog(" # ${i}, players count ${c}, wins number ${w}", ("i", tid.instance) ("c", tournament.registered_players) ("w", tournament.options.number_of_wins ));
        }

#endif
        BOOST_TEST_MESSAGE( "Generating blocks, waiting for tournaments' completion");
        tournament_helper.reset_players_fees();
        while(tournaments_to_complete > 0)
        {
            generate_block();
            enable_fees();
            tournament_helper.play_games();
            for(const auto& tournament_id: tournaments)
            {
                const tournament_object& tournament = tournament_id(db);
                if (tournament.get_state() == tournament_state::concluded) {
                    const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
                    const match_object& final_match = (tournament_details.matches[tournament_details.matches.size() - 1])(db);

                    assert(final_match.match_winners.size() == 1);
                    const account_id_type& winner_id = *final_match.match_winners.begin();
                    BOOST_TEST_MESSAGE( "The winner of " + std::string(object_id_type(tournament_id))  + " is " + winner_id(db).name + " " +  std::string(object_id_type(winner_id)));
                    share_type rake_amount = (fc::uint128_t(tournament.prize_pool.value) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100).to_uint64();
                    optional<account_id_type> dividend_account = tournament_helper.get_asset_dividend_account(tournament.options.buy_in.asset_id);
                    if (dividend_account.valid())
                        players_initial_balances[*dividend_account][tournament.options.buy_in.asset_id] += rake_amount;
                    players_initial_balances[winner_id][tournament.options.buy_in.asset_id] += tournament.prize_pool - rake_amount;

                    tournaments.erase(tournament_id);
                    --tournaments_to_complete;
                    break;
                }
            }
            sleep(1);
        }
        BOOST_CHECK(tournaments.size() == 0);

        // checking if prizes were distributed correctly and fees calculated properly
        std::map<account_id_type, std::map<asset_id_type, share_type>> current_players_balances = tournament_helper.list_players_balances();
        std::map<account_id_type, std::map<asset_id_type, share_type>> players_paid_fees = tournament_helper.get_players_fees();
        for (auto a: current_players_balances)
        {
            BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s balance " + std::to_string((uint64_t)(a.second[asset_id_type()].value)) );
            BOOST_CHECK(a.second[asset_id_type()] == players_initial_balances[a.first][asset_id_type()] + players_paid_fees[a.first][asset_id_type()]);
        }

        BOOST_TEST_MESSAGE("Bye basic tournament test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }

}

#ifdef BYE_MATCHES_FIXED
// Test of several concurrently played tournaments having
// randomized number of players registered from global pool,
// and randomized number of wins,
// generates random moves,
// checks prizes distribution.
// "bye" matches fix is required.
BOOST_FIXTURE_TEST_CASE( massive, database_fixture )
{
    try
    {
        #define MIN_TOURNAMENTS_NUMBER 7
        #define MAX_TOURNAMENTS_NUMBER 13

        #define MIN_PLAYERS_NUMBER 2
        #define MAX_PLAYERS_NUMBER 64

        #define MIN_WINS_NUMBER 2
        #define MAX_WINS_NUMBER 5

        BOOST_TEST_MESSAGE("Hello massive tournament test");

        ACTORS((nathan));
        fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
        transfer(committee_account, nathan_id, asset(1000000000));
        upgrade_to_lifetime_member(nathan);
        BOOST_CHECK(nathan.is_lifetime_member());

        // creating a pool of actors
        std::vector<std::tuple<std::string, account_id_type, fc::ecc::private_key>> actors;
        for(unsigned i = 0; i < 3 * MAX_PLAYERS_NUMBER; ++i)
        {
            std::string name = "account" + std::to_string(i);
            auto priv_key = generate_private_key(name);
            const auto& account = create_account(name, priv_key.get_public_key());
            actors.emplace_back(name, account.id, priv_key);
            transfer(committee_account, account.id,  asset((uint64_t)1000000 * MAX_TOURNAMENTS_NUMBER + 100000 * (i+1)));
        }

        // creating tournaments, registering players
        tournaments_helper tournament_helper(*this);
        unsigned number_of_tournaments = RAND_MAX_MIN(MAX_TOURNAMENTS_NUMBER, MIN_TOURNAMENTS_NUMBER);
        for(unsigned i = 0; i < number_of_tournaments; ++i)
        {
            unsigned number_of_players = RAND_MAX_MIN(MAX_PLAYERS_NUMBER, MIN_PLAYERS_NUMBER);
            unsigned number_of_wins = RAND_MAX_MIN(MAX_WINS_NUMBER, MIN_WINS_NUMBER);
            BOOST_TEST_MESSAGE( "Preparing tournament with " + std::to_string(number_of_players)  + " players and " + std::to_string(number_of_wins) + " wins" );

            asset buy_in = asset(1000 * number_of_players + 100 * i);
            tournament_id_type tournament_id;
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, number_of_players, 30, 30, number_of_wins);
            const tournament_object& tournament = tournament_id(db);

            for (unsigned j = 0; j < actors.size()-1 && number_of_players > 0; ++j)
            {
                if (number_of_players < actors.size() - j && std::rand() % 2 == 0) continue;
                auto a = actors[j];
                tournament_helper.join_tournament(tournament_id, std::get<1>(a), std::get<1>(a), std::get<2>(a), buy_in);
                if (j == i)
                {
                    BOOST_TEST_MESSAGE("Player " + std::get<0>(a) + " is leaving tournament " + std::to_string(i) +
                                        ", when tournament state is " + std::to_string((int)tournament.get_state()));
                    tournament_helper.leave_tournament(tournament_id, std::get<1>(a), std::get<1>(a), std::get<2>(a));
                    continue;
                }
                --number_of_players;
#if 0
                // ok if registration allowed till deadline
                if (!number_of_players)
                {
                    BOOST_TEST_MESSAGE("Player " + std::get<0>(a) + " is leaving tournament " + std::to_string(i) +
                                        ", when tournament state is " + std::to_string((int)tournament.get_state()));
                    tournament_helper.leave_tournament(tournament_id, std::get<1>(a), std::get<1>(a), std::get<2>(a));
                    ++j;
                    a = actors[j];
                    BOOST_TEST_MESSAGE("Player " + std::get<0>(a) + " is joinig tournament " + std::to_string(i) +
                                        ", when tournament state is " + std::to_string((int)tournament.get_state()));
                    tournament_helper.join_tournament(tournament_id, std::get<1>(a), std::get<1>(a), std::get<2>(a), buy_in);
                }
#endif
            }
            BOOST_TEST_MESSAGE("Tournament " +  std::to_string(i) + " is in state " + std::to_string((int)tournament.get_state()));
        }

        uint16_t tournaments_to_complete = number_of_tournaments;
        std::set<tournament_id_type> tournaments = tournament_helper.list_tournaments();
        std::map<account_id_type, std::map<asset_id_type, share_type>> players_balances = tournament_helper.list_players_balances();
        uint16_t rake_fee_percentage = db.get_global_properties().parameters.rake_fee_percentage;

        BOOST_TEST_MESSAGE( "Generating blocks, waiting for tournaments' completion");
        while(tournaments_to_complete > 0)
        {
            generate_block();
            tournament_helper.play_games();
            for(const auto& tournament_id: tournaments)
            {
                const tournament_object& tournament = tournament_id(db);
                if (tournament.get_state() == tournament_state::concluded) {
                    const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
                    const match_object& final_match = (tournament_details.matches[tournament_details.matches.size() - 1])(db);

                    assert(final_match.match_winners.size() == 1);
                    const account_id_type& winner_id = *final_match.match_winners.begin();
                    BOOST_TEST_MESSAGE( "The winner of " + std::string(object_id_type(tournament_id))  + " is " + winner_id(db).name + " " +  std::string(object_id_type(winner_id)));
                    share_type rake_amount = (fc::uint128_t(tournament.prize_pool.value) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100).to_uint64();
                    optional<account_id_type> dividend_account = tournament_helper.get_asset_dividend_account(tournament.options.buy_in.asset_id);
                    if (dividend_account.valid())
                        players_balances[*dividend_account][tournament.options.buy_in.asset_id] += rake_amount;
                    players_balances[winner_id][tournament.options.buy_in.asset_id] += tournament.prize_pool - rake_amount;

                    tournaments.erase(tournament_id);
                    --tournaments_to_complete;
                    break;
                }
            }
            sleep(1);
        }
        BOOST_CHECK(tournaments.size() == 0);
#if 0
        wlog( "Performed tournaments:");
        for(const tournament_id_type& tid: tournament_helper.list_tournaments())
        {
            const tournament_object tournament = tid(db);
            //const tournament_details_object details = tournament.tournament_details_id(db);
            wlog(" # ${i}, players count ${c}, wins number ${w}", ("i", tid.instance) ("c", tournament.registered_players) ("w", tournament.options.number_of_wins ));
        }
#endif
        // checking if prizes were distributed correctly
        std::map<account_id_type, std::map<asset_id_type, share_type>> last_players_balances = tournament_helper.list_players_balances();
        for (auto a: last_players_balances)
        {
            BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s balance " + std::to_string((uint64_t)(a.second[asset_id_type()].value)) );
            BOOST_CHECK(a.second[asset_id_type()] == players_balances[a.first][asset_id_type()]);
        }

        BOOST_TEST_MESSAGE("Bye massive tournament test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}
#endif

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
