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
#include <graphene/chain/database.hpp>
#include <graphene/chain/betting_market_object.hpp>
#include <graphene/chain/event_object.hpp>

namespace graphene { namespace bookie {
using namespace chain;

enum bookie_object_type
{
   persistent_event_object_type,
   persistent_betting_market_group_object_type,
   persistent_betting_market_object_type,
   persistent_bet_object_type,
   BOOKIE_OBJECT_TYPE_COUNT ///< Sentry value which contains the number of different object types
};

namespace detail
{

class persistent_event_object : public graphene::db::abstract_object<persistent_event_object>
{
   public:
      static const uint8_t space_id = bookie_objects;
      static const uint8_t type_id = persistent_event_object_type;

      event_object ephemeral_event_object;

      event_id_type get_event_id() const { return ephemeral_event_object.id; }
};

typedef object_id<bookie_objects, persistent_event_object_type, persistent_event_object> persistent_event_id_type;

struct by_event_id;
typedef multi_index_container<
   persistent_event_object,
   indexed_by<
      ordered_unique<tag<by_id>, member<object, object_id_type, &object::id> >,
      ordered_unique<tag<by_event_id>, const_mem_fun<persistent_event_object, event_id_type, &persistent_event_object::get_event_id> > > > persistent_event_multi_index_type;
typedef generic_index<persistent_event_object, persistent_event_multi_index_type> persistent_event_index;

#if 0 // we no longer have competitors, just leaving this here as an example of how to do a secondary index
class events_by_competitor_index : public secondary_index
{
   public:
      virtual ~events_by_competitor_index() {}

      virtual void object_inserted( const object& obj ) override;
      virtual void object_removed( const object& obj ) override;
      virtual void about_to_modify( const object& before ) override;
      virtual void object_modified( const object& after  ) override;
   protected:

      map<competitor_id_type, set<persistent_event_id_type> > competitor_to_events;
};

void events_by_competitor_index::object_inserted( const object& obj ) 
{
   const persistent_event_object& event_obj = *boost::polymorphic_downcast<const persistent_event_object*>(&obj);
   for (const competitor_id_type& competitor_id : event_obj.competitors)
      competitor_to_events[competitor_id].insert(event_obj.id);
   for (const competitor_id_type& competitor_id : event_obj.competitors)
      competitor_to_events[competitor_id].insert(event_obj.id);
}
void events_by_competitor_index::object_removed( const object& obj ) 
{
   const persistent_event_object& event_obj = *boost::polymorphic_downcast<const persistent_event_object*>(&obj);
   for (const competitor_id_type& competitor_id : event_obj.competitors)
      competitor_to_events[competitor_id].erase(event_obj.id);
}
void events_by_competitor_index::about_to_modify( const object& before ) 
{
   object_removed(before);
}
void events_by_competitor_index::object_modified( const object& after ) 
{
   object_inserted(after);
}
#endif

//////////// betting_market_group_object //////////////////
class persistent_betting_market_group_object : public graphene::db::abstract_object<persistent_betting_market_group_object>
{
   public:
      static const uint8_t space_id = bookie_objects;
      static const uint8_t type_id = persistent_betting_market_group_object_type;

      betting_market_group_object ephemeral_betting_market_group_object;

      share_type total_matched_bets_amount;

      betting_market_group_id_type get_betting_market_group_id() const { return ephemeral_betting_market_group_object.id; }
};

struct by_betting_market_group_id;
typedef multi_index_container<
   persistent_betting_market_group_object,
   indexed_by<
      ordered_unique<tag<by_id>, member<object, object_id_type, &object::id> >,
      ordered_unique<tag<by_betting_market_group_id>, const_mem_fun<persistent_betting_market_group_object, betting_market_group_id_type, &persistent_betting_market_group_object::get_betting_market_group_id> > > > persistent_betting_market_group_multi_index_type;

typedef generic_index<persistent_betting_market_group_object, persistent_betting_market_group_multi_index_type> persistent_betting_market_group_index;

//////////// betting_market_object //////////////////
class persistent_betting_market_object : public graphene::db::abstract_object<persistent_betting_market_object>
{
   public:
      static const uint8_t space_id = bookie_objects;
      static const uint8_t type_id = persistent_betting_market_object_type;

      betting_market_object ephemeral_betting_market_object;

      share_type total_matched_bets_amount;

      betting_market_id_type get_betting_market_id() const { return ephemeral_betting_market_object.id; }
};

struct by_betting_market_id;
typedef multi_index_container<
   persistent_betting_market_object,
   indexed_by<
      ordered_unique<tag<by_id>, member<object, object_id_type, &object::id> >,
      ordered_unique<tag<by_betting_market_id>, const_mem_fun<persistent_betting_market_object, betting_market_id_type, &persistent_betting_market_object::get_betting_market_id> > > > persistent_betting_market_multi_index_type;

typedef generic_index<persistent_betting_market_object, persistent_betting_market_multi_index_type> persistent_betting_market_index;

//////////// bet_object //////////////////
class persistent_bet_object : public graphene::db::abstract_object<persistent_bet_object>
{
   public:
      static const uint8_t space_id = bookie_objects;
      static const uint8_t type_id = persistent_bet_object_type;

      bet_object ephemeral_bet_object;

      // total amount of the bet that matched
      share_type amount_matched;

      std::vector<operation_history_id_type> associated_operations;

      bet_id_type get_bet_id() const { return ephemeral_bet_object.id; }
      account_id_type get_bettor_id() const { return ephemeral_bet_object.bettor_id; }
      bool is_matched() const { return amount_matched != share_type(); }
};

struct by_bet_id;
struct by_bettor_id;
typedef multi_index_container<
   persistent_bet_object,
   indexed_by<
      ordered_unique<tag<by_id>, member<object, object_id_type, &object::id> >,
      ordered_unique<tag<by_bet_id>, const_mem_fun<persistent_bet_object, bet_id_type, &persistent_bet_object::get_bet_id> >,
      ordered_unique<tag<by_bettor_id>, 
            composite_key<
               persistent_bet_object,
               const_mem_fun<persistent_bet_object, account_id_type, &persistent_bet_object::get_bettor_id>,
               const_mem_fun<persistent_bet_object, bool, &persistent_bet_object::is_matched>,
               const_mem_fun<persistent_bet_object, bet_id_type, &persistent_bet_object::get_bet_id> >,
            composite_key_compare<
               std::less<account_id_type>,
               std::less<bool>,
               std::greater<bet_id_type> > > > > persistent_bet_multi_index_type;

typedef generic_index<persistent_bet_object, persistent_bet_multi_index_type> persistent_bet_index;

} } } //graphene::bookie::detail

FC_REFLECT_DERIVED( graphene::bookie::detail::persistent_event_object, (graphene::db::object), (ephemeral_event_object) )
FC_REFLECT_DERIVED( graphene::bookie::detail::persistent_betting_market_group_object, (graphene::db::object), (ephemeral_betting_market_group_object)(total_matched_bets_amount) )
FC_REFLECT_DERIVED( graphene::bookie::detail::persistent_betting_market_object, (graphene::db::object), (ephemeral_betting_market_object) )
FC_REFLECT_DERIVED( graphene::bookie::detail::persistent_bet_object, (graphene::db::object), (ephemeral_bet_object)(amount_matched)(associated_operations) )

