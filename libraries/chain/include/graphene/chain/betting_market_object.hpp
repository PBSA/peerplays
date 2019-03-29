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
#pragma once

#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>
#include <graphene/chain/protocol/betting_market.hpp>

#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {
   class betting_market_object;
   class betting_market_group_object;
} }

namespace fc { 
   void to_variant(const graphene::chain::betting_market_object& betting_market_obj, fc::variant& v);
   void from_variant(const fc::variant& v, graphene::chain::betting_market_object& betting_market_obj);
   void to_variant(const graphene::chain::betting_market_group_object& betting_market_group_obj, fc::variant& v);
   void from_variant(const fc::variant& v, graphene::chain::betting_market_group_object& betting_market_group_obj);
} //end namespace fc

namespace graphene { namespace chain {

FC_DECLARE_EXCEPTION(no_transition, 100000, "Invalid state transition");
class database;

struct by_event_id;
struct by_settling_time;
struct by_betting_market_group_id;

class betting_market_rules_object : public graphene::db::abstract_object< betting_market_rules_object >
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = betting_market_rules_object_type;

      internationalized_string_type name;

      internationalized_string_type description;
};

class betting_market_group_object : public graphene::db::abstract_object< betting_market_group_object >
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = betting_market_group_object_type;

      betting_market_group_object();
      betting_market_group_object(const betting_market_group_object& rhs);
      ~betting_market_group_object();
      betting_market_group_object& operator=(const betting_market_group_object& rhs);

      internationalized_string_type description;

      event_id_type event_id;

      betting_market_rules_id_type rules_id;

      asset_id_type asset_id;

      share_type total_matched_bets_amount;

      bool never_in_play;

      uint32_t delay_before_settling;

      fc::optional<fc::time_point_sec> settling_time;  // the time the payout will occur (set after grading)

      bool bets_are_allowed() const { 
        return get_status() == betting_market_group_status::upcoming || 
               get_status() == betting_market_group_status::in_play; 
      }

      bool bets_are_delayed() const { 
        return get_status() == betting_market_group_status::in_play;
      }

      betting_market_group_status get_status() const;

      // serialization functions:
      // for serializing to raw, go through a temporary sstream object to avoid
      // having to implement serialization in the header file
      template<typename Stream>
      friend Stream& operator<<( Stream& s, const betting_market_group_object& betting_market_group_obj );

      template<typename Stream>
      friend Stream& operator>>( Stream& s, betting_market_group_object& betting_market_group_obj );

      friend void ::fc::to_variant(const graphene::chain::betting_market_group_object& betting_market_group_obj, fc::variant& v);
      friend void ::fc::from_variant(const fc::variant& v, graphene::chain::betting_market_group_object& betting_market_group_obj);

      void pack_impl(std::ostream& stream) const;
      void unpack_impl(std::istream& stream);

      void on_upcoming_event(database& db);
      void on_in_play_event(database& db);
      void on_frozen_event(database& db);
      void on_closed_event(database& db, bool closed_by_event);
      void on_graded_event(database& db);
      void on_re_grading_event(database& db);
      void on_settled_event(database& db);
      void on_canceled_event(database& db, bool canceled_by_event);
      void dispatch_new_status(database& db, betting_market_group_status new_status);

   private:
      class impl;
      std::unique_ptr<impl> my;
};

class betting_market_object : public graphene::db::abstract_object< betting_market_object >
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = betting_market_object_type;

      betting_market_object();
      betting_market_object(const betting_market_object& rhs);
      ~betting_market_object();
      betting_market_object& operator=(const betting_market_object& rhs);

      betting_market_group_id_type group_id;

      internationalized_string_type description;

      internationalized_string_type payout_condition;

      // once the market is graded, this holds the proposed grading
      // after settling/canceling, this is the actual grading
      fc::optional<betting_market_resolution_type> resolution;

      betting_market_status get_status() const;

      void cancel_all_unmatched_bets(database& db) const;
      void cancel_all_bets(database& db) const;

      // serialization functions:
      // for serializing to raw, go through a temporary sstream object to avoid
      // having to implement serialization in the header file
      template<typename Stream>
      friend Stream& operator<<( Stream& s, const betting_market_object& betting_market_obj );

      template<typename Stream>
      friend Stream& operator>>( Stream& s, betting_market_object& betting_market_obj );

      friend void ::fc::to_variant(const graphene::chain::betting_market_object& betting_market_obj, fc::variant& v);
      friend void ::fc::from_variant(const fc::variant& v, graphene::chain::betting_market_object& betting_market_obj);

      void pack_impl(std::ostream& stream) const;
      void unpack_impl(std::istream& stream);

      void on_unresolved_event(database& db);
      void on_frozen_event(database& db);
      void on_closed_event(database& db);
      void on_graded_event(database& db, betting_market_resolution_type new_grading);
      void on_settled_event(database& db);
      void on_canceled_event(database& db);
   private:
      class impl;
      std::unique_ptr<impl> my;
};

class bet_object : public graphene::db::abstract_object< bet_object >
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = bet_object_type;

      account_id_type bettor_id;
      
      betting_market_id_type betting_market_id;

      asset amount_to_bet;

      bet_multiplier_type backer_multiplier;

      bet_type back_or_lay;

      fc::optional<fc::time_point_sec> end_of_delay;

      static share_type get_approximate_matching_amount(share_type bet_amount, bet_multiplier_type backer_multiplier, bet_type back_or_lay, bool round_up = false);

      // returns the amount of a bet that completely matches this bet
      share_type get_approximate_matching_amount(bool round_up = false) const;

      static share_type get_exact_matching_amount(share_type bet_amount, bet_multiplier_type backer_multiplier, bet_type back_or_lay);
      share_type get_exact_matching_amount() const;

      static std::pair<share_type, share_type> get_ratio(bet_multiplier_type backer_multiplier);
      std::pair<share_type, share_type> get_ratio() const; 

      // returns the minimum amount this bet could have that could be matched at these odds
      share_type get_minimum_matchable_amount() const; 
      // returns the minimum amount another user could bet to match this bet at these odds
      share_type get_minimum_matching_amount() const;
};

class betting_market_position_object : public graphene::db::abstract_object< betting_market_position_object >
{
   public:
      static const uint8_t space_id = implementation_ids;
      static const uint8_t type_id = impl_betting_market_position_object_type;

      account_id_type bettor_id;
      
      betting_market_id_type betting_market_id;

      share_type pay_if_payout_condition;
      share_type pay_if_not_payout_condition;
      share_type pay_if_canceled;
      share_type pay_if_not_canceled;
      share_type fees_collected;
      
      share_type reduce();
};

typedef multi_index_container<
   betting_market_rules_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >
   > > betting_market_rules_object_multi_index_type;
typedef generic_index<betting_market_rules_object, betting_market_rules_object_multi_index_type> betting_market_rules_object_index;

typedef multi_index_container<
   betting_market_group_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_event_id>, composite_key<betting_market_group_object,
                                                      member<betting_market_group_object, event_id_type, &betting_market_group_object::event_id>,
                                                      member<object, object_id_type, &object::id> > >,
      ordered_unique< tag<by_settling_time>, composite_key<betting_market_group_object,
                                                           member<betting_market_group_object, fc::optional<fc::time_point_sec>, &betting_market_group_object::settling_time>,
                                                           member<object, object_id_type, &object::id> > >
   > > betting_market_group_object_multi_index_type;
typedef generic_index<betting_market_group_object, betting_market_group_object_multi_index_type> betting_market_group_object_index;

typedef multi_index_container<
   betting_market_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_betting_market_group_id>, composite_key<betting_market_object,
                                                                     member<betting_market_object, betting_market_group_id_type, &betting_market_object::group_id>,
                                                                     member<object, object_id_type, &object::id> > >
   > > betting_market_object_multi_index_type;

typedef generic_index<betting_market_object, betting_market_object_multi_index_type> betting_market_object_index;

struct compare_bet_by_odds {
   bool operator()(const bet_object& lhs, const bet_object& rhs) const
   {
      return compare(lhs.end_of_delay, lhs.betting_market_id, lhs.back_or_lay, lhs.backer_multiplier, lhs.id,
                     rhs.end_of_delay, rhs.betting_market_id, rhs.back_or_lay, rhs.backer_multiplier, rhs.id);
   }

   template<typename T0>
   bool operator() (const std::tuple<T0>& lhs, const bet_object& rhs) const
   {
      return compare(fc::optional<time_point_sec>(), std::get<0>(lhs), rhs.end_of_delay, rhs.betting_market_id);
   }

   template<typename T0>
   bool operator() (const bet_object& lhs, const std::tuple<T0>& rhs) const
   {
      return compare(lhs.end_of_delay, lhs.betting_market_id, fc::optional<time_point_sec>(), std::get<0>(rhs));
   }

   template<typename T0, typename T1>
   bool operator() (const std::tuple<T0, T1>& lhs, const bet_object& rhs) const
   {
      return compare(fc::optional<fc::time_point_sec>(), std::get<0>(lhs), std::get<1>(lhs), rhs.end_of_delay, rhs.betting_market_id, rhs.back_or_lay);
   }

   template<typename T0, typename T1>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1>& rhs) const
   {
      return compare(lhs.end_of_delay, lhs.betting_market_id, lhs.back_or_lay, fc::optional<time_point_sec>(), std::get<0>(rhs), std::get<1>(rhs));
   }

   template<typename T0, typename T1, typename T2>
   bool operator() (const std::tuple<T0, T1, T2>& lhs, const bet_object& rhs) const
   {
      return compare(fc::optional<time_point_sec>(), std::get<0>(lhs), std::get<1>(lhs), std::get<2>(lhs),
                     rhs.end_of_delay, rhs.betting_market_id, rhs.back_or_lay, rhs.backer_multiplier);
   }

   template<typename T0, typename T1, typename T2>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1, T2>& rhs) const
   {
      return compare(lhs.end_of_delay, lhs.betting_market_id, lhs.back_or_lay, lhs.backer_multiplier,
                     fc::optional<time_point_sec>(), std::get<0>(rhs), std::get<1>(rhs), std::get<2>(rhs));
   }
   template<typename T0, typename T1, typename T2, typename T3>
   bool operator() (const std::tuple<T0, T1, T2, T3>& lhs, const bet_object& rhs) const
   {
      return compare(fc::optional<time_point_sec>(), std::get<0>(lhs), std::get<1>(lhs), std::get<2>(lhs), std::get<3>(lhs),
                     rhs.end_of_delay, rhs.betting_market_id, rhs.back_or_lay, rhs.backer_multiplier, rhs.id);
   }

   template<typename T0, typename T1, typename T2, typename T3>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1, T2, T3>& rhs) const
   {
      return compare(lhs.end_of_delay, lhs.betting_market_id, lhs.back_or_lay, lhs.backer_multiplier, lhs.id,
                     fc::optional<time_point_sec>(), std::get<0>(rhs), std::get<1>(rhs), std::get<2>(rhs), std::get<3>(rhs));
   }
   bool compare(const fc::optional<fc::time_point_sec>& lhs_end_of_delay, 
                const betting_market_id_type& lhs_betting_market_id, 
                const fc::optional<fc::time_point_sec>& rhs_end_of_delay, 
                const betting_market_id_type& rhs_betting_market_id) const
   {
      // if either bet is delayed, sort the delayed bet to the
      // front.  If both are delayed, the delay expiring soonest
      // comes first.
      if (lhs_end_of_delay || rhs_end_of_delay)
      {
         if (!rhs_end_of_delay)
            return true;
         if (!lhs_end_of_delay)
            return false;
         return *lhs_end_of_delay < *rhs_end_of_delay;
      }

      return lhs_betting_market_id < rhs_betting_market_id;
   }
   bool compare(const fc::optional<fc::time_point_sec>& lhs_end_of_delay,
                const betting_market_id_type& lhs_betting_market_id, bet_type lhs_bet_type, 
                const fc::optional<fc::time_point_sec>& rhs_end_of_delay, 
                const betting_market_id_type& rhs_betting_market_id, bet_type rhs_bet_type) const
   {
      // if either bet is delayed, sort the delayed bet to the
      // front.  If both are delayed, the delay expiring soonest
      // comes first.
      if (lhs_end_of_delay || rhs_end_of_delay)
      {
         if (!rhs_end_of_delay)
            return true;
         if (!lhs_end_of_delay)
            return false;
         return *lhs_end_of_delay < *rhs_end_of_delay;
      }

      if (lhs_betting_market_id < rhs_betting_market_id)
         return true;
      if (lhs_betting_market_id > rhs_betting_market_id)
         return false;
      return lhs_bet_type < rhs_bet_type;
   }
   bool compare(const fc::optional<fc::time_point_sec>& lhs_end_of_delay, 
                const betting_market_id_type& lhs_betting_market_id, bet_type lhs_bet_type, 
                bet_multiplier_type lhs_backer_multiplier,
                const fc::optional<fc::time_point_sec>& rhs_end_of_delay, 
                const betting_market_id_type& rhs_betting_market_id, bet_type rhs_bet_type,
                bet_multiplier_type rhs_backer_multiplier) const
   {
      // if either bet is delayed, sort the delayed bet to the
      // front.  If both are delayed, the delay expiring soonest
      // comes first.
      if (lhs_end_of_delay || rhs_end_of_delay)
      {
         if (!rhs_end_of_delay)
            return true;
         if (!lhs_end_of_delay)
            return false;
         return *lhs_end_of_delay < *rhs_end_of_delay;
      }

      if (lhs_betting_market_id < rhs_betting_market_id)
         return true;
      if (lhs_betting_market_id > rhs_betting_market_id)
         return false;
      if (lhs_bet_type < rhs_bet_type)
         return true;
      if (lhs_bet_type > rhs_bet_type)
         return false;
      if (lhs_bet_type == bet_type::back)
        return lhs_backer_multiplier < rhs_backer_multiplier;
      else
        return lhs_backer_multiplier > rhs_backer_multiplier;
   }
   bool compare(const fc::optional<fc::time_point_sec>& lhs_end_of_delay, 
                const betting_market_id_type& lhs_betting_market_id, bet_type lhs_bet_type, 
                bet_multiplier_type lhs_backer_multiplier, const bet_id_type& lhs_bet_id,
                const fc::optional<fc::time_point_sec>& rhs_end_of_delay, 
                const betting_market_id_type& rhs_betting_market_id, bet_type rhs_bet_type,
                bet_multiplier_type rhs_backer_multiplier, const bet_id_type& rhs_bet_id) const
   {
     
      // if either bet is delayed, sort the delayed bet to the
      // front.  If both are delayed, the delay expiring soonest
      // comes first.
      if (lhs_end_of_delay || rhs_end_of_delay)
      {
         if (!rhs_end_of_delay)
            return true;
         if (!lhs_end_of_delay)
            return false;
         if (*lhs_end_of_delay < *rhs_end_of_delay)
            return true;
         if (*lhs_end_of_delay > *rhs_end_of_delay)
            return false;
         // if both bets have the same delay, prefer the one
         // that was placed first (lowest id)
         return lhs_bet_id < rhs_bet_id;
      }

      // if neither bet was delayed
      if (lhs_betting_market_id < rhs_betting_market_id)
         return true;
      if (lhs_betting_market_id > rhs_betting_market_id)
         return false;
      if (lhs_bet_type < rhs_bet_type)
         return true;
      if (lhs_bet_type > rhs_bet_type)
         return false;
      if (lhs_backer_multiplier < rhs_backer_multiplier)
         return lhs_bet_type == bet_type::back;
      if (lhs_backer_multiplier > rhs_backer_multiplier)
         return lhs_bet_type == bet_type::lay;
      return lhs_bet_id < rhs_bet_id;
   }
};

struct compare_bet_by_bettor_then_odds {
   bool operator()(const bet_object& lhs, const bet_object& rhs) const
   {
      return compare(lhs.bettor_id, lhs.betting_market_id, lhs.back_or_lay, lhs.backer_multiplier, lhs.id,
                     rhs.bettor_id, rhs.betting_market_id, rhs.back_or_lay, rhs.backer_multiplier, rhs.id);
   }

   template<typename T0>
   bool operator() (const std::tuple<T0>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), rhs.bettor_id);
   }

   template<typename T0>
   bool operator() (const bet_object& lhs, const std::tuple<T0>& rhs) const
   {
      return compare(lhs.bettor_id, std::get<0>(rhs));
   }

   template<typename T0, typename T1>
   bool operator() (const std::tuple<T0, T1>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), std::get<1>(lhs), rhs.bettor_id, rhs.betting_market_id);
   }

   template<typename T0, typename T1>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1>& rhs) const
   {
      return compare(lhs.bettor_id, lhs.betting_market_id, std::get<0>(rhs), std::get<1>(rhs));
   }

   template<typename T0, typename T1, typename T2>
   bool operator() (const std::tuple<T0, T1, T2>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), std::get<1>(lhs), std::get<2>(lhs),
                     rhs.bettor_id, rhs.betting_market_id, rhs.back_or_lay);
   }

   template<typename T0, typename T1, typename T2>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1, T2>& rhs) const
   {
      return compare(lhs.bettor_id, lhs.betting_market_id, lhs.back_or_lay,
                     std::get<0>(rhs), std::get<1>(rhs), std::get<2>(rhs));
   }
   template<typename T0, typename T1, typename T2, typename T3>
   bool operator() (const std::tuple<T0, T1, T2, T3>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), std::get<1>(lhs), std::get<2>(lhs), std::get<3>(lhs),
                     rhs.bettor_id, rhs.betting_market_id, rhs.back_or_lay, rhs.backer_multiplier);
   }

   template<typename T0, typename T1, typename T2, typename T3>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1, T2, T3>& rhs) const
   {
      return compare(lhs.bettor_id, lhs.betting_market_id, lhs.back_or_lay, lhs.backer_multiplier, lhs.id,
                     std::get<0>(rhs), std::get<1>(rhs), std::get<2>(rhs), std::get<3>(rhs));
   }
   template<typename T0, typename T1, typename T2, typename T3, typename T4>
   bool operator() (const std::tuple<T0, T1, T2, T3, T4>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), std::get<1>(lhs), std::get<2>(lhs), std::get<3>(lhs), std::get<4>(lhs),
                     rhs.bettor_id, rhs.betting_market_id, rhs.back_or_lay, rhs.backer_multiplier, rhs.id);
   }
   template<typename T0, typename T1, typename T2, typename T3, typename T4>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1, T2, T3, T4>& rhs) const
   {
      return compare(lhs.bettor_id, lhs.betting_market_id, lhs.back_or_lay, lhs.backer_multiplier, lhs.id,
                     std::get<0>(rhs), std::get<1>(rhs), std::get<2>(rhs), std::get<3>(rhs), std::get<4>(rhs));
   }
   bool compare(const account_id_type& lhs_bettor_id, const account_id_type& rhs_bettor_id) const
   {
      return lhs_bettor_id < rhs_bettor_id;
   }
   bool compare(const account_id_type& lhs_bettor_id, const betting_market_id_type& lhs_betting_market_id, 
                const account_id_type& rhs_bettor_id, const betting_market_id_type& rhs_betting_market_id) const
   {
      if (lhs_bettor_id < rhs_bettor_id)
          return true;
      if (lhs_bettor_id > rhs_bettor_id)
          return false;
      return lhs_betting_market_id < rhs_betting_market_id;
   }

   bool compare(const account_id_type& lhs_bettor_id, const betting_market_id_type& lhs_betting_market_id,  
                bet_type lhs_bet_type, 
                const account_id_type& rhs_bettor_id, const betting_market_id_type& rhs_betting_market_id,  
                bet_type rhs_bet_type) const
   {
      if (lhs_bettor_id < rhs_bettor_id)
          return true;
      if (lhs_bettor_id > rhs_bettor_id)
          return false;
      if (lhs_betting_market_id < rhs_betting_market_id)
         return true;
      if (lhs_betting_market_id > rhs_betting_market_id)
         return false;
      return lhs_bet_type < rhs_bet_type;
   }
   bool compare(const account_id_type& lhs_bettor_id, const betting_market_id_type& lhs_betting_market_id,
                bet_type lhs_bet_type, 
                bet_multiplier_type lhs_backer_multiplier,
                const account_id_type& rhs_bettor_id, const betting_market_id_type& rhs_betting_market_id, 
                bet_type rhs_bet_type,
                bet_multiplier_type rhs_backer_multiplier) const
   {
      if (lhs_bettor_id < rhs_bettor_id)
          return true;
      if (lhs_bettor_id > rhs_bettor_id)
          return false;
      if (lhs_betting_market_id < rhs_betting_market_id)
         return true;
      if (lhs_betting_market_id > rhs_betting_market_id)
         return false;
      if (lhs_bet_type < rhs_bet_type)
         return true;
      if (lhs_bet_type > rhs_bet_type)
         return false;
      if (lhs_bet_type == bet_type::back)
        return lhs_backer_multiplier < rhs_backer_multiplier;
      else
        return lhs_backer_multiplier > rhs_backer_multiplier;
   }
   bool compare(const account_id_type& lhs_bettor_id, const betting_market_id_type& lhs_betting_market_id, 
                bet_type lhs_bet_type, 
                bet_multiplier_type lhs_backer_multiplier, const bet_id_type& lhs_bet_id,
                const account_id_type& rhs_bettor_id, const betting_market_id_type& rhs_betting_market_id,
                bet_type rhs_bet_type,
                bet_multiplier_type rhs_backer_multiplier, const bet_id_type& rhs_bet_id) const
   {
      if (lhs_bettor_id < rhs_bettor_id)
          return true;
      if (lhs_bettor_id > rhs_bettor_id)
          return false;
      if (lhs_betting_market_id < rhs_betting_market_id)
         return true;
      if (lhs_betting_market_id > rhs_betting_market_id)
         return false;
      if (lhs_bet_type < rhs_bet_type)
         return true;
      if (lhs_bet_type > rhs_bet_type)
         return false;
      if (lhs_backer_multiplier < rhs_backer_multiplier)
         return lhs_bet_type == bet_type::back;
      if (lhs_backer_multiplier > rhs_backer_multiplier)
         return lhs_bet_type == bet_type::lay;
      
      return lhs_bet_id < rhs_bet_id;
   }
};

struct by_odds {};
struct by_betting_market_id {};
struct by_bettor_and_odds {};
typedef multi_index_container<
   bet_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_odds>, identity<bet_object>, compare_bet_by_odds >,
      ordered_unique< tag<by_betting_market_id>, composite_key<bet_object,
                                                                   member<bet_object, betting_market_id_type, &bet_object::betting_market_id>,
                                                                   member<object, object_id_type, &object::id> > >,
      ordered_unique< tag<by_bettor_and_odds>, identity<bet_object>, compare_bet_by_bettor_then_odds > > > bet_object_multi_index_type;
typedef generic_index<bet_object, bet_object_multi_index_type> bet_object_index;

struct by_bettor_betting_market{};
struct by_betting_market_bettor{};
typedef multi_index_container<
   betting_market_position_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_bettor_betting_market>, 
            composite_key<
               betting_market_position_object,
               member<betting_market_position_object, account_id_type, &betting_market_position_object::bettor_id>,
               member<betting_market_position_object, betting_market_id_type, &betting_market_position_object::betting_market_id> > >,
      ordered_unique< tag<by_betting_market_bettor>, 
            composite_key<
               betting_market_position_object,
               member<betting_market_position_object, betting_market_id_type, &betting_market_position_object::betting_market_id>,
               member<betting_market_position_object, account_id_type, &betting_market_position_object::bettor_id> > >
  > > betting_market_position_multi_index_type;

typedef generic_index<betting_market_position_object, betting_market_position_multi_index_type> betting_market_position_index;


template<typename Stream>
inline Stream& operator<<( Stream& s, const betting_market_object& betting_market_obj )
{ 
   // pack all fields exposed in the header in the usual way
   // instead of calling the derived pack, just serialize the one field in the base class
   //   fc::raw::pack<Stream, const graphene::db::abstract_object<betting_market_object> >(s, betting_market_obj);
   fc::raw::pack(s, betting_market_obj.id);
   fc::raw::pack(s, betting_market_obj.group_id);
   fc::raw::pack(s, betting_market_obj.description);
   fc::raw::pack(s, betting_market_obj.payout_condition);
   fc::raw::pack(s, betting_market_obj.resolution);

   // fc::raw::pack the contents hidden in the impl class
   std::ostringstream stream;
   betting_market_obj.pack_impl(stream);
   std::string stringified_stream(stream.str());
   fc::raw::pack(s, stream.str());

   return s;
}
template<typename Stream>
inline Stream& operator>>( Stream& s, betting_market_object& betting_market_obj )
{ 
   // unpack all fields exposed in the header in the usual way
   //fc::raw::unpack<Stream, graphene::db::abstract_object<betting_market_object> >(s, betting_market_obj);
   fc::raw::unpack(s, betting_market_obj.id);
   fc::raw::unpack(s, betting_market_obj.group_id);
   fc::raw::unpack(s, betting_market_obj.description);
   fc::raw::unpack(s, betting_market_obj.payout_condition);
   fc::raw::unpack(s, betting_market_obj.resolution);

   // fc::raw::unpack the contents hidden in the impl class
   std::string stringified_stream;
   fc::raw::unpack(s, stringified_stream);
   std::istringstream stream(stringified_stream);
   betting_market_obj.unpack_impl(stream);
   
   return s;
}


template<typename Stream>
inline Stream& operator<<( Stream& s, const betting_market_group_object& betting_market_group_obj )
{ 
   // pack all fields exposed in the header in the usual way
   // instead of calling the derived pack, just serialize the one field in the base class
   //   fc::raw::pack<Stream, const graphene::db::abstract_object<betting_market_group_object> >(s, betting_market_group_obj);
   fc::raw::pack(s, betting_market_group_obj.id);
   fc::raw::pack(s, betting_market_group_obj.description);
   fc::raw::pack(s, betting_market_group_obj.event_id);
   fc::raw::pack(s, betting_market_group_obj.rules_id);
   fc::raw::pack(s, betting_market_group_obj.asset_id);
   fc::raw::pack(s, betting_market_group_obj.total_matched_bets_amount);
   fc::raw::pack(s, betting_market_group_obj.never_in_play);
   fc::raw::pack(s, betting_market_group_obj.delay_before_settling);
   fc::raw::pack(s, betting_market_group_obj.settling_time);
   // fc::raw::pack the contents hidden in the impl class
   std::ostringstream stream;
   betting_market_group_obj.pack_impl(stream);
   std::string stringified_stream(stream.str());
   fc::raw::pack(s, stream.str());

   return s;
}
template<typename Stream>
inline Stream& operator>>( Stream& s, betting_market_group_object& betting_market_group_obj )
{ 
   // unpack all fields exposed in the header in the usual way
   //fc::raw::unpack<Stream, graphene::db::abstract_object<betting_market_group_object> >(s, betting_market_group_obj);
   fc::raw::unpack(s, betting_market_group_obj.id);
   fc::raw::unpack(s, betting_market_group_obj.description);
   fc::raw::unpack(s, betting_market_group_obj.event_id);
   fc::raw::unpack(s, betting_market_group_obj.rules_id);
   fc::raw::unpack(s, betting_market_group_obj.asset_id);
   fc::raw::unpack(s, betting_market_group_obj.total_matched_bets_amount);
   fc::raw::unpack(s, betting_market_group_obj.never_in_play);
   fc::raw::unpack(s, betting_market_group_obj.delay_before_settling);
   fc::raw::unpack(s, betting_market_group_obj.settling_time);

   // fc::raw::unpack the contents hidden in the impl class
   std::string stringified_stream;
   fc::raw::unpack(s, stringified_stream);
   std::istringstream stream(stringified_stream);
   betting_market_group_obj.unpack_impl(stream);
   
   return s;
}

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::betting_market_rules_object, (graphene::db::object), (name)(description) )
FC_REFLECT_DERIVED( graphene::chain::betting_market_group_object, (graphene::db::object), (description) )
FC_REFLECT_DERIVED( graphene::chain::betting_market_object, (graphene::db::object), (group_id) )
FC_REFLECT_DERIVED( graphene::chain::bet_object, (graphene::db::object), (bettor_id)(betting_market_id)(amount_to_bet)(backer_multiplier)(back_or_lay)(end_of_delay) )

FC_REFLECT_DERIVED( graphene::chain::betting_market_position_object, (graphene::db::object), (bettor_id)(betting_market_id)(pay_if_payout_condition)(pay_if_not_payout_condition)(pay_if_canceled)(pay_if_not_canceled)(fees_collected) )
