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
#define BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS
#define BOOST_MPL_LIMIT_VECTOR_SIZE 30
#define BOOST_MPL_LIMIT_MAP_SIZE 30
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/betting_market_object.hpp>
#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/msm/back/tools.hpp>

namespace graphene { namespace chain {
   enum class event_state {
      upcoming,
      frozen_upcoming,
      in_progress,
      frozen_in_progress,
      finished,
      canceled,
      settled
   };
} }

FC_REFLECT_ENUM(graphene::chain::event_state, 
                (upcoming)
                (frozen_upcoming)
                (in_progress)
                (frozen_in_progress)
                (finished)
                (canceled)
                (settled))

namespace graphene { namespace chain {

   namespace msm = boost::msm;
   namespace mpl = boost::mpl;

   namespace 
   {

      // Events -- most events happen when the witnesses publish an event_update operation with a new
      // status, so if they publish an event with the status set to `frozen`, we'll generate a `frozen_event`
      struct upcoming_event 
      {
         database& db;
         upcoming_event(database& db) : db(db) {}
      };
      struct in_progress_event
      {
         database& db;
         in_progress_event(database& db) : db(db) {}
      };
      struct frozen_event 
      {
         database& db;
         frozen_event(database& db) : db(db) {}
      };
      struct finished_event 
      {
         database& db;
         finished_event(database& db) : db(db) {}
      };
      struct canceled_event
      {
         database& db;
         canceled_event(database& db) : db(db) {}
      };

      // event triggered when a betting market group in this event is resolved,
      // when we get this, check and see if all betting market groups are now
      // canceled/settled; if so, transition the event to canceled/settled,
      // otherwise remain in the current state
      struct betting_market_group_resolved_event
      {
         database& db;
         betting_market_group_id_type resolved_group;
         bool was_canceled;
         betting_market_group_resolved_event(database& db, betting_market_group_id_type resolved_group, bool was_canceled) : db(db), resolved_group(resolved_group), was_canceled(was_canceled) {}
      };

      // event triggered when a betting market group is closed.  When we get this, 
      // if all child betting market groups are closed, transition to finished
      struct betting_market_group_closed_event
      {
         database& db;
         betting_market_group_id_type closed_group;
         betting_market_group_closed_event(database& db, betting_market_group_id_type closed_group) : db(db), closed_group(closed_group) {}
      };

      // Events
      struct event_state_machine_ : public msm::front::state_machine_def<event_state_machine_>

      {
         // disable a few state machine features we don't use for performance
         typedef int no_exception_thrown;
         typedef int no_message_queue;

         // States
         struct upcoming : public msm::front::state<> {
            void on_entry(const betting_market_group_resolved_event& event, event_state_machine_& fsm) {} // transition to self
            void on_entry(const upcoming_event& event, event_state_machine_& fsm) {
               dlog("event ${id} -> upcoming", ("id", fsm.event_obj->id));
               auto& betting_market_group_index = event.db.get_index_type<betting_market_group_object_index>().indices().get<by_event_id>();
               for (const betting_market_group_object& betting_market_group : 
                    boost::make_iterator_range(betting_market_group_index.equal_range(fsm.event_obj->id)))
                  try
                  {
                     event.db.modify(betting_market_group, [&event](betting_market_group_object& betting_market_group_obj) {
                        betting_market_group_obj.on_upcoming_event(event.db);
                     });
                  }
                  catch (const graphene::chain::no_transition&)
                  {
                     // it's possible a betting market group has already been closed or canceled,
                     // in which case we can't freeze the group.  just ignore the exception
                  }
            }
         };
         struct in_progress : public msm::front::state<> {
            void on_entry(const betting_market_group_resolved_event& event, event_state_machine_& fsm) {} // transition to self
            void on_entry(const in_progress_event& event, event_state_machine_& fsm) {
               dlog("event ${id} -> in_progress", ("id", fsm.event_obj->id));
               auto& betting_market_group_index = event.db.get_index_type<betting_market_group_object_index>().indices().get<by_event_id>();
               for (const betting_market_group_object& betting_market_group : 
                    boost::make_iterator_range(betting_market_group_index.equal_range(fsm.event_obj->id)))
                  try
                  {
                     event.db.modify(betting_market_group, [&event](betting_market_group_object& betting_market_group_obj) {
                        betting_market_group_obj.on_in_play_event(event.db);
                     });
                  }
                  catch (const graphene::chain::no_transition&)
                  {
                     // it's possible a betting market group has already been closed or canceled,
                     // in which case we can't freeze the group.  just ignore the exception
                  }
            }
         };
         struct frozen_upcoming : public msm::front::state<> {
            void on_entry(const betting_market_group_resolved_event& event, event_state_machine_& fsm) {} // transition to self
            template <class Event>
            void on_entry(const Event& event, event_state_machine_& fsm) {
               dlog("event ${id} -> frozen_upcoming", ("id", fsm.event_obj->id));
            }
         };
         struct frozen_in_progress : public msm::front::state<> {
            void on_entry(const betting_market_group_resolved_event& event, event_state_machine_& fsm) {} // transition to self
            template <class Event>
            void on_entry(const Event& event, event_state_machine_& fsm) {
               dlog("event ${id} -> frozen_in_progress", ("id", fsm.event_obj->id));
            }
         };
         struct finished : public msm::front::state<> {
            template <class Event>
            void on_entry(const Event& event, event_state_machine_& fsm) {
               dlog("event ${id} -> finished", ("id", fsm.event_obj->id));
            }
         };
         struct settled : public msm::front::state<>{
            template <class Event>
            void on_entry(const Event& event, event_state_machine_& fsm) {
               dlog("event ${id} -> settled", ("id", fsm.event_obj->id));
            }
         };
         struct canceled : public msm::front::state<> {
            template <class Event>
            void on_entry(const Event& event, event_state_machine_& fsm) {
               dlog("event ${id} -> canceled", ("id", fsm.event_obj->id));
            }
         };

         // actions
         void record_whether_group_settled_or_canceled(const betting_market_group_resolved_event& event) {
            if (!event.was_canceled)
               event_obj->at_least_one_betting_market_group_settled = true;
         }

         void freeze_betting_market_groups(const frozen_event& event) {
            auto& betting_market_group_index = event.db.get_index_type<betting_market_group_object_index>().indices().get<by_event_id>();
            for (const betting_market_group_object& betting_market_group : 
                 boost::make_iterator_range(betting_market_group_index.equal_range(event_obj->id)))
            {
               try
               {
                  event.db.modify(betting_market_group, [&event](betting_market_group_object& betting_market_group_obj) {
                     betting_market_group_obj.on_frozen_event(event.db);
                  });
               }
               catch (const graphene::chain::no_transition&)
               {
                  // it's possible a betting market group has already been closed or canceled,
                  // in which case we can't freeze the group.  just ignore the exception
               }
            }
         }

         void close_all_betting_market_groups(const finished_event& event) {
            auto& betting_market_group_index = event.db.get_index_type<betting_market_group_object_index>().indices().get<by_event_id>();
            for (const betting_market_group_object& betting_market_group : 
                 boost::make_iterator_range(betting_market_group_index.equal_range(event_obj->id)))
            {
               try
               {
                  event.db.modify(betting_market_group, [&event](betting_market_group_object& betting_market_group_obj) {
                     betting_market_group_obj.on_closed_event(event.db, true);
                  });
               }
               catch (const graphene::chain::no_transition&)
               {
                  // it's possible a betting market group has already been closed or canceled,
                  // in which case we can't close the group.  just ignore the exception
               }
            }
         }

         void cancel_all_betting_market_groups(const canceled_event& event) {
            auto& betting_market_group_index = event.db.template get_index_type<betting_market_group_object_index>().indices().template get<by_event_id>();
            for (const betting_market_group_object& betting_market_group : 
                 boost::make_iterator_range(betting_market_group_index.equal_range(event_obj->id)))
               event.db.modify(betting_market_group, [&event](betting_market_group_object& betting_market_group_obj) {
                  betting_market_group_obj.on_canceled_event(event.db, true);
               });
         }

         // Guards
         bool all_betting_market_groups_are_closed(const betting_market_group_closed_event& event)
         {
            auto& betting_market_group_index = event.db.get_index_type<betting_market_group_object_index>().indices().get<by_event_id>();
            for (const betting_market_group_object& betting_market_group : 
                 boost::make_iterator_range(betting_market_group_index.equal_range(event_obj->id)))
               if (betting_market_group.id != event.closed_group)
               {
                  betting_market_group_status status = betting_market_group.get_status();
                  if (status != betting_market_group_status::closed  && 
                      status != betting_market_group_status::graded  && 
                      status != betting_market_group_status::re_grading  && 
                      status != betting_market_group_status::settled  && 
                      status != betting_market_group_status::canceled)
                     return false;
               }
            return true;
         }

         bool all_betting_market_groups_are_canceled(const betting_market_group_resolved_event& event)
         {
            // if the bmg that just resolved was settled, obviously all didn't cancel
            if (!event.was_canceled)
               return false;
            // if a previously-resolved group was settled, all didn't cancel
            if (event_obj->at_least_one_betting_market_group_settled)
               return false;
            auto& betting_market_group_index = event.db.get_index_type<betting_market_group_object_index>().indices().get<by_event_id>();
            for (const betting_market_group_object& betting_market_group : 
                 boost::make_iterator_range(betting_market_group_index.equal_range(event_obj->id)))
               if (betting_market_group.id != event.resolved_group)
                  if (betting_market_group.get_status() != betting_market_group_status::canceled)
                     return false;
            return true;
         }

         bool all_betting_market_groups_are_resolved(const betting_market_group_resolved_event& event)
         {
            if (!event.was_canceled)
               event_obj->at_least_one_betting_market_group_settled = true;

            auto& betting_market_group_index = event.db.get_index_type<betting_market_group_object_index>().indices().get<by_event_id>();
            for (const betting_market_group_object& betting_market_group : 
                 boost::make_iterator_range(betting_market_group_index.equal_range(event_obj->id))) {
               if (betting_market_group.id != event.resolved_group) {
                  betting_market_group_status status = betting_market_group.get_status();
                  if (status != betting_market_group_status::canceled && status != betting_market_group_status::settled)
                     return false;
               }
            }
            return true;
         }

         typedef upcoming initial_state;
         typedef event_state_machine_ x; // makes transition table cleaner

         // Transition table for tournament
         struct transition_table : mpl::vector<
         //    Start                 Event                         Next               Action               Guard
         //  +---------------------+-----------------------------+--------------------+--------------------------------+----------------------+
         _row < upcoming,           in_progress_event,            in_progress >,
         a_row< upcoming,           finished_event,               finished,           &x::close_all_betting_market_groups >,
         a_row< upcoming,           frozen_event,                 frozen_upcoming,    &x::freeze_betting_market_groups >,
         a_row< upcoming,           canceled_event,               canceled,           &x::cancel_all_betting_market_groups >,
         a_row< upcoming,           betting_market_group_resolved_event,upcoming,     &x::record_whether_group_settled_or_canceled >,
         g_row< upcoming,           betting_market_group_closed_event, finished,                                       &x::all_betting_market_groups_are_closed >,
         //  +---------------------+-----------------------------+--------------------+--------------------------------+----------------------+
         _row < frozen_upcoming,    upcoming_event,               upcoming >,
         _row < frozen_upcoming,    in_progress_event,            in_progress >,
         a_row< frozen_upcoming,    finished_event,               finished,           &x::close_all_betting_market_groups >,
         a_row< frozen_upcoming,    canceled_event,               canceled,           &x::cancel_all_betting_market_groups >,
         a_row< frozen_upcoming,    betting_market_group_resolved_event,frozen_upcoming, &x::record_whether_group_settled_or_canceled >,
         g_row< frozen_upcoming,    betting_market_group_closed_event, finished,                                       &x::all_betting_market_groups_are_closed >,
         //  +---------------------+-----------------------------+--------------------+--------------------------------+----------------------+
         a_row< in_progress,        frozen_event,                 frozen_in_progress, &x::freeze_betting_market_groups >,
         a_row< in_progress,        finished_event,               finished,           &x::close_all_betting_market_groups >,
         a_row< in_progress,        canceled_event,               canceled,           &x::cancel_all_betting_market_groups >,
         a_row< in_progress,        betting_market_group_resolved_event,in_progress,  &x::record_whether_group_settled_or_canceled >,
         g_row< in_progress,        betting_market_group_closed_event, finished,                                       &x::all_betting_market_groups_are_closed >,
         //  +---------------------+-----------------------------+--------------------+--------------------------------+----------------------+
         _row < frozen_in_progress, in_progress_event,            in_progress >,
         a_row< frozen_in_progress, finished_event,               finished,           &x::close_all_betting_market_groups >,
         a_row< frozen_in_progress, canceled_event,               canceled,           &x::cancel_all_betting_market_groups >,
         a_row< frozen_in_progress, betting_market_group_resolved_event,frozen_in_progress, &x::record_whether_group_settled_or_canceled >,
         g_row< frozen_in_progress, betting_market_group_closed_event, finished,                                       &x::all_betting_market_groups_are_closed >,
         //  +---------------------+-----------------------------+--------------------+--------------------------------+----------------------+
         a_row< finished,           canceled_event,               canceled,           &x::cancel_all_betting_market_groups >,
         g_row< finished,           betting_market_group_resolved_event,settled,                                       &x::all_betting_market_groups_are_resolved >,
         g_row< finished,           betting_market_group_resolved_event,canceled,                                      &x::all_betting_market_groups_are_canceled >
         > {};

         template <class Fsm,class Event>
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

         event_object* event_obj;
         event_state_machine_(event_object* event_obj) : event_obj(event_obj) {}
      };
      typedef msm::back::state_machine<event_state_machine_> event_state_machine;

   } // end anonymous namespace

   class event_object::impl {
   public:
      event_state_machine state_machine;

      impl(event_object* self) : state_machine(self) {}
   };

   event_object::event_object() :
      at_least_one_betting_market_group_settled(false),
      my(new impl(this))
   {
   }

   event_object::event_object(const event_object& rhs) : 
      graphene::db::abstract_object<event_object>(rhs),
      name(rhs.name),
      season(rhs.season),
      start_time(rhs.start_time),
      event_group_id(rhs.event_group_id),
      at_least_one_betting_market_group_settled(rhs.at_least_one_betting_market_group_settled),
      scores(rhs.scores),
      my(new impl(this))
   {
      my->state_machine = rhs.my->state_machine;
      my->state_machine.event_obj = this;
   }

   event_object& event_object::operator=(const event_object& rhs)
   {
      //graphene::db::abstract_object<event_object>::operator=(rhs);
      id = rhs.id;
      name = rhs.name;
      season = rhs.season;
      start_time = rhs.start_time;
      event_group_id = rhs.event_group_id;
      at_least_one_betting_market_group_settled = rhs.at_least_one_betting_market_group_settled;
      scores = rhs.scores;

      my->state_machine = rhs.my->state_machine;
      my->state_machine.event_obj = this;

      return *this;
   }

   event_object::~event_object()
   {
   }

   namespace {
   
      bool verify_event_status_constants()
      {
         unsigned error_count = 0;
         typedef msm::back::generate_state_set<event_state_machine::stt>::type all_states;
         static char const* filled_state_names[mpl::size<all_states>::value];
         mpl::for_each<all_states,boost::msm::wrap<mpl::placeholders::_1> >
            (msm::back::fill_state_names<event_state_machine::stt>(filled_state_names));
         for (unsigned i = 0; i < mpl::size<all_states>::value; ++i)
         {
            try
            {
               // this is an approximate test, the state name provided by typeinfo will be mangled, but should
               // at least contain the string we're looking for
               const char* fc_reflected_value_name = fc::reflector<event_state>::to_string((event_state)i);
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
                       "Error, no reflection for value ${int_value} in enum event_status",
                       ("int_value", i));
               ++error_count;
            }
         }
         if (error_count == 0)
            dlog("Event status constants are correct");
         else
            wlog("There were ${count} errors in the event status constants", ("count", error_count));
   
         return error_count == 0;
      }
   } // end anonymous namespace
   
   event_status event_object::get_status() const
   {
      static bool state_constants_are_correct = verify_event_status_constants();
      (void)&state_constants_are_correct;
      event_state state = (event_state)my->state_machine.current_state()[0];
      
      ddump((state));
   
      switch (state)
      {
         case event_state::upcoming:
            return event_status::upcoming;
         case event_state::frozen_upcoming:
         case event_state::frozen_in_progress:
            return event_status::frozen;
         case event_state::in_progress:
            return event_status::in_progress;
         case event_state::finished:
            return event_status::finished;
         case event_state::canceled:
            return event_status::canceled;
         case event_state::settled:
            return event_status::settled;
         default:
            FC_THROW("Unexpected event state");
      };
   }

   void event_object::pack_impl(std::ostream& stream) const
   {
      boost::archive::binary_oarchive oa(stream, boost::archive::no_header|boost::archive::no_codecvt|boost::archive::no_xml_tag_checking);
      oa << my->state_machine;
   }

   void event_object::unpack_impl(std::istream& stream)
   {
      boost::archive::binary_iarchive ia(stream, boost::archive::no_header|boost::archive::no_codecvt|boost::archive::no_xml_tag_checking);
      ia >> my->state_machine;
   }

   void event_object::on_upcoming_event(database& db)
   {
      my->state_machine.process_event(upcoming_event(db));
   }

   void event_object::on_in_progress_event(database& db)
   {
      my->state_machine.process_event(in_progress_event(db));
   }

   void event_object::on_frozen_event(database& db)
   {
      my->state_machine.process_event(frozen_event(db));
   }

   void event_object::on_finished_event(database& db)
   {
      my->state_machine.process_event(finished_event(db));
   }

   void event_object::on_canceled_event(database& db)
   {
      my->state_machine.process_event(canceled_event(db));
   }

   void event_object::on_betting_market_group_resolved(database& db, betting_market_group_id_type resolved_group, bool was_canceled)
   {
      my->state_machine.process_event(betting_market_group_resolved_event(db, resolved_group, was_canceled));
   }

   void event_object::on_betting_market_group_closed(database& db, betting_market_group_id_type closed_group)
   {
      my->state_machine.process_event(betting_market_group_closed_event(db, closed_group));
   }

   // These are the only statuses that can be explicitly set by witness operations.  The missing 
   // status, 'settled', is automatically set when all of the betting market groups have 
   // settled/canceled
   void event_object::dispatch_new_status(database& db, event_status new_status)
   {
      switch (new_status) {
      case event_status::upcoming: // by witnesses to unfreeze a frozen event
         on_upcoming_event(db);
         break;
      case event_status::in_progress:  // by witnesses when the event starts
         on_in_progress_event(db); 
         break;
      case event_status::frozen: // by witnesses when the event needs to be frozen
         on_frozen_event(db); 
         break;
      case event_status::finished: // by witnesses when the event is complete
         on_finished_event(db); 
         break;
      case event_status::canceled: // by witnesses to cancel the event
         on_canceled_event(db); 
         break;
      default:
         FC_THROW("Status ${new_status} cannot be explicitly set", ("new_status", new_status));
      }
   }

} } // graphene::chain

namespace fc { 
   // Manually reflect event_object to variant to properly reflect "state"
   void to_variant(const graphene::chain::event_object& event_obj, fc::variant& v, uint32_t max_depth)
   {
      fc::mutable_variant_object o;
      o("id", fc::variant(event_obj.id, max_depth))
       ("name", fc::variant(event_obj.name, max_depth))
       ("season", fc::variant(event_obj.season, max_depth))
       ("start_time", fc::variant(event_obj.start_time, max_depth))
       ("event_group_id", fc::variant(event_obj.event_group_id, max_depth))
       ("scores", fc::variant(event_obj.scores, max_depth))
       ("status", fc::variant(event_obj.get_status(), max_depth));

      v = o;
   }

   // Manually reflect event_object to variant to properly reflect "state"
   void from_variant(const fc::variant& v, graphene::chain::event_object& event_obj, uint32_t max_depth)
   {
      event_obj.id = v["id"].as<graphene::chain::event_id_type>( max_depth );
      event_obj.name = v["name"].as<graphene::chain::internationalized_string_type>( max_depth );
      event_obj.season = v["season"].as<graphene::chain::internationalized_string_type>( max_depth );
      event_obj.start_time = v["start_time"].as<optional<time_point_sec> >( max_depth );
      event_obj.event_group_id = v["event_group_id"].as<graphene::chain::event_group_id_type>( max_depth );
      event_obj.scores = v["scores"].as<std::vector<std::string>>( max_depth );
      graphene::chain::event_status status = v["status"].as<graphene::chain::event_status>( max_depth );
      const_cast<int*>(event_obj.my->state_machine.current_state())[0] = (int)status;
   }
} //end namespace fc


