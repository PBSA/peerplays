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
#pragma once

#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>
#include <graphene/chain/protocol/betting_market.hpp>

#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {

class database;

struct by_event_id;
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

      internationalized_string_type description;

      event_id_type event_id;

      betting_market_rules_id_type rules_id;

      asset_id_type asset_id;

      share_type total_matched_bets_amount;

      bool frozen;
};

class betting_market_object : public graphene::db::abstract_object< betting_market_object >
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = betting_market_object_type;

      betting_market_group_id_type group_id;

      internationalized_string_type description;

      internationalized_string_type payout_condition;
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

      share_type amount_reserved_for_fees; // same asset type as amount_to_bet

      bet_type back_or_lay;

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
      ordered_non_unique< tag<by_event_id>, member<betting_market_group_object, event_id_type, &betting_market_group_object::event_id> >
   > > betting_market_group_object_multi_index_type;
typedef generic_index<betting_market_group_object, betting_market_group_object_multi_index_type> betting_market_group_object_index;

typedef multi_index_container<
   betting_market_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_non_unique< tag<by_betting_market_group_id>, member<betting_market_object, betting_market_group_id_type, &betting_market_object::group_id> >
   > > betting_market_object_multi_index_type;

typedef generic_index<betting_market_object, betting_market_object_multi_index_type> betting_market_object_index;

struct compare_bet_by_odds {
   bool operator()(const bet_object& lhs, const bet_object& rhs) const
   {
      return compare(lhs.betting_market_id, lhs.back_or_lay, lhs.backer_multiplier, lhs.id,
                     rhs.betting_market_id, rhs.back_or_lay, rhs.backer_multiplier, rhs.id);
   }

   template<typename T0>
   bool operator() (const std::tuple<T0>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), rhs.betting_market_id);
   }

   template<typename T0>
   bool operator() (const bet_object& lhs, const std::tuple<T0>& rhs) const
   {
      return compare(lhs.betting_market_id, std::get<0>(rhs));
   }

   template<typename T0, typename T1>
   bool operator() (const std::tuple<T0, T1>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), std::get<1>(lhs), rhs.betting_market_id, rhs.back_or_lay);
   }

   template<typename T0, typename T1>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1>& rhs) const
   {
      return compare(lhs.betting_market_id, lhs.back_or_lay, std::get<0>(rhs), std::get<1>(rhs));
   }

   template<typename T0, typename T1, typename T2>
   bool operator() (const std::tuple<T0, T1, T2>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), std::get<1>(lhs), std::get<2>(lhs),
                     rhs.betting_market_id, rhs.back_or_lay, rhs.backer_multiplier);
   }

   template<typename T0, typename T1, typename T2>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1, T2>& rhs) const
   {
      return compare(lhs.betting_market_id, lhs.back_or_lay, lhs.backer_multiplier,
                     std::get<0>(rhs), std::get<1>(rhs), std::get<2>(rhs));
   }
   template<typename T0, typename T1, typename T2, typename T3>
   bool operator() (const std::tuple<T0, T1, T2, T3>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), std::get<1>(lhs), std::get<2>(lhs), std::get<3>(lhs),
                     rhs.betting_market_id, rhs.back_or_lay, rhs.backer_multiplier, rhs.id);
   }

   template<typename T0, typename T1, typename T2, typename T3>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1, T2, T3>& rhs) const
   {
      return compare(lhs.betting_market_id, lhs.back_or_lay, lhs.backer_multiplier, lhs.id,
                     std::get<0>(rhs), std::get<1>(rhs), std::get<2>(rhs), std::get<3>(rhs));
   }
   bool compare(const betting_market_id_type& lhs_betting_market_id, const betting_market_id_type& rhs_betting_market_id) const
   {
      return lhs_betting_market_id < rhs_betting_market_id;
   }
   bool compare(const betting_market_id_type& lhs_betting_market_id, bet_type lhs_bet_type, 
                const betting_market_id_type& rhs_betting_market_id, bet_type rhs_bet_type) const
   {
      if (lhs_betting_market_id < rhs_betting_market_id)
         return true;
      if (lhs_betting_market_id > rhs_betting_market_id)
         return false;
      return lhs_bet_type < rhs_bet_type;
   }
   bool compare(const betting_market_id_type& lhs_betting_market_id, bet_type lhs_bet_type, 
                bet_multiplier_type lhs_backer_multiplier,
                const betting_market_id_type& rhs_betting_market_id, bet_type rhs_bet_type,
                bet_multiplier_type rhs_backer_multiplier) const
   {
      if (lhs_betting_market_id < rhs_betting_market_id)
         return true;
      if (lhs_betting_market_id > rhs_betting_market_id)
         return false;
      if (lhs_bet_type < rhs_bet_type)
         return true;
      if (lhs_bet_type > rhs_bet_type)
         return false;
      return lhs_backer_multiplier < rhs_backer_multiplier;
   }
   bool compare(const betting_market_id_type& lhs_betting_market_id, bet_type lhs_bet_type, 
                bet_multiplier_type lhs_backer_multiplier, const bet_id_type& lhs_bet_id,
                const betting_market_id_type& rhs_betting_market_id, bet_type rhs_bet_type,
                bet_multiplier_type rhs_backer_multiplier, const bet_id_type& rhs_bet_id) const
   {
      if (lhs_betting_market_id < rhs_betting_market_id)
         return true;
      if (lhs_betting_market_id > rhs_betting_market_id)
         return false;
      if (lhs_bet_type < rhs_bet_type)
         return true;
      if (lhs_bet_type > rhs_bet_type)
         return false;
      if (lhs_backer_multiplier < rhs_backer_multiplier)
         return true;
      if (lhs_backer_multiplier > rhs_backer_multiplier)
         return false;
      return lhs_bet_id < rhs_bet_id;
   }
};

struct compare_bet_by_bettor_then_odds {
   bool operator()(const bet_object& lhs, const bet_object& rhs) const
   {
      return compare(lhs.betting_market_id, lhs.bettor_id, lhs.back_or_lay, lhs.backer_multiplier, lhs.id,
                     rhs.betting_market_id, rhs.bettor_id, rhs.back_or_lay, rhs.backer_multiplier, rhs.id);
   }

   template<typename T0>
   bool operator() (const std::tuple<T0>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), rhs.betting_market_id);
   }

   template<typename T0>
   bool operator() (const bet_object& lhs, const std::tuple<T0>& rhs) const
   {
      return compare(lhs.betting_market_id, std::get<0>(rhs));
   }

   template<typename T0, typename T1>
   bool operator() (const std::tuple<T0, T1>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), std::get<1>(lhs), rhs.betting_market_id, rhs.bettor_id);
   }

   template<typename T0, typename T1>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1>& rhs) const
   {
      return compare(lhs.betting_market_id, lhs.bettor_id, std::get<0>(rhs), std::get<1>(rhs));
   }

   template<typename T0, typename T1, typename T2>
   bool operator() (const std::tuple<T0, T1, T2>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), std::get<1>(lhs), std::get<2>(lhs),
                     rhs.betting_market_id, rhs.bettor_id, rhs.back_or_lay);
   }

   template<typename T0, typename T1, typename T2>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1, T2>& rhs) const
   {
      return compare(lhs.betting_market_id, lhs.bettor_id, lhs.back_or_lay,
                     std::get<0>(rhs), std::get<1>(rhs), std::get<2>(rhs));
   }
   template<typename T0, typename T1, typename T2, typename T3>
   bool operator() (const std::tuple<T0, T1, T2, T3>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), std::get<1>(lhs), std::get<2>(lhs), std::get<3>(lhs),
                     rhs.betting_market_id, rhs.bettor_id, rhs.back_or_lay, rhs.backer_multiplier);
   }

   template<typename T0, typename T1, typename T2, typename T3>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1, T2, T3>& rhs) const
   {
      return compare(lhs.betting_market_id, lhs.bettor_id, lhs.back_or_lay, lhs.backer_multiplier, lhs.id,
                     std::get<0>(rhs), std::get<1>(rhs), std::get<2>(rhs), std::get<3>(rhs));
   }
   template<typename T0, typename T1, typename T2, typename T3, typename T4>
   bool operator() (const std::tuple<T0, T1, T2, T3, T4>& lhs, const bet_object& rhs) const
   {
      return compare(std::get<0>(lhs), std::get<1>(lhs), std::get<2>(lhs), std::get<3>(lhs), std::get<4>(lhs),
                     rhs.betting_market_id, rhs.bettor_id, rhs.back_or_lay, rhs.backer_multiplier, rhs.id);
   }
   template<typename T0, typename T1, typename T2, typename T3, typename T4>
   bool operator() (const bet_object& lhs, const std::tuple<T0, T1, T2, T3, T4>& rhs) const
   {
      return compare(lhs.betting_market_id, lhs.bettor_id, lhs.back_or_lay, lhs.backer_multiplier, lhs.id,
                     std::get<0>(rhs), std::get<1>(rhs), std::get<2>(rhs), std::get<3>(rhs), std::get<4>(rhs));
   }
   bool compare(const betting_market_id_type& lhs_betting_market_id, const betting_market_id_type& rhs_betting_market_id) const
   {
      return lhs_betting_market_id < rhs_betting_market_id;
   }
   bool compare(const betting_market_id_type& lhs_betting_market_id, const account_id_type& lhs_bettor_id, 
                const betting_market_id_type& rhs_betting_market_id, const account_id_type& rhs_bettor_id) const
   {
      if (lhs_betting_market_id < rhs_betting_market_id)
         return true;
      if (lhs_betting_market_id > rhs_betting_market_id)
         return false;
      return lhs_bettor_id < rhs_bettor_id;
   }

   bool compare(const betting_market_id_type& lhs_betting_market_id, const account_id_type& lhs_bettor_id, 
                bet_type lhs_bet_type, 
                const betting_market_id_type& rhs_betting_market_id, const account_id_type& rhs_bettor_id, 
                bet_type rhs_bet_type) const
   {
      if (lhs_betting_market_id < rhs_betting_market_id)
         return true;
      if (lhs_betting_market_id > rhs_betting_market_id)
         return false;
      if (lhs_bettor_id < rhs_bettor_id)
          return true;
      if (lhs_bettor_id > rhs_bettor_id)
          return false;
      return lhs_bet_type < rhs_bet_type;
   }
   bool compare(const betting_market_id_type& lhs_betting_market_id, const account_id_type& lhs_bettor_id, 
                bet_type lhs_bet_type, 
                bet_multiplier_type lhs_backer_multiplier,
                const betting_market_id_type& rhs_betting_market_id, const account_id_type& rhs_bettor_id, 
                bet_type rhs_bet_type,
                bet_multiplier_type rhs_backer_multiplier) const
   {
      if (lhs_betting_market_id < rhs_betting_market_id)
         return true;
      if (lhs_betting_market_id > rhs_betting_market_id)
         return false;
      if (lhs_bettor_id < rhs_bettor_id)
          return true;
      if (lhs_bettor_id > rhs_bettor_id)
          return false;
      if (lhs_bet_type < rhs_bet_type)
         return true;
      if (lhs_bet_type > rhs_bet_type)
         return false;
      return lhs_backer_multiplier < rhs_backer_multiplier;
   }
   bool compare(const betting_market_id_type& lhs_betting_market_id, const account_id_type& lhs_bettor_id,
                bet_type lhs_bet_type, 
                bet_multiplier_type lhs_backer_multiplier, const bet_id_type& lhs_bet_id,
                const betting_market_id_type& rhs_betting_market_id, const account_id_type& rhs_bettor_id,
                bet_type rhs_bet_type,
                bet_multiplier_type rhs_backer_multiplier, const bet_id_type& rhs_bet_id) const
   {
      if (lhs_betting_market_id < rhs_betting_market_id)
         return true;
      if (lhs_betting_market_id > rhs_betting_market_id)
         return false;
      if (lhs_bettor_id < rhs_bettor_id)
          return true;
      if (lhs_bettor_id > rhs_bettor_id)
          return false;
      if (lhs_bet_type < rhs_bet_type)
         return true;
      if (lhs_bet_type > rhs_bet_type)
         return false;
      if (lhs_backer_multiplier < rhs_backer_multiplier)
         return true;
      if (lhs_backer_multiplier > rhs_backer_multiplier)
         return false;
      return lhs_bet_id < rhs_bet_id;
   }
};

struct by_odds {};
struct by_bettor_and_odds {};
typedef multi_index_container<
   bet_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_odds>, identity<bet_object>, compare_bet_by_odds >,
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
} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::betting_market_rules_object, (graphene::db::object), (name)(description) )
FC_REFLECT_DERIVED( graphene::chain::betting_market_group_object, (graphene::db::object), (description)(event_id)(rules_id)(asset_id)(frozen)(total_matched_bets_amount) )
FC_REFLECT_DERIVED( graphene::chain::betting_market_object, (graphene::db::object), (group_id)(description)(payout_condition) )
FC_REFLECT_DERIVED( graphene::chain::bet_object, (graphene::db::object), (bettor_id)(betting_market_id)(amount_to_bet)(backer_multiplier)(amount_reserved_for_fees)(back_or_lay) )

FC_REFLECT_DERIVED( graphene::chain::betting_market_position_object, (graphene::db::object), (bettor_id)(betting_market_id)(pay_if_payout_condition)(pay_if_not_payout_condition)(pay_if_canceled)(pay_if_not_canceled)(fees_collected) )
