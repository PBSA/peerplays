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

#include <graphene/bookie/bookie_plugin.hpp>

#include <graphene/app/impacted.hpp>

#include <graphene/chain/account_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/betting_market_object.hpp>
#include <graphene/chain/config.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>

#include <boost/algorithm/string/case_conv.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/thread/thread.hpp>

#include <boost/polymorphic_cast.hpp>

#if 0
# ifdef DEFAULT_LOGGER
#  undef DEFAULT_LOGGER
# endif
# define DEFAULT_LOGGER "bookie_plugin"
#endif

namespace graphene { namespace bookie {

namespace detail
{

class persistent_event_object : public graphene::db::abstract_object<event_object>
{
   public:
      static const uint8_t space_id = bookie_objects;
      static const uint8_t type_id = persistent_event_object_type;

      event_id_type event_object_id;

      internationalized_string_type name;
      
      internationalized_string_type season;

      optional<time_point_sec> start_time;

      event_group_id_type event_group_id;

      event_status status;
      vector<string> scores;
};
typedef object_id<bookie_objects, persistent_event_object_type, persistent_event_object> persistent_event_id_type;

struct by_event_id;
typedef multi_index_container<
   persistent_event_object,
   indexed_by<
      ordered_unique<tag<by_id>, member<object, object_id_type, &object::id> >,
      ordered_unique<tag<by_event_id>, member<persistent_event_object, event_id_type, &persistent_event_object::event_object_id> > > > persistent_event_object_multi_index_type;

typedef generic_index<persistent_event_object, persistent_event_object_multi_index_type> persistent_event_object_index;

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

class bookie_plugin_impl
{
   public:
      bookie_plugin_impl(bookie_plugin& _plugin)
         : _self( _plugin )
      { }
      virtual ~bookie_plugin_impl();


      /**
       *  Called After a block has been applied and committed.  The callback
       *  should not yield and should execute quickly.
       */
      void on_objects_changed(const vector<object_id_type>& changed_object_ids);

      /** this method is called as a callback after a block is applied
       * and will process/index all operations that were applied in the block.
       */
      void on_block_applied( const signed_block& b );

      asset get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id);

      void fill_localized_event_strings();

      void get_events_containing_sub_string(std::vector<event_object>& events, const std::string& sub_string, const std::string& language);

      graphene::chain::database& database()
      {
         return _self.database();
      }

      //                1.18.          "Washington Capitals/Chicago Blackhawks"
      typedef std::pair<event_id_type, std::string> event_string;
      struct event_string_less : public std::less<const event_string&>
      {
          bool operator()(const event_string &_left, const event_string &_right) const
          {
              return (_left.first.instance < _right.first.instance);
          }
      };

      typedef flat_set<event_string, event_string_less> event_string_set;
      //       "en"
      std::map<std::string, event_string_set> localized_event_strings;

      bookie_plugin& _self;
      flat_set<account_id_type> _tracked_accounts;
};

bookie_plugin_impl::~bookie_plugin_impl()
{
   return;
}

void bookie_plugin_impl::on_objects_changed(const vector<object_id_type>& changed_object_ids)
{
   graphene::chain::database& db = database();
   auto& event_id_index = db.get_index_type<persistent_event_object_index>().indices().get<by_event_id>();

   for (const object_id_type& changed_object_id : changed_object_ids)
   {
      if (changed_object_id.space() == event_id_type::space_id && 
          changed_object_id.type() == event_id_type::type_id)
      {
         event_id_type changed_event_id = changed_object_id;
         const event_object* new_event_obj = nullptr;
         try
         {
            new_event_obj = &changed_event_id(db);
         }
         catch (fc::exception& e)
         {
         }
         // new_event_obj should point to the now-changed event_object, or null if it was removed from the database

         const persistent_event_object* old_event_obj = nullptr;

         auto persistent_event_iter = event_id_index.find(changed_event_id);
         if (persistent_event_iter != event_id_index.end())
            old_event_obj = &*persistent_event_iter;
         
         // and old_event_obj is a pointer to our saved copy, or nullptr if it is a new object
         if (old_event_obj && new_event_obj)
         {
            ilog("Modifying persistent event object ${id}", ("id", changed_event_id));
            db.modify(*old_event_obj, [&](persistent_event_object& saved_event_obj) {
               saved_event_obj.name = new_event_obj->name;
               saved_event_obj.season = new_event_obj->season;
               saved_event_obj.start_time = new_event_obj->start_time;;
               saved_event_obj.event_group_id = new_event_obj->event_group_id;
               saved_event_obj.status = new_event_obj->status;
               saved_event_obj.scores = new_event_obj->scores;
            });
         }
         else if (new_event_obj)
         {
            ilog("Creating new persistent event object ${id}", ("id", changed_event_id));
            db.create<persistent_event_object>([&](persistent_event_object& saved_event_obj) {
               saved_event_obj.event_object_id = new_event_obj->id;
               saved_event_obj.name = new_event_obj->name;
               saved_event_obj.season = new_event_obj->season;
               saved_event_obj.start_time = new_event_obj->start_time;;
               saved_event_obj.event_group_id = new_event_obj->event_group_id;
               saved_event_obj.status = new_event_obj->status;
               saved_event_obj.scores = new_event_obj->scores;
            });
         }
      }
   }
}

void bookie_plugin_impl::on_block_applied( const signed_block& )
{
   graphene::chain::database& db = database();
   const vector<optional<operation_history_object> >& hist = db.get_applied_operations();
   for( const optional< operation_history_object >& o_op : hist )
   {
      if( !o_op.valid() )
      {
         continue;
      }
      const operation_history_object& op = *o_op;
      if( op.op.which() == operation::tag< bet_matched_operation >::value )
      {
           const bet_matched_operation& bet_matched_op = op.op.get<bet_matched_operation>();
           idump((bet_matched_op));
           const asset& amount_bet = bet_matched_op.amount_bet;
           // object may no longer exist
           //const bet_object& bet = bet_matched_op.bet_id(db);
           const betting_market_object& betting_market = bet_matched_op.betting_market_id(db);
           const betting_market_group_object& betting_market_group = betting_market.group_id(db);
           db.modify( betting_market_group, [&]( betting_market_group_object& obj ){
               obj.total_matched_bets_amount += amount_bet.amount;
           });
      }
      else if( op.op.which() == operation::tag< event_create_operation >::value )
      {
          FC_ASSERT(op.result.which() == operation_result::tag< object_id_type >::value);
          //object_id_type object_id = op.result.get<object_id_type>();
          event_id_type object_id = op.result.get<object_id_type>();
          FC_ASSERT( db.find_object(object_id), "invalid event specified" );
          const event_create_operation& event_create_op = op.op.get<event_create_operation>();
          for(const std::pair<std::string, std::string>& pair : event_create_op.name)
          {
              localized_event_strings[pair.first].insert(event_string(object_id, pair.second));
          }
      }
      else if( op.op.which() == operation::tag< event_update_operation >::value )
      {
          const event_update_operation& event_create_op = op.op.get<event_update_operation>();
          if (!event_create_op.new_name.valid())
              continue;
          event_id_type event_id = event_create_op.event_id;
          for(const std::pair<std::string, std::string>& pair : *event_create_op.new_name)
          {
              // try insert
              std::pair<event_string_set::iterator, bool> result =
                       localized_event_strings[pair.first].insert(event_string(event_id, pair.second));
              if (!result.second)
                   //  update string only
                   result.first->second = pair.second;
          }
      }
   }
}

void bookie_plugin_impl::fill_localized_event_strings()
{
       graphene::chain::database& db = database();
       const auto& event_index = db.get_index_type<event_object_index>().indices().get<by_id>();
       auto event_itr = event_index.cbegin();
       while (event_itr != event_index.cend())
       {
           const event_object& event_obj = *event_itr;
           ++event_itr;
           for(const std::pair<std::string, std::string>& pair : event_obj.name)
           {
                localized_event_strings[pair.first].insert(event_string(event_obj.id, pair.second));
           }
       }
}

void bookie_plugin_impl::get_events_containing_sub_string(std::vector<event_object>& events, const std::string& sub_string, const std::string& language)
{
    graphene::chain::database& db = database();
    if (localized_event_strings.find(language) != localized_event_strings.end())
    {
        std::string lower_case_sub_string = boost::algorithm::to_lower_copy(sub_string);
        const event_string_set& language_set = localized_event_strings[language];
        for (const event_string& pair : language_set)
        {
               std::string lower_case_string = boost::algorithm::to_lower_copy(pair.second);
               if (lower_case_string.find(lower_case_sub_string) != std::string::npos)
               events.push_back(pair.first(db));
        }
    }
}

asset bookie_plugin_impl::get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id)
{
    graphene::chain::database& db = database();
    FC_ASSERT( db.find_object(group_id), "Invalid betting market group specified" );
    const betting_market_group_object& betting_market_group =  group_id(db);
    return asset(betting_market_group.total_matched_bets_amount, betting_market_group.asset_id);
}
} // end namespace detail

bookie_plugin::bookie_plugin() :
   my( new detail::bookie_plugin_impl(*this) )
{
}

bookie_plugin::~bookie_plugin()
{
}

std::string bookie_plugin::plugin_name()const
{
   return "bookie";
}

void bookie_plugin::plugin_set_program_options(
   boost::program_options::options_description& cli,
   boost::program_options::options_description& cfg
   )
{
   //cli.add_options()
   //      ("track-account", boost::program_options::value<std::vector<std::string>>()->composing()->multitoken(), "Account ID to track history for (may specify multiple times)")
   //      ;
   //cfg.add(cli);
}

void bookie_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
    ilog("bookie plugin: plugin_startup() begin");
    database().applied_block.connect( [&]( const signed_block& b){ my->on_block_applied(b); } );
    database().changed_objects.connect([&](const vector<object_id_type>& changed_object_ids, const fc::flat_set<graphene::chain::account_id_type>& impacted_accounts){ my->on_objects_changed(changed_object_ids); });
    //auto event_index =
    database().add_index<primary_index<detail::persistent_event_object_index> >();
    //event_index->add_secondary_index<detail::events_by_competitor_index>();
     //LOAD_VALUE_SET(options, "tracked-accounts", my->_tracked_accounts, graphene::chain::account_id_type);

    ilog("bookie plugin: plugin_startup() end");
 }

void bookie_plugin::plugin_startup()
{
   ilog("bookie plugin: plugin_startup()");
    my->fill_localized_event_strings();
}

flat_set<account_id_type> bookie_plugin::tracked_accounts() const
{
   return my->_tracked_accounts;
}

asset bookie_plugin::get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id)
{
     ilog("bookie plugin: get_total_matched_bet_amount_for_betting_market_group($group_id)", ("group_d", group_id));
     return my->get_total_matched_bet_amount_for_betting_market_group(group_id);
}
void bookie_plugin::get_events_containing_sub_string(std::vector<event_object>& events, const std::string& sub_string, const std::string& language)
{
    ilog("bookie plugin: get_events_containing_sub_string($s, $l)", ("s", sub_string)("l", language));
    my->get_events_containing_sub_string(events, sub_string, language);
}

} }
FC_REFLECT_DERIVED( graphene::bookie::detail::persistent_event_object, (graphene::db::object), (event_object_id)(name)(season)(start_time)(event_group_id)(status)(scores) )

