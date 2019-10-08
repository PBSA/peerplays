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
#include <graphene/chain/tournament_object.hpp>
#include <graphene/chain/match_object.hpp>
#include <graphene/chain/game_object.hpp>

#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/msm/back/tools.hpp>
#include <boost/range/algorithm/copy.hpp>

namespace graphene { namespace chain {

   namespace msm = boost::msm;
   namespace mpl = boost::mpl;

   namespace 
   {
      // Events
      struct initiate_match
      {
         database& db;
         initiate_match(database& db) :
            db(db)
         {}
      };

      struct game_complete 
      {
         database& db;
         const game_object& game;
         game_complete(database& db, const game_object& game) : db(db), game(game) {};
      };

      struct match_state_machine_ : public msm::front::state_machine_def<match_state_machine_>
      {
         // disable a few state machine features we don't use for performance
         typedef int no_exception_thrown;
         typedef int no_message_queue;

         // States
         struct waiting_on_previous_matches : public msm::front::state<>{};
         struct match_in_progress : public msm::front::state<>
         {
            void on_entry(const game_complete& event, match_state_machine_& fsm)
            {
               fc_ilog(fc::logger::get("tournament"),
                       "Game ${game_id} in match ${id} is complete",
                       ("game_id", event.game.id)("id", fsm.match_obj->id));
            }
            void on_entry(const initiate_match& event, match_state_machine_& fsm)
            {
               match_object& match = *fsm.match_obj;
               fc_ilog(fc::logger::get("tournament"),
                       "Match ${id} is now in progress",
                       ("id", match.id));
               match.number_of_wins.resize(match.players.size());
               match.start_time = event.db.head_block_time();

               fsm.start_next_game(event.db);
            }
         };
         struct match_complete : public msm::front::state<>
         {
            void on_entry(const game_complete& event, match_state_machine_& fsm)
            {

               match_object& match = *fsm.match_obj;
               //wdump((match));
               fc_ilog(fc::logger::get("tournament"),
                       "Match ${id} is complete",
                       ("id", match.id));

               optional<account_id_type> last_game_winner;
               std::map<account_id_type, unsigned> scores_by_player;
               for (const flat_set<account_id_type>& game_winners : match.game_winners)
                  for (const account_id_type& account_id : game_winners)
                  {
                     ++scores_by_player[account_id];
                     last_game_winner = account_id;
                  }
               
               bool all_scores_same = true;
               optional<account_id_type> high_scoring_account;
               unsigned high_score = 0;
               for (const auto& value : scores_by_player)
                  if (value.second > high_score)
                  {
                     if (high_scoring_account)
                         all_scores_same = false;
                     high_score = value.second;
                     high_scoring_account = value.first;
                  }

               if (high_scoring_account)
               {
                  if (all_scores_same && last_game_winner)
                      match.match_winners.insert(*last_game_winner);
                  else
                      match.match_winners.insert(*high_scoring_account);
               }
               else
               {
                  // III. Rock Paper Scissors Game Need to review how Ties are dealt with.
                  short i = match.number_of_ties == match.games.size() ? 0 : event.db.get_random_bits(match.players.size()) ;
                  match.match_winners.insert(match.players[i]);
                  ++match.number_of_wins[i];
                  if (match.number_of_ties == match.games.size())
                      match.game_winners[match.game_winners.size()-1].insert(match.players[i]);
               }

               match.end_time = event.db.head_block_time();
               const tournament_object& tournament_obj = match.tournament_id(event.db);
               event.db.modify(tournament_obj, [&](tournament_object& tournament) {
                     tournament.on_match_completed(event.db, match);
                     });
            }
            void on_entry(const initiate_match& event, match_state_machine_& fsm)
            {
               match_object& match = *fsm.match_obj;
               fc_ilog(fc::logger::get("tournament"),
                       "Match ${id} is complete, it was a buy",
                       ("id", match));
               match.number_of_wins.resize(match.players.size());
               boost::copy(match.players, std::inserter(match.match_winners, match.match_winners.end()));
               match.start_time = event.db.head_block_time();
               match.end_time = event.db.head_block_time();
               // NOTE: when the match is a buy, we don't send a match completed event to
               // the tournament_obj, because it is already in the middle of handling
               // an event; it will figure out that the match has completed on its own.
            }
         };
         typedef waiting_on_previous_matches initial_state;

         typedef match_state_machine_ x; // makes transition table cleaner
         
         bool was_final_game(const game_complete& event)
         {
            const tournament_object& tournament_obj = match_obj->tournament_id(event.db);

            if (match_obj->games.size() >= tournament_obj.options.number_of_wins * 4)
            {
                wdump((match_obj->games.size()));
                return true;
            }

            for (unsigned i = 0; i < match_obj->players.size(); ++i)
            {
               // this guard is called before the winner of the current game factored in to our running totals,
               // so we must add the current game to our count
               uint32_t win_for_this_game = event.game.winners.find(match_obj->players[i]) != event.game.winners.end() ? 1 : 0;
               if (match_obj->number_of_wins[i] + win_for_this_game >= tournament_obj.options.number_of_wins)
                  return true;
            }
            return false;
         }

         bool match_is_a_buy(const initiate_match& event)
         {
            return match_obj->players.size() < 2;
         }
         
         void record_completed_game(const game_complete& event)
         {
            if (event.game.winners.empty())
               ++match_obj->number_of_ties;
            else
               for (unsigned i = 0; i < match_obj->players.size(); ++i)
                  if (event.game.winners.find(match_obj->players[i]) != event.game.winners.end())
                     ++match_obj->number_of_wins[i];
            match_obj->game_winners.emplace_back(event.game.winners);
         }

         void start_next_game(database& db)
         {
            fc_ilog(fc::logger::get("tournament"),
                    "In start_next_game");
            const game_object& game =
               db.create<game_object>( [&]( game_object& game ) {
                  game.match_id = match_obj->id;
                  game.players = match_obj->players;
                  game.game_details = rock_paper_scissors_game_details();
                  game.start_game(db, game.players);
               });
            match_obj->games.push_back(game.id);
         }

         void record_and_start_next_game(const game_complete& event)
         {
            record_completed_game(event);
            start_next_game(event.db);
         }

         // Transition table for tournament
         struct transition_table : mpl::vector<
         //    Start                          Event                     Next                         Action                Guard
         //  +-------------------------------+-------------------------+----------------------------+---------------------+----------------------+
         _row  < waiting_on_previous_matches, initiate_match,           match_in_progress >,
         g_row < waiting_on_previous_matches, initiate_match,           match_complete,                                    &x::match_is_a_buy >,
         //  +-------------------------------+-------------------------+----------------------------+---------------------+----------------------+
         a_row < match_in_progress,           game_complete,            match_in_progress,           &x::record_and_start_next_game >,
         row   < match_in_progress,           game_complete,            match_complete,              &x::record_completed_game, &x::was_final_game >
         //  +---------------------------+-----------------------------+----------------------------+---------------------+----------------------+
         > {};


         match_object* match_obj;
         match_state_machine_(match_object* match_obj) : match_obj(match_obj) {}
      };
      typedef msm::back::state_machine<match_state_machine_> match_state_machine;
   }

   class match_object::impl {
   public:
      match_state_machine state_machine;

      impl(match_object* self) : state_machine(self) {}
   };

   match_object::match_object() :
       number_of_ties(0),
       my(new impl(this))
   {
   }

   match_object::match_object(const match_object& rhs) : 
      graphene::db::abstract_object<match_object>(rhs),
      tournament_id(rhs.tournament_id),
      players(rhs.players),
      games(rhs.games),
      game_winners(rhs.game_winners),
      number_of_wins(rhs.number_of_wins),
      number_of_ties(rhs.number_of_ties),
      match_winners(rhs.match_winners),
      start_time(rhs.start_time),
      end_time(rhs.end_time),
      my(new impl(this))
   {
      my->state_machine = rhs.my->state_machine;
      my->state_machine.match_obj = this;
   }

   match_object& match_object::operator=(const match_object& rhs)
   {
      //graphene::db::abstract_object<match_object>::operator=(rhs);
      id = rhs.id;
      tournament_id = rhs.tournament_id;
      players = rhs.players;
      games = rhs.games;
      game_winners = rhs.game_winners;
      number_of_wins = rhs.number_of_wins;
      number_of_ties = rhs.number_of_ties;
      match_winners = rhs.match_winners;
      start_time = rhs.start_time;
      end_time = rhs.end_time;
      my->state_machine = rhs.my->state_machine;
      my->state_machine.match_obj = this;

      return *this;
   }

   match_object::~match_object()
   {
   }

   bool verify_match_state_constants()
   {
      unsigned error_count = 0;
      typedef msm::back::generate_state_set<match_state_machine::stt>::type all_states;
      static char const* filled_state_names[mpl::size<all_states>::value];
      mpl::for_each<all_states,boost::msm::wrap<mpl::placeholders::_1> >
         (msm::back::fill_state_names<match_state_machine::stt>(filled_state_names));
      for (unsigned i = 0; i < mpl::size<all_states>::value; ++i)
      {
         try
         {
            // this is an approximate test, the state name provided by typeinfo will be mangled, but should
            // at least contain the string we're looking for
            const char* fc_reflected_value_name = fc::reflector<match_state>::to_string((match_state)i);
            if (!strcmp(fc_reflected_value_name, filled_state_names[i]))
               fc_elog(fc::logger::get("match"),
                       "Error, state string mismatch between fc and boost::msm for int value ${int_value}: boost::msm -> ${boost_string}, fc::reflect -> ${fc_string}",
                       ("int_value", i)("boost_string", filled_state_names[i])("fc_string", fc_reflected_value_name));
         }
         catch (const fc::bad_cast_exception&)
         {
            fc_elog(fc::logger::get("match"),
                    "Error, no reflection for value ${int_value} in enum match_state",
                    ("int_value", i));
            ++error_count;
         }
      }

      return error_count == 0;
   }

   match_state match_object::get_state() const
   {
      static bool state_constants_are_correct = verify_match_state_constants();
      (void)&state_constants_are_correct;
      match_state state = (match_state)my->state_machine.current_state()[0];

      return state;  
   }

   void match_object::pack_impl(std::ostream& stream) const
   {
      boost::archive::binary_oarchive oa(stream, boost::archive::no_header|boost::archive::no_codecvt|boost::archive::no_xml_tag_checking);
      oa << my->state_machine;
   }

   void match_object::unpack_impl(std::istream& stream)
   {
      boost::archive::binary_iarchive ia(stream, boost::archive::no_header|boost::archive::no_codecvt|boost::archive::no_xml_tag_checking);
      ia >> my->state_machine;
   }

   void match_object::on_initiate_match(database& db)
   {
      my->state_machine.process_event(initiate_match(db));
   }

   void match_object::on_game_complete(database& db, const game_object& game)
   {
      my->state_machine.process_event(game_complete(db, game));
   }
#if 0
   game_id_type match_object::start_next_game(database& db, match_id_type match_id)
   {
      const game_object& game =
         db.create<game_object>( [&]( game_object& game ) {
            game.match_id = match_id;
            game.players = players;
         });
      return game.id;
   }
#endif

} } // graphene::chain

namespace fc { 
   // Manually reflect match_object to variant to properly reflect "state"
   void to_variant(const graphene::chain::match_object& match_obj, fc::variant& v, uint32_t max_depth)
   { try {
      fc_elog(fc::logger::get("tournament"), "In match_obj to_variant");
      elog("In match_obj to_variant");
      fc::mutable_variant_object o;
      o("id", fc::variant(match_obj.id, max_depth))
       ("tournament_id", fc::variant(match_obj.tournament_id, max_depth))
       ("players", fc::variant(match_obj.players, max_depth))
       ("games", fc::variant(match_obj.games, max_depth))
       ("game_winners", fc::variant(match_obj.game_winners, max_depth))
       ("number_of_wins", fc::variant(match_obj.number_of_wins, max_depth))
       ("number_of_ties", fc::variant(match_obj.number_of_ties, max_depth))
       ("match_winners", fc::variant(match_obj.match_winners, max_depth))
       ("start_time", fc::variant(match_obj.start_time, max_depth))
       ("end_time", fc::variant(match_obj.end_time, max_depth))
       ("state", fc::variant(match_obj.get_state(), max_depth));

      v = o;
   } FC_RETHROW_EXCEPTIONS(warn, "") }

   // Manually reflect match_object to variant to properly reflect "state"
   void from_variant(const fc::variant& v, graphene::chain::match_object& match_obj, uint32_t max_depth)
   { try {
      fc_elog(fc::logger::get("tournament"), "In match_obj from_variant");
      match_obj.id = v["id"].as<graphene::chain::match_id_type>( max_depth );
      match_obj.tournament_id = v["tournament_id"].as<graphene::chain::tournament_id_type>( max_depth );
      match_obj.players = v["players"].as<std::vector<graphene::chain::account_id_type> >( max_depth );
      match_obj.games = v["games"].as<std::vector<graphene::chain::game_id_type> >( max_depth );
      match_obj.game_winners = v["game_winners"].as<std::vector<flat_set<graphene::chain::account_id_type> > >( max_depth );
      match_obj.number_of_wins = v["number_of_wins"].as<std::vector<uint32_t> >( max_depth );
      match_obj.number_of_ties = v["number_of_ties"].as<uint32_t>( max_depth );
      match_obj.match_winners = v["match_winners"].as<flat_set<graphene::chain::account_id_type> >( max_depth );
      match_obj.start_time = v["start_time"].as<time_point_sec>( max_depth );
      match_obj.end_time = v["end_time"].as<optional<time_point_sec> >( max_depth );
      graphene::chain::match_state state = v["state"].as<graphene::chain::match_state>( max_depth );
      const_cast<int*>(match_obj.my->state_machine.current_state())[0] = (int)state;
   } FC_RETHROW_EXCEPTIONS(warn, "") }
} //end namespace fc


