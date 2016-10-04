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
      struct initiate_game
      {
         database& db;
         vector<account_id_type> players;
         initiate_game(database& db, const vector<account_id_type>& players) :
            db(db), players(players)
         {}
      };

      struct game_complete 
      {
         database& db;
         game_id_type game_id;
         game_complete(database& db, game_id_type game_id) : db(db), game_id(game_id) {};
      };

      struct game_state_machine_ : public msm::front::state_machine_def<game_state_machine_>
      {
         // disable a few state machine features we don't use for performance
         typedef int no_exception_thrown;
         typedef int no_message_queue;

         // States
         struct waiting_on_previous_gamees : public msm::front::state<>{};
         struct game_in_progress : public msm::front::state<>
         {
            void on_entry(const initiate_game& event, game_state_machine_& fsm)
            {
               game_object& game = *fsm.game_obj;

               fc_ilog(fc::logger::get("tournament"),
                       "game ${id} is now in progress",
                       ("id", game.id));
            }
         };
         struct game_complete : public msm::front::state<>
         {
            void on_entry(const game_complete& event, game_state_machine_& fsm)
            {
               fc_ilog(fc::logger::get("tournament"),
                       "game ${id} is complete",
                       ("id", fsm.game_obj->id));
            }
            void on_entry(const initiate_game& event, game_state_machine_& fsm)
            {
               game_object& game = *fsm.game_obj;
               fc_ilog(fc::logger::get("tournament"),
                       "game ${id} is complete, it was a buy",
                       ("id", game));
            }
         };
         typedef waiting_on_previous_gamees initial_state;

         typedef game_state_machine_ x; // makes transition table cleaner
         
         // Guards
         bool was_final_game(const game_complete& event)
         {
            fc_ilog(fc::logger::get("tournament"),
                    "In was_final_game guard, returning ${value}",
                    ("value", false));// game_obj->registered_players == game_obj->options.number_of_players - 1));
            return false;
            //return game_obj->registered_players == game_obj->options.number_of_players - 1;
         }

         bool game_is_a_buy(const initiate_game& event)
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
         _row  < waiting_on_previous_gamees, initiate_game,           game_in_progress >,
         g_row < waiting_on_previous_gamees, initiate_game,           game_complete,                                    &x::game_is_a_buy >,
         //  +-------------------------------+-------------------------+----------------------------+---------------------+----------------------+
         a_row < game_in_progress,           game_complete,            game_in_progress,           &x::start_next_game >,
         g_row < game_in_progress,           game_complete,            game_complete,                                    &x::was_final_game >
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
      players(rhs.players),
      winners(rhs.winners),
      game_details(rhs.game_details),
      my(new impl(this))
   {
      my->state_machine = rhs.my->state_machine;
      my->state_machine.game_obj = this;
   }

   game_object& game_object::operator=(const game_object& rhs)
   {
      //graphene::db::abstract_object<game_object>::operator=(rhs);
      id = rhs.id;
      players = rhs.players;
      winners = rhs.winners;
      game_details = rhs.game_details;
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
   void to_variant(const graphene::chain::game_object& game_obj, fc::variant& v)
   {
      fc_elog(fc::logger::get("tournament"), "In game_obj to_variant");
      elog("In game_obj to_variant");
      fc::mutable_variant_object o;
      o("id", game_obj.id)
       ("players", game_obj.players)
       ("winners", game_obj.winners)
       ("game_details", game_obj.game_details)
       ("state", game_obj.get_state());

      v = o;
   }

   // Manually reflect game_object to variant to properly reflect "state"
   void from_variant(const fc::variant& v, graphene::chain::game_object& game_obj)
   {
      fc_elog(fc::logger::get("tournament"), "In game_obj from_variant");
      game_obj.id = v["id"].as<graphene::chain::game_id_type>();
      game_obj.players = v["players"].as<std::vector<graphene::chain::account_id_type> >();
      game_obj.winners = v["winners"].as<flat_set<graphene::chain::account_id_type> >();
      game_obj.game_details = v["game_details"].as<graphene::chain::game_specific_details>();
      graphene::chain::game_state state = v["state"].as<graphene::chain::game_state>();
      const_cast<int*>(game_obj.my->state_machine.current_state())[0] = (int)state;
   }
} //end namespace fc


