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
#include "tournament_helper.hpp"

#include <graphene/chain/game_object.hpp>
#include <graphene/chain/match_object.hpp>
#include <graphene/chain/tournament_object.hpp>

#include <fc/crypto/rand.hpp>

using namespace graphene::chain;

tournaments_helper::tournaments_helper(database_fixture& df) : df(df)
{
   assets.insert(asset_id_type());
   current_asset_idx = 0;
   optional<account_id_type> dividend_account = get_asset_dividend_account(asset_id_type());
   if (dividend_account.valid())
      players.insert(*dividend_account);
}

const std::set<tournament_id_type>& tournaments_helper::list_tournaments()const
{
   return tournaments;
}

const std::map<account_id_type, std::map<asset_id_type, share_type>> tournaments_helper::list_players_balances()const
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

const std::map<account_id_type, std::map<asset_id_type, share_type>> tournaments_helper::get_players_fees()const
{
   return players_fees;
}

void tournaments_helper::reset_players_fees()
{
        for (account_id_type player_id: players)
        {
            for( asset_id_type asset_id: assets)
            {
                players_fees[player_id][asset_id] = 0;
            }
        }
}

void tournaments_helper::create_asset(const account_id_type& issuer_account_id,
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

void tournaments_helper::update_dividend_asset(const asset_id_type asset_to_update_id,
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

optional<account_id_type> tournaments_helper::get_asset_dividend_account(const asset_id_type& asset_id)const
{
        graphene::chain::database& db = df.db;
        optional<account_id_type> result;
        const asset_object& asset_obj = asset_id_type()(db);

        if (asset_obj.dividend_data_id.valid())
        {
           const asset_dividend_data_object& dividend_data = (*asset_obj.dividend_data_id)(db);
           result = dividend_data.dividend_distribution_account;
        }
        return result;
}

const tournament_id_type tournaments_helper::create_tournament (const account_id_type& creator,
                                                const fc::ecc::private_key& sig_priv_key,
                                                asset buy_in,
                                                uint32_t number_of_players,
                                                uint32_t time_per_commit_move,
                                                uint32_t time_per_reveal_move,
                                                uint32_t number_of_wins,
                                                int32_t registration_deadline,
                                                uint32_t start_delay,
                                                uint32_t round_delay,
                                                bool insurance_enabled,
                                                uint32_t number_of_gestures,
                                                uint32_t start_time,
                                                fc::optional<flat_set<account_id_type> > whitelist
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

        game_options.number_of_gestures = number_of_gestures;
        game_options.time_per_commit_move = time_per_commit_move;
        game_options.time_per_reveal_move = time_per_reveal_move;
        game_options.insurance_enabled = insurance_enabled;

        options.registration_deadline = db.head_block_time() + fc::seconds(registration_deadline + *current_tournament_idx);
        options.buy_in = buy_in;
        options.number_of_players = number_of_players;
        if (start_delay)
            options.start_delay = start_delay;
        if (start_time)
            options.start_time = db.head_block_time() + fc::seconds(start_time);
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

void tournaments_helper::join_tournament(const tournament_id_type & tournament_id,
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

void tournaments_helper::leave_tournament(const tournament_id_type & tournament_id,
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
void tournaments_helper::rps_throw(const game_id_type& game_id,
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
       FC_ASSERT( (int)gesture < game_options.number_of_gestures );

       account_object player_account_obj = player_id(db);

       // construct the complete throw, the commit, and reveal
       rock_paper_scissors_throw full_throw;
       fc::rand_bytes((char*)&full_throw.nonce1, sizeof(full_throw.nonce1));
       fc::rand_bytes((char*)&full_throw.nonce2, sizeof(full_throw.nonce2));
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
       latest_committs[player_account_obj.id] = commit_throw;

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

void tournaments_helper::rps_reveal( const game_id_type& game_id,
                                     const account_id_type& player_id,
                                     rock_paper_scissors_gesture gesture,
                                     const fc::ecc::private_key& sig_priv_key )
{
   graphene::chain::database& db = df.db;

   FC_ASSERT( latest_committs.find(player_id) != latest_committs.end() );
   const auto& reveal = committed_game_moves.find( latest_committs[player_id] );
   FC_ASSERT( reveal != committed_game_moves.end() );
   FC_ASSERT( reveal->second.gesture == gesture );

   game_move_operation move_operation;
   move_operation.game_id = game_id;
   move_operation.player_account_id = player_id;
   move_operation.move = reveal->second;

   signed_transaction tx;
   tx.operations.push_back( move_operation );
   const asset f = db.current_fee_schedule().set_fee( tx.operations[0] );
   players_fees[player_id][f.asset_id] -= f.amount;
   tx.validate();
   test::set_expiration( db, tx );
   df.sign( tx, sig_priv_key );
   PUSH_TX(db, tx);
}

// spaghetti programming
// walking through all tournaments, matches and games and throwing random moves
// optionaly skip generting randomly selected moves
// every_move_is >= 0 : every game is tie
void tournaments_helper::play_games(unsigned skip_some_commits, unsigned skip_some_reveals, int every_move_is)
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
