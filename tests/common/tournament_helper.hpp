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
#include "../common/database_fixture.hpp"

using namespace graphene::chain;

// class performing operations necessary for creating tournaments,
// having players join the tournaments and playing tournaments to completion.
class tournaments_helper
{
public:

    tournaments_helper(database_fixture& df);

    const std::set<tournament_id_type>& list_tournaments()const;

    const std::map<account_id_type, std::map<asset_id_type, share_type>> list_players_balances()const;

    const std::map<account_id_type, std::map<asset_id_type, share_type>> get_players_fees()const;

    void reset_players_fees();

    void create_asset(const account_id_type& issuer_account_id,
                      const string& symbol,
                      uint8_t precision,
                      asset_options& common,
                      const fc::ecc::private_key& sig_priv_key);

    void update_dividend_asset(const asset_id_type asset_to_update_id,
                               dividend_asset_options new_options,
                               const fc::ecc::private_key& sig_priv_key);

    optional<account_id_type> get_asset_dividend_account( const asset_id_type& asset_id = asset_id_type() )const;

    const tournament_id_type create_tournament( const account_id_type& creator,
                                                const fc::ecc::private_key& sig_priv_key,
                                                asset buy_in,
                                                uint32_t number_of_players = 2,
                                                uint32_t time_per_commit_move = 3,
                                                uint32_t time_per_reveal_move = 1,
                                                uint32_t number_of_wins = 3,
                                                int32_t registration_deadline = 3600,
                                                uint32_t start_delay = 3,
                                                uint32_t round_delay = 3,
                                                bool insurance_enabled = false,
                                                uint32_t number_of_gestures = 3,
                                                uint32_t start_time = 0,
                                                fc::optional<flat_set<account_id_type> > whitelist = fc::optional<flat_set<account_id_type> >()
                                                );

    void join_tournament(const tournament_id_type & tournament_id,
                         const account_id_type& player_id,
                         const account_id_type& payer_id,
                         const fc::ecc::private_key& sig_priv_key,
                         asset buy_in
                         );

   void leave_tournament(const tournament_id_type & tournament_id,
                         const account_id_type& player_id,
                         const account_id_type& canceling_account_id,
                         const fc::ecc::private_key& sig_priv_key
                        );

    // stolen from cli_wallet
    void rps_throw(const game_id_type& game_id,
                   const account_id_type& player_id,
                   rock_paper_scissors_gesture gesture,
                   const fc::ecc::private_key& sig_priv_key
                   );

    void rps_reveal( const game_id_type& game_id,
                     const account_id_type& player_id,
                     rock_paper_scissors_gesture gesture,
                     const fc::ecc::private_key& sig_priv_key );

    // spaghetti programming
    // walking through all tournaments, matches and games and throwing random moves
    // optionaly skip generting randomly selected moves
    // every_move_is >= 0 : every game is tie
    void play_games(unsigned skip_some_commits = 0, unsigned skip_some_reveals = 0, int every_move_is = -1);

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
    // store of latest commits, for use by rps_reveal
    std::map<account_id_type, rock_paper_scissors_throw_commit> latest_committs;
};
