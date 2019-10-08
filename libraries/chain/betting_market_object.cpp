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
#include <graphene/chain/database.hpp>
#include <boost/integer/common_factor_rt.hpp>

#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/msm/back/tools.hpp>

namespace graphene { namespace chain {
   enum class betting_market_state {
      unresolved,
      frozen,
      closed,
      graded,
      canceled,
      settled
   };
} }
FC_REFLECT_ENUM(graphene::chain::betting_market_state, 
                (unresolved)
                (frozen)
                (closed)
                (graded)
                (canceled)
                (settled))


namespace graphene { namespace chain {

namespace msm = boost::msm;
namespace mpl = boost::mpl;

/* static */ share_type bet_object::get_approximate_matching_amount(share_type bet_amount, bet_multiplier_type backer_multiplier, bet_type back_or_lay, bool round_up /* = false */)
{
   fc::uint128_t amount_to_match_128 = bet_amount.value;

   if (back_or_lay == bet_type::back)
   {
       amount_to_match_128 *= backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION;
       if (round_up)
         amount_to_match_128 += GRAPHENE_BETTING_ODDS_PRECISION - 1;
       amount_to_match_128 /= GRAPHENE_BETTING_ODDS_PRECISION;
   }
   else
   {
       amount_to_match_128 *= GRAPHENE_BETTING_ODDS_PRECISION;
       if (round_up)
         amount_to_match_128 += backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION - 1;
       amount_to_match_128 /= backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION;
   }
   return amount_to_match_128.to_uint64();
}

share_type bet_object::get_approximate_matching_amount(bool round_up /* = false */) const
{
   return get_approximate_matching_amount(amount_to_bet.amount, backer_multiplier, back_or_lay, round_up);
}

/* static */ share_type bet_object::get_exact_matching_amount(share_type bet_amount, bet_multiplier_type backer_multiplier, bet_type back_or_lay)
{
   share_type back_ratio;
   share_type lay_ratio;
   std::tie(back_ratio, lay_ratio) = get_ratio(backer_multiplier);
   if (back_or_lay == bet_type::back)
      return bet_amount / back_ratio * lay_ratio;
   else
      return bet_amount / lay_ratio * back_ratio;
}

share_type bet_object::get_exact_matching_amount() const
{
   return get_exact_matching_amount(amount_to_bet.amount, backer_multiplier, back_or_lay);
}

/* static */ std::pair<share_type, share_type> bet_object::get_ratio(bet_multiplier_type backer_multiplier)
{
   share_type gcd = boost::integer::gcd(GRAPHENE_BETTING_ODDS_PRECISION, static_cast<int32_t>(backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION));
   return std::make_pair(GRAPHENE_BETTING_ODDS_PRECISION / gcd, (backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION) / gcd);
}

std::pair<share_type, share_type> bet_object::get_ratio() const
{
   return get_ratio(backer_multiplier);
}

share_type bet_object::get_minimum_matchable_amount() const
{
   share_type gcd = boost::integer::gcd(GRAPHENE_BETTING_ODDS_PRECISION, static_cast<int32_t>(backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION));
   return (back_or_lay == bet_type::back ? GRAPHENE_BETTING_ODDS_PRECISION : backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION) / gcd;
}

share_type bet_object::get_minimum_matching_amount() const
{
   share_type gcd = boost::integer::gcd(GRAPHENE_BETTING_ODDS_PRECISION, static_cast<int32_t>(backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION));
   return (back_or_lay == bet_type::lay ? GRAPHENE_BETTING_ODDS_PRECISION : backer_multiplier - GRAPHENE_BETTING_ODDS_PRECISION) / gcd;
}


share_type betting_market_position_object::reduce()
{
   share_type additional_not_cancel_balance = std::min(pay_if_payout_condition, pay_if_not_payout_condition);
   if (additional_not_cancel_balance == 0)
      return 0;
   pay_if_payout_condition -= additional_not_cancel_balance;
   pay_if_not_payout_condition -= additional_not_cancel_balance;
   pay_if_not_canceled += additional_not_cancel_balance;
   
   share_type immediate_winnings = std::min(pay_if_canceled, pay_if_not_canceled);
   if (immediate_winnings == 0)
      return 0;
   pay_if_canceled -= immediate_winnings;
   pay_if_not_canceled -= immediate_winnings;
   return immediate_winnings;
}

// betting market object implementation
namespace 
{
   // Events -- most events happen when the witnesses publish an update operation with a new
   // status, so if they publish an event with the status set to `frozen`, we'll generate a `frozen_event`
   struct unresolved_event 
   {
      database& db;
      unresolved_event(database& db) : db(db) {}
   };
   struct frozen_event 
   {
      database& db;
      frozen_event(database& db) : db(db) {}
   };
   struct closed_event 
   {
      database& db;
      closed_event(database& db) : db(db) {}
   };
   struct graded_event
   {
      database& db;
      betting_market_resolution_type new_grading;
      graded_event(database& db, betting_market_resolution_type new_grading) : db(db), new_grading(new_grading) {}
   };
   struct settled_event
   {
      database& db;
      settled_event(database& db) : db(db) {}
   };
   struct canceled_event
   {
      database& db;
      canceled_event(database& db) : db(db) {}
   };

   // Events
   struct betting_market_state_machine_ : public msm::front::state_machine_def<betting_market_state_machine_>
   {
      // disable a few state machine features we don't use for performance
      typedef int no_exception_thrown;
      typedef int no_message_queue;

      // States
      struct unresolved : public msm::front::state<>{
         template <class Event>
         void on_entry(const Event& event, betting_market_state_machine_& fsm) {
            dlog("betting market ${id} -> unresolved", ("id", fsm.betting_market_obj->id));
         }
      };
      struct frozen : public msm::front::state<>{
         template <class Event>
         void on_entry(const Event& event, betting_market_state_machine_& fsm) {
            dlog("betting market ${id} -> frozen", ("id", fsm.betting_market_obj->id));
         }
      };
      struct closed : public msm::front::state<>{
         template <class Event>
         void on_entry(const Event& event, betting_market_state_machine_& fsm) {
            dlog("betting market ${id} -> closed", ("id", fsm.betting_market_obj->id));
         }
      };
      struct graded : public msm::front::state<>{
         void on_entry(const graded_event& event, betting_market_state_machine_& fsm) {
            dlog("betting market ${id} -> graded", ("id", fsm.betting_market_obj->id));
            fsm.betting_market_obj->resolution = event.new_grading;
         }
      };
      struct settled : public msm::front::state<>{
         template <class Event>
         void on_entry(const Event& event, betting_market_state_machine_& fsm) {
            dlog("betting market ${id} -> settled", ("id", fsm.betting_market_obj->id));
         }
      };
      struct canceled : public msm::front::state<>{
         void on_entry(const canceled_event& event, betting_market_state_machine_& fsm) {
            dlog("betting market ${id} -> canceled", ("id", fsm.betting_market_obj->id));
            fsm.betting_market_obj->resolution = betting_market_resolution_type::cancel;
            fsm.betting_market_obj->cancel_all_bets(event.db);
         }
      };

      typedef unresolved initial_state;
      typedef betting_market_state_machine_ x; // makes transition table cleaner


      // Transition table for betting market
      struct transition_table : mpl::vector<
      //    Start                       Event                         Next                       Action               Guard
      //  +---------------------------+-----------------------------+----------------------------+---------------------+----------------------+
      _row < unresolved,               frozen_event,                 frozen >,
      _row < unresolved,               closed_event,                 closed >,
      _row < unresolved,               canceled_event,               canceled >,
      //  +---------------------------+-----------------------------+----------------------------+---------------------+----------------------+
      _row < frozen,                   unresolved_event,             unresolved >,
      _row < frozen,                   closed_event,                 closed >,
      _row < frozen,                   canceled_event,               canceled >,
      //  +---------------------------+-----------------------------+----------------------------+---------------------+----------------------+
      _row < closed,                   graded_event,                 graded >,
      _row < closed,                   canceled_event,               canceled >,
      //  +---------------------------+-----------------------------+----------------------------+---------------------+----------------------+
      _row < graded,                   settled_event,                settled >,
      _row < graded,                   canceled_event,               canceled >
      //  +---------------------------+-----------------------------+----------------------------+---------------------+----------------------+
      > {};

      template <class Fsm, class Event>
      void no_transition(Event const& e, Fsm& ,int state)
      {
         FC_THROW_EXCEPTION(graphene::chain::no_transition, "No transition");
      }
       
      template <class Fsm>
      void no_transition(canceled_event const& e, Fsm&, int state)
      {
         //ignore transitions from settled to canceled state
         //and from canceled to canceled state
      }

      betting_market_object* betting_market_obj;
      betting_market_state_machine_(betting_market_object* betting_market_obj) : betting_market_obj(betting_market_obj) {}
   };
   typedef msm::back::state_machine<betting_market_state_machine_> betting_market_state_machine;

} // end anonymous namespace

class betting_market_object::impl {
public:
   betting_market_state_machine state_machine;

   impl(betting_market_object* self) : state_machine(self) {}
};

betting_market_object::betting_market_object() :
   my(new impl(this))
{
}

betting_market_object::betting_market_object(const betting_market_object& rhs) : 
   graphene::db::abstract_object<betting_market_object>(rhs),
   group_id(rhs.group_id),
   description(rhs.description),
   payout_condition(rhs.payout_condition),
   resolution(rhs.resolution),
   my(new impl(this))
{
   my->state_machine = rhs.my->state_machine;
   my->state_machine.betting_market_obj = this;
}

betting_market_object& betting_market_object::operator=(const betting_market_object& rhs)
{
   //graphene::db::abstract_object<betting_market_object>::operator=(rhs);
   id = rhs.id;
   group_id = rhs.group_id;
   description = rhs.description;
   payout_condition = rhs.payout_condition;
   resolution = rhs.resolution;

   my->state_machine = rhs.my->state_machine;
   my->state_machine.betting_market_obj = this;

   return *this;
}

betting_market_object::~betting_market_object()
{
}

namespace {


   bool verify_betting_market_status_constants()
   {
      unsigned error_count = 0;
      typedef msm::back::generate_state_set<betting_market_state_machine::stt>::type all_states;
      static char const* filled_state_names[mpl::size<all_states>::value];
      mpl::for_each<all_states,boost::msm::wrap<mpl::placeholders::_1> >
         (msm::back::fill_state_names<betting_market_state_machine::stt>(filled_state_names));
      for (unsigned i = 0; i < mpl::size<all_states>::value; ++i)
      {
         try
         {
            // this is an approximate test, the state name provided by typeinfo will be mangled, but should
            // at least contain the string we're looking for
            const char* fc_reflected_value_name = fc::reflector<betting_market_state>::to_string((betting_market_state)i);
            if (!strstr(filled_state_names[i], fc_reflected_value_name))
            {
               fc_elog(fc::logger::get("default"),
                       "Error, state string mismatch between fc and boost::msm for int value ${int_value}: "
                       "boost::msm -> ${boost_string}, fc::reflect -> ${fc_string}",
                       ("int_value", i)("boost_string", filled_state_names[i])("fc_string", fc_reflected_value_name));
               ++error_count;
            }
         }
         catch (const fc::bad_cast_exception&)
         {
            fc_elog(fc::logger::get("default"), 
                    "Error, no reflection for value ${int_value} in enum betting_market_status",
                    ("int_value", i));
            ++error_count;
         }
      }
      if (error_count == 0)
         dlog("Betting market status constants are correct");
      else
         wlog("There were ${count} errors in the betting market status constants", ("count", error_count));

      return error_count == 0;
   }
} // end anonymous namespace


betting_market_status betting_market_object::get_status() const
{
   static bool state_constants_are_correct = verify_betting_market_status_constants();
   (void)&state_constants_are_correct;
   betting_market_state state = (betting_market_state)my->state_machine.current_state()[0];
   
   edump((state));

   switch (state)
   {
      case betting_market_state::unresolved:
         return betting_market_status::unresolved;
      case betting_market_state::frozen:
         return betting_market_status::frozen;
      case betting_market_state::closed:
         return betting_market_status::unresolved;
      case betting_market_state::canceled:
         return betting_market_status::canceled;
      case betting_market_state::graded:
         return betting_market_status::graded;
      case betting_market_state::settled:
         return betting_market_status::settled;
      default:
         FC_THROW("Unexpected betting market state");
   };
}

void betting_market_object::cancel_all_unmatched_bets(database& db) const
{
   const auto& bet_odds_idx = db.get_index_type<bet_object_index>().indices().get<by_odds>();

   // first, cancel all bets on the active books
   auto book_itr = bet_odds_idx.lower_bound(std::make_tuple(id));
   auto book_end = bet_odds_idx.upper_bound(std::make_tuple(id));
   while (book_itr != book_end)
   {
      auto old_book_itr = book_itr;
      ++book_itr;
      db.cancel_bet(*old_book_itr, true);
   }

   // then, cancel any delayed bets on that market.  We don't have an index for
   // that, so walk through all delayed bets
   book_itr = bet_odds_idx.begin();
   while (book_itr != bet_odds_idx.end() &&
          book_itr->end_of_delay)
   {
      auto old_book_itr = book_itr;
      ++book_itr;
      if (old_book_itr->betting_market_id == id)
         db.cancel_bet(*old_book_itr, true);
   }
}
    
void betting_market_object::cancel_all_bets(database& db) const
{
    const auto& bets_by_market_id = db.get_index_type<bet_object_index>().indices().get<by_betting_market_id>();
    
    auto bet_it = bets_by_market_id.lower_bound(id);
    auto bet_it_end = bets_by_market_id.upper_bound(id);
    while (bet_it != bet_it_end)
    {
        auto old_bet_it = bet_it;
        ++bet_it;
        db.cancel_bet(*old_bet_it, true);
    }
}

void betting_market_object::pack_impl(std::ostream& stream) const
{
   boost::archive::binary_oarchive oa(stream, boost::archive::no_header|boost::archive::no_codecvt|boost::archive::no_xml_tag_checking);
   oa << my->state_machine;
}

void betting_market_object::unpack_impl(std::istream& stream)
{
   boost::archive::binary_iarchive ia(stream, boost::archive::no_header|boost::archive::no_codecvt|boost::archive::no_xml_tag_checking);
   ia >> my->state_machine;
}

void betting_market_object::on_unresolved_event(database& db)
{
   my->state_machine.process_event(unresolved_event(db));
}

void betting_market_object::on_frozen_event(database& db)
{
   my->state_machine.process_event(frozen_event(db));
}

void betting_market_object::on_closed_event(database& db)
{
   my->state_machine.process_event(closed_event(db));
}

void betting_market_object::on_graded_event(database& db, betting_market_resolution_type new_grading)
{
   my->state_machine.process_event(graded_event(db, new_grading));
}

void betting_market_object::on_settled_event(database& db)
{
   my->state_machine.process_event(settled_event(db));
}

void betting_market_object::on_canceled_event(database& db)
{
   my->state_machine.process_event(canceled_event(db));
}

} } // graphene::chain

namespace fc { 
   // Manually reflect betting_market_object to variant to properly reflect "state"
   void to_variant(const graphene::chain::betting_market_object& event_obj, fc::variant& v, uint32_t max_depth)
   {
      fc::mutable_variant_object o;
      o("id", fc::variant(event_obj.id, max_depth) )
       ("group_id", fc::variant(event_obj.group_id, max_depth))
       ("description", fc::variant(event_obj.description, max_depth))
       ("payout_condition", fc::variant(event_obj.payout_condition, max_depth))
       ("resolution", fc::variant(event_obj.resolution, max_depth))
       ("status", fc::variant(event_obj.get_status(), max_depth));

      v = o;
   }

   // Manually reflect betting_market_object to variant to properly reflect "state"
   void from_variant(const fc::variant& v, graphene::chain::betting_market_object& event_obj, uint32_t max_depth)
   {
      event_obj.id = v["id"].as<graphene::chain::betting_market_id_type>( max_depth );
      event_obj.group_id = v["name"].as<graphene::chain::betting_market_group_id_type>( max_depth );
      event_obj.description = v["description"].as<graphene::chain::internationalized_string_type>( max_depth );
      event_obj.payout_condition = v["payout_condition"].as<graphene::chain::internationalized_string_type>( max_depth );
      event_obj.resolution = v["resolution"].as<fc::optional<graphene::chain::betting_market_resolution_type>>( max_depth );
      graphene::chain::betting_market_status status = v["status"].as<graphene::chain::betting_market_status>( max_depth );
      const_cast<int*>(event_obj.my->state_machine.current_state())[0] = (int)status;
   }
} //end namespace fc

