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

#define DEFAULT_LOGGER "betting"
#include <graphene/chain/betting_market_object.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/database.hpp>
#include <boost/integer/common_factor_rt.hpp>

#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/msm/back/tools.hpp>

namespace graphene { namespace chain {
   enum class betting_market_group_state {
      upcoming,
      frozen_upcoming,
      in_play,
      frozen_in_play,
      closed,
      graded,
      canceled,
      settled
   };
} }

FC_REFLECT_ENUM(graphene::chain::betting_market_group_state, 
                (upcoming)
                (frozen_upcoming)
                (in_play)
                (frozen_in_play)
                (closed)
                (graded)
                (canceled)
                (settled))

namespace graphene { namespace chain {

namespace msm = boost::msm;
namespace mpl = boost::mpl;

// betting market object implementation
namespace 
{
   // Events -- most events happen when the witnesses publish an update operation with a new
   // status, so if they publish an event with the status set to `frozen`, we'll generate a `frozen_event`
   struct upcoming_event 
   {
      database& db;
      upcoming_event(database& db) : db(db) {}
   };
   struct frozen_event 
   {
      database& db;
      frozen_event(database& db) : db(db) {}
   };
   struct in_play_event 
   {
      database& db;
      in_play_event(database& db) : db(db) {}
   };
   struct closed_event
   {
      database& db;
      bool closed_by_event;
      closed_event(database& db, bool closed_by_event) : db(db), closed_by_event(closed_by_event) {}
   };
   struct graded_event
   {
      database& db;
      graded_event(database& db) : db(db) {}
   };
   struct re_grading_event
   {
      database& db;
      re_grading_event(database& db) : db(db) {}
   };
   struct settled_event
   {
      database& db;
      settled_event(database& db) : db(db) {}
   };
   struct canceled_event
   {
      database& db;

      // true if this was triggered by setting event to canceled state, 
      // false if this was triggered directly on this betting market group
      bool canceled_by_event;

      canceled_event(database& db, bool canceled_by_event = false) : db(db), canceled_by_event(canceled_by_event) {}
   };

   // Events
   struct betting_market_group_state_machine_ : public msm::front::state_machine_def<betting_market_group_state_machine_>
   {
      // disable a few state machine features we don't use for performance
      typedef int no_exception_thrown;
      typedef int no_message_queue;

      // States
      struct upcoming : public msm::front::state<>
      {
         template <class Event>
         void on_entry(const Event& event, betting_market_group_state_machine_& fsm) {
            dlog("betting market group ${id} -> upcoming", ("id", fsm.betting_market_group_obj->id));
            // when a betting market group goes from frozen -> upcoming, transition the markets from frozen -> unresolved
            auto& betting_market_index = event.db.template get_index_type<betting_market_object_index>().indices().template get<by_betting_market_group_id>();
            for (const betting_market_object& betting_market : 
                 boost::make_iterator_range(betting_market_index.equal_range(fsm.betting_market_group_obj->id)))
               try
               {
                  event.db.modify(betting_market, [&event](betting_market_object& betting_market) {
                     betting_market.on_unresolved_event(event.db);
                  });
               }
               catch (const graphene::chain::no_transition&)
               {
               }
         }
      };
      struct frozen_upcoming : public msm::front::state<>
      {
         template <class Event>
         void on_entry(const Event& event, betting_market_group_state_machine_& fsm) {
            dlog("betting market group ${id} -> frozen_upcoming", ("id", fsm.betting_market_group_obj->id));

            auto& betting_market_index = event.db.template get_index_type<betting_market_object_index>().indices().template get<by_betting_market_group_id>();
            for (const betting_market_object& betting_market : 
                 boost::make_iterator_range(betting_market_index.equal_range(fsm.betting_market_group_obj->id)))
               event.db.modify(betting_market, [&event](betting_market_object& betting_market) {
                  betting_market.on_frozen_event(event.db);
               });
         }
      };

      struct in_play : public msm::front::state<> {
         template <class Event>
         void on_entry(const Event& event, betting_market_group_state_machine_& fsm) {
            dlog("betting market group ${id} -> in_play", ("id", fsm.betting_market_group_obj->id));
            // when an event goes in-play, cancel all unmatched bets in its betting markets
            auto& betting_market_index = event.db.template get_index_type<betting_market_object_index>().indices().template get<by_betting_market_group_id>();
            for (const betting_market_object& betting_market : 
                 boost::make_iterator_range(betting_market_index.equal_range(fsm.betting_market_group_obj->id)))
               event.db.modify(betting_market, [&event](betting_market_object& betting_market) {
                  betting_market.cancel_all_unmatched_bets(event.db);
                  try {
                     event.db.modify(betting_market, [&event](betting_market_object& betting_market) {
                        betting_market.on_unresolved_event(event.db);
                     });
                  } catch (const graphene::chain::no_transition&) {
                     // if this wasn't a transition from frozen state, this wasn't necessary
                  }
               });
         }
      };
      struct frozen_in_play : public msm::front::state<> {
         template <class Event>
         void on_entry(const Event& event, betting_market_group_state_machine_& fsm) {
            dlog("betting market group ${id} -> frozen_in_play", ("id", fsm.betting_market_group_obj->id));
            auto& betting_market_index = event.db.template get_index_type<betting_market_object_index>().indices().template get<by_betting_market_group_id>();
            for (const betting_market_object& betting_market : 
                 boost::make_iterator_range(betting_market_index.equal_range(fsm.betting_market_group_obj->id)))
               event.db.modify(betting_market, [&event](betting_market_object& betting_market) {
                  betting_market.on_frozen_event(event.db);
               });
         }
      };
      struct closed : public msm::front::state<> {
         template <class Event>
         void on_entry(const Event& fsm_event, betting_market_group_state_machine_& fsm) {
            dlog("betting market group ${id} -> closed", ("id", fsm.betting_market_group_obj->id));
            auto& betting_market_index = fsm_event.db.template get_index_type<betting_market_object_index>().indices().template get<by_betting_market_group_id>();
            for (const betting_market_object& betting_market : 
                 boost::make_iterator_range(betting_market_index.equal_range(fsm.betting_market_group_obj->id)))
               fsm_event.db.modify(betting_market, [&fsm_event](betting_market_object& betting_market) {
                  betting_market.cancel_all_unmatched_bets(fsm_event.db);
                  betting_market.on_closed_event(fsm_event.db);
               });

            // then notify the event that this betting market is now closed so it can change its status accordingly
            if (!fsm_event.closed_by_event) {
               const event_object& event = fsm.betting_market_group_obj->event_id(fsm_event.db);
               fsm_event.db.modify(event, [&fsm_event,&fsm](event_object& event_obj) {
                  event_obj.on_betting_market_group_closed(fsm_event.db, fsm.betting_market_group_obj->id);
               });
            }
         }
      };
      struct graded : public msm::front::state<> {
         template <class Event>
         void on_entry(const Event& event, betting_market_group_state_machine_& fsm) {
            dlog("betting market group ${id} -> graded", ("id", fsm.betting_market_group_obj->id));
            fsm.betting_market_group_obj->settling_time = event.db.head_block_time() + fsm.betting_market_group_obj->delay_before_settling;
            dlog("grading complete, setting settling time to ${settling_time}", ("settling_time", fsm.betting_market_group_obj->settling_time));
         }
      };
      struct re_grading : public msm::front::state<> {
         template <class Event>
         void on_entry(const Event& event, betting_market_group_state_machine_& fsm) {
            dlog("betting market group ${id} -> re_grading", ("id", fsm.betting_market_group_obj->id));
         }
      };
      struct settled : public msm::front::state<> {
         template <class Event>
         void on_entry(const Event& fsm_event, betting_market_group_state_machine_& fsm) {
            dlog("betting market group ${id} -> settled", ("id", fsm.betting_market_group_obj->id));
            // TODO: what triggers this?  I guess it will be the blockchain when its settling delay expires.  So in that case, it should
            // trigger the payout in the betting markets
            auto& betting_market_index = fsm_event.db.template get_index_type<betting_market_object_index>().indices().template get<by_betting_market_group_id>();
            for (const betting_market_object& betting_market : 
                 boost::make_iterator_range(betting_market_index.equal_range(fsm.betting_market_group_obj->id)))
               fsm_event.db.modify(betting_market, [&fsm_event](betting_market_object& betting_market) {
                  betting_market.on_settled_event(fsm_event.db);
               });

            // then notify the event that this betting market is now resolved so it can change its status accordingly
            const event_object& event = fsm.betting_market_group_obj->event_id(fsm_event.db);
            fsm_event.db.modify(event, [&fsm_event,&fsm](event_object& event_obj) {
               event_obj.on_betting_market_group_resolved(fsm_event.db, fsm.betting_market_group_obj->id, false);
            });
         }
      };
      struct canceled : public msm::front::state<>{
         void on_entry(const canceled_event& fsm_event, betting_market_group_state_machine_& fsm) {
            dlog("betting market group ${id} -> canceled", ("id", fsm.betting_market_group_obj->id));
            auto& betting_market_index = fsm_event.db.get_index_type<betting_market_object_index>().indices().get<by_betting_market_group_id>();
            auto betting_markets_in_group = boost::make_iterator_range(betting_market_index.equal_range(fsm.betting_market_group_obj->id));

            for (const betting_market_object& betting_market : betting_markets_in_group)
               fsm_event.db.modify(betting_market, [&fsm_event](betting_market_object& betting_market_obj) {
                  betting_market_obj.on_canceled_event(fsm_event.db);
               });

            if (!fsm_event.canceled_by_event) {
               const event_object& event = fsm.betting_market_group_obj->event_id(fsm_event.db);
               fsm_event.db.modify(event, [&fsm_event,&fsm](event_object& event_obj) {
                  event_obj.on_betting_market_group_resolved(fsm_event.db, fsm.betting_market_group_obj->id, true);
               });
            }

            fsm.betting_market_group_obj->settling_time = fsm_event.db.head_block_time();
            dlog("cancel complete, setting settling time to ${settling_time}", ("settling_time", fsm.betting_market_group_obj->settling_time));
         }
      };

      typedef upcoming initial_state;

      // actions
      void cancel_all_unmatched_bets(const in_play_event& event) {
         event.db.cancel_all_unmatched_bets_on_betting_market_group(*betting_market_group_obj);
      }

      // guards
      bool in_play_is_allowed(const in_play_event& event) {
         return !betting_market_group_obj->never_in_play;
      }

      typedef betting_market_group_state_machine_ x; // makes transition table cleaner

      // Transition table for betting market
      struct transition_table : mpl::vector<
      //    Start              Event              Next                  Action                         Guard
      //  +-------------------+------------------+---------------------+------------------------------+----------------------+
      _row < upcoming,         frozen_event,      frozen_upcoming >,
      row  < upcoming,         in_play_event,     in_play,              &x::cancel_all_unmatched_bets, &x::in_play_is_allowed >,
      _row < upcoming,         closed_event,      closed >,
      _row < upcoming,         canceled_event,    canceled >,
      //  +-------------------+------------------+---------------------+------------------------------+----------------------+
      _row < frozen_upcoming,  upcoming_event,    upcoming >,
      _row < frozen_upcoming,  in_play_event,     upcoming >,
      row  < frozen_upcoming,  in_play_event,     in_play,              &x::cancel_all_unmatched_bets, &x::in_play_is_allowed >,
      _row < frozen_upcoming,  closed_event,      closed >,
      _row < frozen_upcoming,  canceled_event,    canceled >,
      //  +-------------------+------------------+---------------------+------------------------------+----------------------+
      _row < in_play,          frozen_event,      frozen_in_play >,
      _row < in_play,          closed_event,      closed >,
      _row < in_play,          canceled_event,    canceled >,
      //  +-------------------+------------------+---------------------+------------------------------+----------------------+
      _row < frozen_in_play,   in_play_event,     in_play >,
      _row < frozen_in_play,   closed_event,      closed >,
      _row < frozen_in_play,   canceled_event,    canceled >,
      //  +-------------------+------------------+---------------------+------------------------------+----------------------+
      _row < closed,           graded_event,      graded >,
      _row < closed,           canceled_event,    canceled >,
      //  +-------------------+------------------+---------------------+------------------------------+----------------------+
      //_row < graded          re_grading_event,  re_grading >,
      _row < graded,           settled_event,     settled >,
      _row < graded,           canceled_event,    canceled >
      //  +-------------------+------------------+---------------------+------------------------------+----------------------+
      //_row < re_grading,     graded_event,      graded >,
      //_row < re_grading,     canceled_event,    canceled >
      //  +-------------------+------------------+---------------------+------------------------------+----------------------+
      > {};

      template <class Fsm, class Event>
      void no_transition(Event const& e, Fsm&, int state)
      {
         FC_THROW_EXCEPTION(graphene::chain::no_transition, "No transition");
      }

      template <class Fsm>
      void no_transition(canceled_event const& e, Fsm&, int state)
      {
         //ignore transitions from settled to canceled state
         //and from canceled to canceled state
      }
       
      betting_market_group_object* betting_market_group_obj;
      betting_market_group_state_machine_(betting_market_group_object* betting_market_group_obj) : betting_market_group_obj(betting_market_group_obj) {}
   };
   typedef msm::back::state_machine<betting_market_group_state_machine_> betting_market_group_state_machine;

} // end anonymous namespace

class betting_market_group_object::impl {
public:
   betting_market_group_state_machine state_machine;

   impl(betting_market_group_object* self) : state_machine(self) {}
};

betting_market_group_object::betting_market_group_object() :
   my(new impl(this))
{
   dlog("betting_market_group_object ctor");
}

betting_market_group_object::betting_market_group_object(const betting_market_group_object& rhs) : 
   graphene::db::abstract_object<betting_market_group_object>(rhs),
   description(rhs.description),
   event_id(rhs.event_id),
   rules_id(rhs.rules_id),
   asset_id(rhs.asset_id),
   total_matched_bets_amount(rhs.total_matched_bets_amount),
   never_in_play(rhs.never_in_play),
   delay_before_settling(rhs.delay_before_settling),
   settling_time(rhs.settling_time),
   my(new impl(this))
{
   my->state_machine = rhs.my->state_machine;
   my->state_machine.betting_market_group_obj = this;
}

betting_market_group_object& betting_market_group_object::operator=(const betting_market_group_object& rhs)
{
   //graphene::db::abstract_object<betting_market_group_object>::operator=(rhs);
   id = rhs.id;
   description = rhs.description;
   event_id = rhs.event_id;
   rules_id = rhs.rules_id;
   asset_id = rhs.asset_id;
   total_matched_bets_amount = rhs.total_matched_bets_amount;
   never_in_play = rhs.never_in_play;
   delay_before_settling = rhs.delay_before_settling;
   settling_time = rhs.settling_time;

   my->state_machine = rhs.my->state_machine;
   my->state_machine.betting_market_group_obj = this;

   return *this;
}

betting_market_group_object::~betting_market_group_object()
{
}

namespace {

   bool verify_betting_market_group_status_constants()
   {
      unsigned error_count = 0;
      typedef msm::back::generate_state_set<betting_market_group_state_machine::stt>::type all_states;
      static char const* filled_state_names[mpl::size<all_states>::value];
      mpl::for_each<all_states,boost::msm::wrap<mpl::placeholders::_1> >
         (msm::back::fill_state_names<betting_market_group_state_machine::stt>(filled_state_names));
      for (unsigned i = 0; i < mpl::size<all_states>::value; ++i)
      {
         try
         {
            // this is an approximate test, the state name provided by typeinfo will be mangled, but should
            // at least contain the string we're looking for
            const char* fc_reflected_value_name = fc::reflector<betting_market_group_state>::to_string((betting_market_group_state)i);
            if (!strstr(filled_state_names[i], fc_reflected_value_name))
            {
               fc_elog(fc::logger::get("default"),
                       "Error, state string mismatch between fc and boost::msm for int value ${int_value}: boost::msm -> ${boost_string}, fc::reflect -> ${fc_string}",
                       ("int_value", i)("boost_string", filled_state_names[i])("fc_string", fc_reflected_value_name));
               ++error_count;
            }
         }
         catch (const fc::bad_cast_exception&)
         {
            fc_elog(fc::logger::get("default"),
                    "Error, no reflection for value ${int_value} in enum betting_market_group_status",
                    ("int_value", i));
            ++error_count;
         }
      }
      if (error_count == 0)
         dlog("Betting market group status constants are correct");
      else
         wlog("There were ${count} errors in the betting market group status constants", ("count", error_count));

      return error_count == 0;
   }
} // end anonymous namespace

betting_market_group_status betting_market_group_object::get_status() const
{
   static bool state_constants_are_correct = verify_betting_market_group_status_constants();
   (void)&state_constants_are_correct;
   betting_market_group_state state = (betting_market_group_state)my->state_machine.current_state()[0];
   
   ddump((state));

   switch (state)
   {
      case betting_market_group_state::upcoming:
         return betting_market_group_status::upcoming;
      case betting_market_group_state::frozen_upcoming:
         return betting_market_group_status::frozen;
      case betting_market_group_state::in_play:
         return betting_market_group_status::in_play;
      case betting_market_group_state::frozen_in_play:
         return betting_market_group_status::frozen;
      case betting_market_group_state::closed:
         return betting_market_group_status::closed;
      case betting_market_group_state::graded:
         return betting_market_group_status::graded;
      case betting_market_group_state::canceled:
         return betting_market_group_status::canceled;
      case betting_market_group_state::settled:
         return betting_market_group_status::settled;
      default:
         FC_THROW("Unexpected betting market group state");
   };
}

void betting_market_group_object::pack_impl(std::ostream& stream) const
{
   boost::archive::binary_oarchive oa(stream, boost::archive::no_header|boost::archive::no_codecvt|boost::archive::no_xml_tag_checking);
   oa << my->state_machine;
}

void betting_market_group_object::unpack_impl(std::istream& stream)
{
   boost::archive::binary_iarchive ia(stream, boost::archive::no_header|boost::archive::no_codecvt|boost::archive::no_xml_tag_checking);
   ia >> my->state_machine;
}

void betting_market_group_object::on_upcoming_event(database& db)
{
   my->state_machine.process_event(upcoming_event(db));
}

void betting_market_group_object::on_in_play_event(database& db)
{
   my->state_machine.process_event(in_play_event(db));
}

void betting_market_group_object::on_frozen_event(database& db)
{
   my->state_machine.process_event(frozen_event(db));
}

void betting_market_group_object::on_closed_event(database& db, bool closed_by_event)
{
   my->state_machine.process_event(closed_event(db, closed_by_event));
}

void betting_market_group_object::on_graded_event(database& db)
{
   my->state_machine.process_event(graded_event(db));
}

void betting_market_group_object::on_re_grading_event(database& db)
{
   my->state_machine.process_event(re_grading_event(db));
}

void betting_market_group_object::on_settled_event(database& db)
{
   my->state_machine.process_event(settled_event(db));
}

void betting_market_group_object::on_canceled_event(database& db, bool canceled_by_event)
{
   my->state_machine.process_event(canceled_event(db, canceled_by_event));
}

// These are the only statuses that can be explicitly set by witness operations.
// Other states can only be reached indirectly (i.e., settling happens a fixed
// delay after grading)
void betting_market_group_object::dispatch_new_status(database& db, betting_market_group_status new_status)
{ 
   switch (new_status) {
   case betting_market_group_status::upcoming: // by witnesses to unfreeze a bmg
      on_upcoming_event(db); 
      break;
   case betting_market_group_status::in_play: // by witnesses to make a bmg in-play
      on_in_play_event(db); 
      break;
   case betting_market_group_status::closed: // by witnesses to close a bmg
      on_closed_event(db, false); 
      break;
   case betting_market_group_status::frozen: // by witnesses to freeze a bmg
      on_frozen_event(db); 
      break;
   case betting_market_group_status::canceled: // by witnesses to cancel a bmg
      on_canceled_event(db, false); 
      break;
   default:
      FC_THROW("The status ${new_status} cannot be set directly", ("new_status", new_status));
   }
}


} } // graphene::chain

namespace fc { 
   // Manually reflect betting_market_group_object to variant to properly reflect "state"
   void to_variant(const graphene::chain::betting_market_group_object& betting_market_group_obj, fc::variant& v, uint32_t max_depth)
   {
      fc::mutable_variant_object o;
      o("id", fc::variant(betting_market_group_obj.id, max_depth))
       ("description", fc::variant(betting_market_group_obj.description, max_depth))
       ("event_id", fc::variant(betting_market_group_obj.event_id, max_depth))
       ("rules_id", fc::variant(betting_market_group_obj.rules_id, max_depth))
       ("asset_id", fc::variant(betting_market_group_obj.asset_id, max_depth))
       ("total_matched_bets_amount", fc::variant(betting_market_group_obj.total_matched_bets_amount, max_depth))
       ("never_in_play", fc::variant(betting_market_group_obj.never_in_play, max_depth))
       ("delay_before_settling", fc::variant(betting_market_group_obj.delay_before_settling, max_depth))
       ("settling_time", fc::variant(betting_market_group_obj.settling_time, max_depth))
       ("status", fc::variant(betting_market_group_obj.get_status(), max_depth));

      v = o;
   }

   // Manually reflect betting_market_group_object to variant to properly reflect "state"
   void from_variant(const fc::variant& v, graphene::chain::betting_market_group_object& betting_market_group_obj, uint32_t max_depth)
   {
      betting_market_group_obj.id = v["id"].as<graphene::chain::betting_market_group_id_type>( max_depth );
      betting_market_group_obj.description = v["description"].as<graphene::chain::internationalized_string_type>( max_depth );
      betting_market_group_obj.event_id = v["event_id"].as<graphene::chain::event_id_type>( max_depth );
      betting_market_group_obj.asset_id = v["asset_id"].as<graphene::chain::asset_id_type>( max_depth );
      betting_market_group_obj.total_matched_bets_amount = v["total_matched_bets_amount"].as<graphene::chain::share_type>( max_depth );
      betting_market_group_obj.never_in_play = v["never_in_play"].as<bool>( max_depth );
      betting_market_group_obj.delay_before_settling = v["delay_before_settling"].as<uint32_t>( max_depth );
      betting_market_group_obj.settling_time = v["settling_time"].as<fc::optional<fc::time_point_sec>>( max_depth );
      graphene::chain::betting_market_group_status status = v["status"].as<graphene::chain::betting_market_group_status>( max_depth );
      const_cast<int*>(betting_market_group_obj.my->state_machine.current_state())[0] = (int)status;
   }
} //end namespace fc

