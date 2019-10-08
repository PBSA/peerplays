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
#include <graphene/chain/database.hpp>
#include <graphene/chain/game_object.hpp>
#include <graphene/chain/tournament_object.hpp>
#include <graphene/chain/match_object.hpp>

#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/msm/back/tools.hpp>
#include <boost/range/algorithm/copy.hpp>

#include <fc/crypto/hash_ctr_rng.hpp>

namespace graphene { namespace chain {

   namespace msm = boost::msm;
   namespace mpl = boost::mpl;

   namespace 
   {
      // Events
      struct initiate_game
      {
         database& db;
         vector<account_id_type> players;
         initiate_game(database& db, const vector<account_id_type>& players) :
            db(db), players(players)
         {}
      };

      struct game_move
      {
         database& db;
         const game_move_operation& move;
         game_move(database& db, const game_move_operation& move) :
            db(db), move(move)
         {}
      };

      struct timeout
      {
         database& db;
         timeout(database& db) :
            db(db)
         {}
      };

      struct game_state_machine_ : public msm::front::state_machine_def<game_state_machine_>
      {
         // disable a few state machine features we don't use for performance
         typedef int no_exception_thrown;
         typedef int no_message_queue;

         // States
         struct waiting_for_game_to_start : public msm::front::state<> {};
         struct expecting_commit_moves : public msm::front::state<>
         {
            void set_next_timeout(database& db, game_object& game)
            {
               const match_object& match_obj = game.match_id(db);
               const tournament_object& tournament_obj = match_obj.tournament_id(db);
               const rock_paper_scissors_game_options& game_options = tournament_obj.options.game_options.get<rock_paper_scissors_game_options>();
               game.next_timeout = db.head_block_time() + game_options.time_per_commit_move;
            }
            void on_entry(const initiate_game& event, game_state_machine_& fsm)
            {
               game_object& game = *fsm.game_obj;

               fc_ilog(fc::logger::get("tournament"),
                       "game ${id} is now in progress, expecting commit moves",
                       ("id", game.id));
               fc_ilog(fc::logger::get("tournament"),
                       "game ${id} is associtated with match ${match_id}",
                       ("id", game.id)
                       ("match_id", game.match_id));
               set_next_timeout(event.db, game);
            }
            void on_entry(const game_move& event, game_state_machine_& fsm)
            {
               game_object& game = *fsm.game_obj;

               fc_ilog(fc::logger::get("tournament"),
                       "game ${id} received a commit move, still expecting another commit move",
                       ("id", game.id));
            }
         };
         struct expecting_reveal_moves : public msm::front::state<>
         {
            void set_next_timeout(database& db, game_object& game)
            {
               const match_object& match_obj = game.match_id(db);
               const tournament_object& tournament_obj = match_obj.tournament_id(db);
               const rock_paper_scissors_game_options& game_options = tournament_obj.options.game_options.get<rock_paper_scissors_game_options>();
               game.next_timeout = db.head_block_time() + game_options.time_per_reveal_move;
            }
            void on_entry(const timeout& event, game_state_machine_& fsm)
            {
               game_object& game = *fsm.game_obj;
               fc_ilog(fc::logger::get("tournament"),
                       "game ${id} timed out waiting for commit moves, now expecting reveal move",
                       ("id", game.id));
               set_next_timeout(event.db, game);
            }
            void on_entry(const game_move& event, game_state_machine_& fsm)
            {
               game_object& game = *fsm.game_obj;

               if (event.move.move.which() == game_specific_moves::tag<rock_paper_scissors_throw_commit>::value)
               {
                  fc_ilog(fc::logger::get("tournament"),
                          "game ${id} received a commit move, now expecting reveal moves",
                          ("id", game.id));
                  set_next_timeout(event.db, game);
               }
               else
                  fc_ilog(fc::logger::get("tournament"),
                          "game ${id} received a reveal move, still expecting reveal moves",
                          ("id", game.id));
            }
         };

         struct game_complete : public msm::front::state<>
         {
            void clear_next_timeout(database& db, game_object& game)
            {
               //const match_object& match_obj = game.match_id(db);
               //const tournament_object& tournament_obj = match_obj.tournament_id(db);
               //const rock_paper_scissors_game_options& game_options = tournament_obj.options.game_options.get<rock_paper_scissors_game_options>();
               game.next_timeout = fc::optional<fc::time_point_sec>();
            }
            void on_entry(const timeout& event, game_state_machine_& fsm)
            {
               game_object& game = *fsm.game_obj;
               fc_ilog(fc::logger::get("tournament"),
                       "timed out waiting for commits or reveals, game ${id} is complete",
                       ("id", game.id));

               game.make_automatic_moves(event.db);
               game.determine_winner(event.db);
               clear_next_timeout(event.db, game);
            }

            void on_entry(const game_move& event, game_state_machine_& fsm)
            {
               game_object& game = *fsm.game_obj;
               fc_ilog(fc::logger::get("tournament"),
                       "received a reveal move, game ${id} is complete",
                       ("id", fsm.game_obj->id));

               // if one player didn't commit a move we might need to make their "insurance" move now
               game.make_automatic_moves(event.db);
               game.determine_winner(event.db);
               clear_next_timeout(event.db, game);
            }
         };
         typedef waiting_for_game_to_start initial_state;

         typedef game_state_machine_ x; // makes transition table cleaner

         // Guards
         bool already_have_other_commit(const game_move& event)
         {
            auto iter = std::find(game_obj->players.begin(), game_obj->players.end(),
                                  event.move.player_account_id);
            unsigned player_index = std::distance(game_obj->players.begin(), iter);
            // hard-coded here for two-player games
            unsigned other_player_index = player_index == 0 ? 1 : 0;
            const rock_paper_scissors_game_details& game_details = game_obj->game_details.get<rock_paper_scissors_game_details>();
            return game_details.commit_moves.at(other_player_index).valid();
         }

         bool now_have_reveals_for_all_commits(const game_move& event)
         {
            auto iter = std::find(game_obj->players.begin(), game_obj->players.end(),
                                  event.move.player_account_id);
            unsigned this_reveal_index = std::distance(game_obj->players.begin(), iter);

            const rock_paper_scissors_game_details& game_details = game_obj->game_details.get<rock_paper_scissors_game_details>();
            for (unsigned i = 0; i < game_details.commit_moves.size(); ++i)
               if (game_details.commit_moves[i] && !game_details.reveal_moves[i] && i != this_reveal_index)
                  return false;
            return true;
         }

         bool have_at_least_one_commit_move(const timeout& event)
         {
            const rock_paper_scissors_game_details& game_details = game_obj->game_details.get<rock_paper_scissors_game_details>();
            return game_details.commit_moves[0] || game_details.commit_moves[1];
         }

         void apply_commit_move(const game_move& event)
         {
            auto iter = std::find(game_obj->players.begin(), game_obj->players.end(),
                                  event.move.player_account_id);
            unsigned player_index = std::distance(game_obj->players.begin(), iter);

            rock_paper_scissors_game_details& details = game_obj->game_details.get<rock_paper_scissors_game_details>();
            details.commit_moves[player_index] = event.move.move.get<rock_paper_scissors_throw_commit>();
         }

         void apply_reveal_move(const game_move& event)
         {
            auto iter = std::find(game_obj->players.begin(), game_obj->players.end(),
                                  event.move.player_account_id);
            unsigned player_index = std::distance(game_obj->players.begin(), iter);

            rock_paper_scissors_game_details& details = game_obj->game_details.get<rock_paper_scissors_game_details>();
            details.reveal_moves[player_index] = event.move.move.get<rock_paper_scissors_throw_reveal>();
         }

         void start_next_game(const game_complete& event)
         {
            fc_ilog(fc::logger::get("tournament"),
                    "In start_next_game action");
         }

         // Transition table for tournament
         struct transition_table : mpl::vector<
         //    Start                          Event                     Next                         Action                Guard
         //    +-------------------------------+-------------------------+----------------------------+---------------------+----------------------+
         _row  < waiting_for_game_to_start, initiate_game,           expecting_commit_moves >,
         //  +-------------------------------+-------------------------+----------------------------+---------------------+----------------------+
        a_row < expecting_commit_moves,    game_move,               expecting_commit_moves,        &x::apply_commit_move >,
        row   < expecting_commit_moves,    game_move,               expecting_reveal_moves,        &x::apply_commit_move, &x::already_have_other_commit >,
        _row  < expecting_commit_moves,    timeout,                 game_complete >,
        g_row < expecting_commit_moves,    timeout,                 expecting_reveal_moves,                               &x::have_at_least_one_commit_move >,
         //  +-------------------------------+-------------------------+----------------------------+---------------------+----------------------+
        _row  < expecting_reveal_moves,    timeout,                 game_complete >,
        a_row < expecting_reveal_moves,    game_move,               expecting_reveal_moves,        &x::apply_reveal_move >,
        row   < expecting_reveal_moves,    game_move,               game_complete,                 &x::apply_reveal_move, &x::now_have_reveals_for_all_commits >
         //  +-------------------------------+-------------------------+----------------------------+---------------------+----------------------+
         //a_row < game_in_progress,           game_complete,            game_in_progress,           &x::start_next_game >,
         //g_row < game_in_progress,           game_complete,            game_complete,                                    &x::was_final_game >
         //  +---------------------------+-----------------------------+----------------------------+---------------------+----------------------+
         > {};


         game_object* game_obj;
         game_state_machine_(game_object* game_obj) : game_obj(game_obj) {}
      };
      typedef msm::back::state_machine<game_state_machine_> game_state_machine;
   }

   class game_object::impl {
   public:
      game_state_machine state_machine;

      impl(game_object* self) : state_machine(self) {}
   };

   game_object::game_object() :
      my(new impl(this))
   {
   }

   game_object::game_object(const game_object& rhs) : 
      graphene::db::abstract_object<game_object>(rhs),
      match_id(rhs.match_id),
      players(rhs.players),
      winners(rhs.winners),
      game_details(rhs.game_details),
      next_timeout(rhs.next_timeout),
      my(new impl(this))
   {
      my->state_machine = rhs.my->state_machine;
      my->state_machine.game_obj = this;
   }

   game_object& game_object::operator=(const game_object& rhs)
   {
      //graphene::db::abstract_object<game_object>::operator=(rhs);
      id = rhs.id;
      match_id = rhs.match_id;
      players = rhs.players;
      winners = rhs.winners;
      game_details = rhs.game_details;
      next_timeout = rhs.next_timeout;
      my->state_machine = rhs.my->state_machine;
      my->state_machine.game_obj = this;

      return *this;
   }

   game_object::~game_object()
   {
   }

   bool verify_game_state_constants()
   {
      unsigned error_count = 0;
      typedef msm::back::generate_state_set<game_state_machine::stt>::type all_states;
      static char const* filled_state_names[mpl::size<all_states>::value];
      mpl::for_each<all_states,boost::msm::wrap<mpl::placeholders::_1> >
         (msm::back::fill_state_names<game_state_machine::stt>(filled_state_names));
      for (unsigned i = 0; i < mpl::size<all_states>::value; ++i)
      {
         try
         {
            // this is an approximate test, the state name provided by typeinfo will be mangled, but should
            // at least contain the string we're looking for
            const char* fc_reflected_value_name = fc::reflector<game_state>::to_string((game_state)i);
            if (!strcmp(fc_reflected_value_name, filled_state_names[i]))
               fc_elog(fc::logger::get("game"),
                       "Error, state string misgame between fc and boost::msm for int value ${int_value}: boost::msm -> ${boost_string}, fc::reflect -> ${fc_string}",
                       ("int_value", i)("boost_string", filled_state_names[i])("fc_string", fc_reflected_value_name));
         }
         catch (const fc::bad_cast_exception&)
         {
            fc_elog(fc::logger::get("game"),
                    "Error, no reflection for value ${int_value} in enum game_state",
                    ("int_value", i));
            ++error_count;
         }
      }

      return error_count == 0;
   }

   game_state game_object::get_state() const
   {
      static bool state_constants_are_correct = verify_game_state_constants();
      (void)&state_constants_are_correct;
      game_state state = (game_state)my->state_machine.current_state()[0];

      return state;  
   }

   void game_object::evaluate_move_operation(const database& db, const game_move_operation& op) const
   {
      //const match_object& match_obj = match_id(db);

      if (game_details.which() == game_specific_details::tag<rock_paper_scissors_game_details>::value)
      {
         if (op.move.which() == game_specific_moves::tag<rock_paper_scissors_throw_commit>::value)
         {
            // Is this move made by a player in the match
            auto iter = std::find(players.begin(), players.end(),
                                  op.player_account_id);
            if (iter == players.end())
               FC_THROW("Player ${account_id} is not a player in game ${game}",
                        ("account_id", op.player_account_id)
                        ("game", id));
            unsigned player_index = std::distance(players.begin(), iter);

            //const rock_paper_scissors_throw_commit& commit = op.move.get<rock_paper_scissors_throw_commit>();
            
            // are we expecting commits?
            if (get_state() != game_state::expecting_commit_moves)
               FC_THROW("Game ${game} is not accepting any commit moves", ("game", id));

            // has this player committed already?
            const rock_paper_scissors_game_details& details = game_details.get<rock_paper_scissors_game_details>();
            if (details.commit_moves.at(player_index))
               FC_THROW("Player ${account_id} has already committed their move for game ${game}",
                        ("account_id", op.player_account_id)
                        ("game", id));
            // if all the above checks pass, then the move is accepted
         }    
         else if (op.move.which() == game_specific_moves::tag<rock_paper_scissors_throw_reveal>::value)
         {
            // Is this move made by a player in the match
            auto iter = std::find(players.begin(), players.end(),
                                  op.player_account_id);
            if (iter == players.end())
               FC_THROW("Player ${account_id} is not a player in game ${game}",
                        ("account_id", op.player_account_id)
                        ("game", id));
            unsigned player_index = std::distance(players.begin(), iter);

            // has this player committed already?
            const rock_paper_scissors_game_details& details = game_details.get<rock_paper_scissors_game_details>();
            if (!details.commit_moves.at(player_index))
               FC_THROW("Player ${account_id} cannot reveal a move which they did not commit in game ${game}",
                        ("account_id", op.player_account_id)
                        ("game", id));

            // are we expecting reveals?
            if (get_state() != game_state::expecting_reveal_moves)
               FC_THROW("Game ${game} is not accepting any reveal moves", ("game", id));

            const rock_paper_scissors_throw_commit& commit = *details.commit_moves.at(player_index);
            const rock_paper_scissors_throw_reveal& reveal = op.move.get<rock_paper_scissors_throw_reveal>();

            // does the reveal match the commit?
            rock_paper_scissors_throw reconstructed_throw;
            reconstructed_throw.nonce1 = commit.nonce1;
            reconstructed_throw.nonce2 = reveal.nonce2;
            reconstructed_throw.gesture = reveal.gesture;
            fc::sha256 reconstructed_hash = reconstructed_throw.calculate_hash();

            if (commit.throw_hash != reconstructed_hash)
               FC_THROW("Reveal does not match commit's hash of ${commit_hash}", 
                        ("commit_hash", commit.throw_hash));

            // is the throw valid for this game
            const match_object& match_obj = match_id(db);
            const tournament_object& tournament_obj = match_obj.tournament_id(db);
            const rock_paper_scissors_game_options& game_options = tournament_obj.options.game_options.get<rock_paper_scissors_game_options>();
            if ((unsigned)reveal.gesture >= game_options.number_of_gestures)
               FC_THROW("Gesture ${gesture_int} is not valid for this game", ("gesture", (unsigned)reveal.gesture));
            // if all the above checks pass, then the move is accepted
         }
         else
            FC_THROW("The only valid moves in a rock-paper-scissors game are commit and reveal, not ${type}", 
                     ("type", op.move.which()));
      }
      else
         FC_THROW("Game of type ${type} not supported", ("type", game_details.which()));
   }

   void game_object::make_automatic_moves(database& db)
   {
      rock_paper_scissors_game_details& rps_game_details = game_details.get<rock_paper_scissors_game_details>();

      unsigned players_without_commit_moves = 0;
      bool no_player_has_reveal_move = true;
      for (unsigned i = 0; i < 2; ++i)
      {
         if (!rps_game_details.commit_moves[i])
            ++players_without_commit_moves;
         if (rps_game_details.reveal_moves[i])
            no_player_has_reveal_move = false;
      }

      if (players_without_commit_moves || no_player_has_reveal_move)
      {
         const match_object& match_obj = match_id(db);
         const tournament_object& tournament_obj = match_obj.tournament_id(db);
         const rock_paper_scissors_game_options& game_options = tournament_obj.options.game_options.get<rock_paper_scissors_game_options>();

         if (game_options.insurance_enabled)
         {
             for (unsigned i = 0; i < 2; ++i)
             {
                if (!rps_game_details.commit_moves[i] ||
                    no_player_has_reveal_move)
                {
                   struct rock_paper_scissors_throw_reveal reveal;
                   reveal.nonce2 = 0;
                   reveal.gesture = (rock_paper_scissors_gesture)db.get_random_bits(game_options.number_of_gestures);
                   rps_game_details.reveal_moves[i] = reveal;
                   ilog("Player ${player} failed to commit a move, generating a random move for him: ${gesture}",
                        ("player", i)("gesture", reveal.gesture));
                }
             }
         }
      }
   }

   void game_object::determine_winner(database& db)
   {
      // we now know who played what, figure out if we have a winner
      const rock_paper_scissors_game_details& rps_game_details = game_details.get<rock_paper_scissors_game_details>();
      if (rps_game_details.reveal_moves[0] && rps_game_details.reveal_moves[1] &&
          rps_game_details.reveal_moves[0]->gesture == rps_game_details.reveal_moves[1]->gesture)
         ilog("The game was a tie, both players threw ${gesture}", ("gesture", rps_game_details.reveal_moves[0]->gesture));
      else 
      {
         const match_object& match_obj = match_id(db);
         const tournament_object& tournament_obj = match_obj.tournament_id(db);
         const rock_paper_scissors_game_options& game_options = tournament_obj.options.game_options.get<rock_paper_scissors_game_options>();

         if (rps_game_details.reveal_moves[0] && rps_game_details.reveal_moves[1])
         {
            unsigned winner = ((((int)rps_game_details.reveal_moves[0]->gesture - 
                                 (int)rps_game_details.reveal_moves[1]->gesture +
                                 game_options.number_of_gestures) % game_options.number_of_gestures) + 1) % 2;
            ilog("${gesture1} vs ${gesture2}, ${winner} wins",
                 ("gesture1", rps_game_details.reveal_moves[1]->gesture)
                 ("gesture2", rps_game_details.reveal_moves[0]->gesture)
                 ("winner", rps_game_details.reveal_moves[winner]->gesture));
            winners.insert(players[winner]);
         }
         else if (rps_game_details.reveal_moves[0])
         {
            ilog("Player 1 didn't commit or reveal their move, player 0 wins");
            winners.insert(players[0]);
         }
         else if (rps_game_details.reveal_moves[1])
         {
            ilog("Player 0 didn't commit or reveal their move, player 1 wins");
            winners.insert(players[1]);
         }
         else
         {
            ilog("Neither player made a move, both players lose");
         }
      }

   
      const match_object& match_obj = match_id(db);
      db.modify(match_obj, [&](match_object& match) {
         match.on_game_complete(db, *this);
         });
   }

   void game_object::on_move(database& db, const game_move_operation& op)
   {
      my->state_machine.process_event(game_move(db, op));
   }

   void game_object::on_timeout(database& db)
   {
      my->state_machine.process_event(timeout(db));
   }

   void game_object::start_game(database& db, const std::vector<account_id_type>& players)
   {
      my->state_machine.process_event(initiate_game(db, players));
   }

   void game_object::pack_impl(std::ostream& stream) const
   {
      boost::archive::binary_oarchive oa(stream, boost::archive::no_header|boost::archive::no_codecvt|boost::archive::no_xml_tag_checking);
      oa << my->state_machine;
   }

   void game_object::unpack_impl(std::istream& stream)
   {
      boost::archive::binary_iarchive ia(stream, boost::archive::no_header|boost::archive::no_codecvt|boost::archive::no_xml_tag_checking);
      ia >> my->state_machine;
   }

} } // graphene::chain

namespace fc { 
   // Manually reflect game_object to variant to properly reflect "state"
   void to_variant(const graphene::chain::game_object& game_obj, fc::variant& v, uint32_t max_depth)
   {
      fc_elog(fc::logger::get("tournament"), "In game_obj to_variant");
      elog("In game_obj to_variant");
      fc::mutable_variant_object o;
      o("id", fc::variant(game_obj.id,  max_depth ))
       ("match_id", fc::variant(game_obj.match_id,  max_depth ))
       ("players", fc::variant(game_obj.players,  max_depth ))
       ("winners", fc::variant(game_obj.winners,  max_depth ))
       ("game_details", fc::variant(game_obj.game_details,  max_depth ))
       ("next_timeout", fc::variant(game_obj.next_timeout,  max_depth ))
       ("state", fc::variant(game_obj.get_state(),  max_depth ));

      v = o;
   }

   // Manually reflect game_object to variant to properly reflect "state"
   void from_variant(const fc::variant& v, graphene::chain::game_object& game_obj, uint32_t max_depth)
   {
      fc_elog(fc::logger::get("tournament"), "In game_obj from_variant");
      game_obj.id = v["id"].as<graphene::chain::game_id_type>(  max_depth  );
      game_obj.match_id = v["match_id"].as<graphene::chain::match_id_type>(  max_depth  );
      game_obj.players = v["players"].as<std::vector<graphene::chain::account_id_type> >(  max_depth  );
      game_obj.winners = v["winners"].as<flat_set<graphene::chain::account_id_type> >(  max_depth  );
      game_obj.game_details = v["game_details"].as<graphene::chain::game_specific_details>(  max_depth  );
      game_obj.next_timeout = v["next_timeout"].as<fc::optional<time_point_sec> >(  max_depth  );
      graphene::chain::game_state state = v["state"].as<graphene::chain::game_state>(  max_depth  );
      const_cast<int*>(game_obj.my->state_machine.current_state())[0] = (int)state;
   }
} //end namespace fc


