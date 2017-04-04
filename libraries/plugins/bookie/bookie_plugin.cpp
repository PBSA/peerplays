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
#include <graphene/chain/config.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>

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

      vector<competitor_id_type> competitors;

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

      graphene::chain::database& database()
      {
         return _self.database();
      }

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
               saved_event_obj.competitors = new_event_obj->competitors;
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
               saved_event_obj.competitors = new_event_obj->competitors;
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
   const vector<optional< operation_history_object > >& hist = db.get_applied_operations();
   for( const optional< operation_history_object >& o_op : hist )
   {
      // add to the operation history index
      const auto& oho = db.create<operation_history_object>( [&]( operation_history_object& h )
      {
         if( o_op.valid() )
            h = *o_op;
      } );

      if( !o_op.valid() )
      {
         ilog( "removing failed operation with ID: ${id}", ("id", oho.id) );
         db.remove( oho );
         continue;
      }

      const operation_history_object& op = *o_op;

      // get the set of accounts this operation applies to
      flat_set<account_id_type> impacted;
      vector<authority> other;
      operation_get_required_authorities( op.op, impacted, impacted, other );

      if( op.op.which() == operation::tag< account_create_operation >::value )
         impacted.insert( oho.result.get<object_id_type>() );
      else
         graphene::app::operation_get_impacted_accounts( op.op, impacted );

      for( auto& a : other )
         for( auto& item : a.account_auths )
            impacted.insert( item.first );

      // for each operation this account applies to that is in the config link it into the history
      if( _tracked_accounts.size() == 0 )
      {
         for( auto& account_id : impacted )
         {
            // we don't do index_account_keys here anymore, because
            // that indexing now happens in observers' post_evaluate()

            // add history
            const auto& stats_obj = account_id(db).statistics(db);
            const auto& ath = db.create<account_transaction_history_object>( [&]( account_transaction_history_object& obj ){
                obj.operation_id = oho.id;
                obj.next = stats_obj.most_recent_op;
            });
            db.modify( stats_obj, [&]( account_statistics_object& obj ){
                obj.most_recent_op = ath.id;
            });
         }
      }
      else
      {
         for( auto account_id : _tracked_accounts )
         {
            if( impacted.find( account_id ) != impacted.end() )
            {
               // add history
               const auto& stats_obj = account_id(db).statistics(db);
               const auto& ath = db.create<account_transaction_history_object>( [&]( account_transaction_history_object& obj ){
                   obj.operation_id = oho.id;
                   obj.next = stats_obj.most_recent_op;
               });
               db.modify( stats_obj, [&]( account_statistics_object& obj ){
                   obj.most_recent_op = ath.id;
               });
            }
         }
      }
   }
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
   cli.add_options()
         ("track-account", boost::program_options::value<std::vector<std::string>>()->composing()->multitoken(), "Account ID to track history for (may specify multiple times)")
         ;
   cfg.add(cli);
}

void bookie_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
   database().applied_block.connect( [&]( const signed_block& b){ my->on_block_applied(b); } );
   database().changed_objects.connect([&](const vector<object_id_type>& changed_object_ids){ my->on_objects_changed(changed_object_ids); });
   auto event_index = database().add_index<primary_index<detail::persistent_event_object_index> >();
   event_index->add_secondary_index<detail::events_by_competitor_index>();

   LOAD_VALUE_SET(options, "tracked-accounts", my->_tracked_accounts, graphene::chain::account_id_type);
}

void bookie_plugin::plugin_startup()
{
}

flat_set<account_id_type> bookie_plugin::tracked_accounts() const
{
   return my->_tracked_accounts;
}

} }
FC_REFLECT_DERIVED( graphene::bookie::detail::persistent_event_object, (graphene::db::object), (event_object_id)(name)(season)(start_time)(event_group_id)(status)(competitors)(scores) )

