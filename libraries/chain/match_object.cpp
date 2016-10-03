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
         vector<account_id_type> players;
         initiate_match(database& db, const vector<account_id_type>& players) :
            db(db), players(players)
         {}
      };

      struct game_complete 
      {
         database& db;
         game_id_type game_id;
         game_complete(database& db, game_id_type game_id) : db(db), game_id(game_id) {};
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
            void on_entry(const initiate_match& event, match_state_machine_& fsm)
            {
               match_object& match = *fsm.match_obj;
               match.players = event.players;
               match.start_time = event.db.head_block_time();

               fc_ilog(fc::logger::get("tournament"),
                       "Match ${id} is now in progress",
                       ("id", match.id));
            }
         };
         struct match_complete : public msm::front::state<>
         {
            void on_entry(const game_complete& event, match_state_machine_& fsm)
            {
               fc_ilog(fc::logger::get("tournament"),
                       "Match ${id} is complete",
                       ("id", fsm.match_obj->id));
            }
            void on_entry(const initiate_match& event, match_state_machine_& fsm)
            {
               match_object& match = *fsm.match_obj;
               fc_ilog(fc::logger::get("tournament"),
                       "Match ${id} is complete, it was a buy",
                       ("id", match));
               match.players = event.players;
               boost::copy(event.players, std::inserter(match.match_winners, match.match_winners.end()));
               match.start_time = event.db.head_block_time();
               match.end_time = event.db.head_block_time();
            }
         };
         typedef waiting_on_previous_matches initial_state;

         typedef match_state_machine_ x; // makes transition table cleaner
         
         // Guards
         bool was_final_game(const game_complete& event)
         {
            fc_ilog(fc::logger::get("tournament"),
                    "In was_final_game guard, returning ${value}",
                    ("value", false));// match_obj->registered_players == match_obj->options.number_of_players - 1));
            return false;
            //return match_obj->registered_players == match_obj->options.number_of_players - 1;
         }

         bool match_is_a_buy(const initiate_match& event)
         {
            return event.players.size() < 2;
         }
         
         void start_next_game(const game_complete& event)
         {
            fc_ilog(fc::logger::get("tournament"),
                    "In start_next_game action");
         }

         // Transition table for tournament
         struct transition_table : mpl::vector<
         //    Start                          Event                     Next                         Action                Guard
         //  +-------------------------------+-------------------------+----------------------------+---------------------+----------------------+
         _row  < waiting_on_previous_matches, initiate_match,           match_in_progress >,
         g_row < waiting_on_previous_matches, initiate_match,           match_complete,                                    &x::match_is_a_buy >,
         //  +-------------------------------+-------------------------+----------------------------+---------------------+----------------------+
         a_row < match_in_progress,           game_complete,            match_in_progress,           &x::start_next_game >,
         g_row < match_in_progress,           game_complete,            match_complete,                                    &x::was_final_game >
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
      my(new impl(this))
   {
   }

   match_object::match_object(const match_object& rhs) : 
      graphene::db::abstract_object<match_object>(rhs),
      players(rhs.players),
      games(rhs.games),
      game_winners(rhs.game_winners),
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
      players = rhs.players;
      games = rhs.games;
      game_winners = rhs.game_winners;
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

   void match_object::on_initiate_match(database& db, const vector<account_id_type>& players)
   {
      my->state_machine.process_event(initiate_match(db, players));
   }

   game_id_type match_object::start_next_game(database& db, match_id_type match_id)
   {
      const game_object& game =
         db.create<game_object>( [&]( game_object& game ) {
            game.match_id = match_id;
            game.players = players;
         });
      return game.id;
   }

} } // graphene::chain

namespace fc { 
   // Manually reflect match_object to variant to properly reflect "state"
   void to_variant(const graphene::chain::match_object& match_obj, fc::variant& v)
   {
      fc_elog(fc::logger::get("tournament"), "In match_obj to_variant");
      elog("In match_obj to_variant");
      fc::mutable_variant_object o;
      o("id", match_obj.id)
       ("players", match_obj.players)
       ("games", match_obj.games)
       ("game_winners", match_obj.game_winners)
       ("match_winners", match_obj.match_winners)
       ("start_time", match_obj.start_time)
       ("end_time", match_obj.end_time)
       ("state", match_obj.get_state());

      v = o;
   }

   // Manually reflect match_object to variant to properly reflect "state"
   void from_variant(const fc::variant& v, graphene::chain::match_object& match_obj)
   {
      fc_elog(fc::logger::get("tournament"), "In match_obj from_variant");
      match_obj.id = v["id"].as<graphene::chain::match_id_type>();
      match_obj.players = v["players"].as<std::vector<graphene::chain::account_id_type> >();
      match_obj.games = v["games"].as<std::vector<graphene::chain::game_id_type> >();
      match_obj.game_winners = v["game_winners"].as<std::vector<flat_set<graphene::chain::account_id_type> > >();
      match_obj.match_winners = v["match_winners"].as<flat_set<graphene::chain::account_id_type> >();
      match_obj.start_time = v["start_time"].as<time_point_sec>();
      match_obj.end_time = v["end_time"].as<optional<time_point_sec> >();
      graphene::chain::match_state state = v["state"].as<graphene::chain::match_state>();
      const_cast<int*>(match_obj.my->state_machine.current_state())[0] = (int)state;
   }
} //end namespace fc


