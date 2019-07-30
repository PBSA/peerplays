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
#include <graphene/bookie/bookie_plugin.hpp>
#include <graphene/bookie/bookie_objects.hpp>

#include <graphene/app/impacted.hpp>

#include <graphene/chain/account_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/config.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
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
/* As a plugin, we get notified of new/changed objects at the end of every block processed.
 * For most objects, that's fine, because we expect them to always be around until the end of
 * the block.  However, with bet objects, it's possible that the user places a bet and it fills
 * and is removed during the same block, so need another strategy to detect them immediately after
 * they are created. 
 * We do this by creating a secondary index on bet_object.  We don't actually use it
 * to index any property of the bet, we just use it to register for callbacks.
 */
class persistent_bet_object_helper : public secondary_index
{
   public:
      virtual ~persistent_bet_object_helper() {}

      virtual void object_inserted(const object& obj) override;
      //virtual void object_removed( const object& obj ) override;
      //virtual void about_to_modify( const object& before ) override;
      virtual void object_modified(const object& after) override;
      void set_plugin_instance(bookie_plugin* instance) { _bookie_plugin = instance; }
   private:
      bookie_plugin* _bookie_plugin;
};

void persistent_bet_object_helper::object_inserted(const object& obj) 
{
   const bet_object& bet_obj = *boost::polymorphic_downcast<const bet_object*>(&obj);
   _bookie_plugin->database().create<persistent_bet_object>([&](persistent_bet_object& saved_bet_obj) {
      saved_bet_obj.ephemeral_bet_object = bet_obj;
   });
}
void persistent_bet_object_helper::object_modified(const object& after) 
{
   database& db = _bookie_plugin->database();
   auto& persistent_bets_by_bet_id = db.get_index_type<persistent_bet_index>().indices().get<by_bet_id>();
   const bet_object& bet_obj = *boost::polymorphic_downcast<const bet_object*>(&after);
   auto iter = persistent_bets_by_bet_id.find(bet_obj.id);
   assert (iter != persistent_bets_by_bet_id.end());
   if (iter != persistent_bets_by_bet_id.end())
      db.modify(*iter, [&](persistent_bet_object& saved_bet_obj) {
         saved_bet_obj.ephemeral_bet_object = bet_obj;
      });
}

//////////// end bet_object ///////////////////
class persistent_betting_market_object_helper : public secondary_index
{
   public:
      virtual ~persistent_betting_market_object_helper() {}

      virtual void object_inserted(const object& obj) override;
      //virtual void object_removed( const object& obj ) override;
      //virtual void about_to_modify( const object& before ) override;
      virtual void object_modified(const object& after) override;
      void set_plugin_instance(bookie_plugin* instance) { _bookie_plugin = instance; }
   private:
      bookie_plugin* _bookie_plugin;
};

void persistent_betting_market_object_helper::object_inserted(const object& obj) 
{
   const betting_market_object& betting_market_obj = *boost::polymorphic_downcast<const betting_market_object*>(&obj);
   _bookie_plugin->database().create<persistent_betting_market_object>([&](persistent_betting_market_object& saved_betting_market_obj) {
      saved_betting_market_obj.ephemeral_betting_market_object = betting_market_obj;
   });
}
void persistent_betting_market_object_helper::object_modified(const object& after) 
{
   database& db = _bookie_plugin->database();
   auto& persistent_betting_markets_by_betting_market_id = db.get_index_type<persistent_betting_market_index>().indices().get<by_betting_market_id>();
   const betting_market_object& betting_market_obj = *boost::polymorphic_downcast<const betting_market_object*>(&after);
   auto iter = persistent_betting_markets_by_betting_market_id.find(betting_market_obj.id);
   assert (iter != persistent_betting_markets_by_betting_market_id.end());
   if (iter != persistent_betting_markets_by_betting_market_id.end())
      db.modify(*iter, [&](persistent_betting_market_object& saved_betting_market_obj) {
         saved_betting_market_obj.ephemeral_betting_market_object = betting_market_obj;
      });
}

//////////// end betting_market_object ///////////////////
class persistent_betting_market_group_object_helper : public secondary_index
{
   public:
      virtual ~persistent_betting_market_group_object_helper() {}

      virtual void object_inserted(const object& obj) override;
      //virtual void object_removed( const object& obj ) override;
      //virtual void about_to_modify( const object& before ) override;
      virtual void object_modified(const object& after) override;
      void set_plugin_instance(bookie_plugin* instance) { _bookie_plugin = instance; }
   private:
      bookie_plugin* _bookie_plugin;
};

void persistent_betting_market_group_object_helper::object_inserted(const object& obj) 
{
   const betting_market_group_object& betting_market_group_obj = *boost::polymorphic_downcast<const betting_market_group_object*>(&obj);
   _bookie_plugin->database().create<persistent_betting_market_group_object>([&](persistent_betting_market_group_object& saved_betting_market_group_obj) {
      saved_betting_market_group_obj.ephemeral_betting_market_group_object = betting_market_group_obj;
   });
}
void persistent_betting_market_group_object_helper::object_modified(const object& after) 
{
   database& db = _bookie_plugin->database();
   auto& persistent_betting_market_groups_by_betting_market_group_id = db.get_index_type<persistent_betting_market_group_index>().indices().get<by_betting_market_group_id>();
   const betting_market_group_object& betting_market_group_obj = *boost::polymorphic_downcast<const betting_market_group_object*>(&after);
   auto iter = persistent_betting_market_groups_by_betting_market_group_id.find(betting_market_group_obj.id);
   assert (iter != persistent_betting_market_groups_by_betting_market_group_id.end());
   if (iter != persistent_betting_market_groups_by_betting_market_group_id.end())
      db.modify(*iter, [&](persistent_betting_market_group_object& saved_betting_market_group_obj) {
         saved_betting_market_group_obj.ephemeral_betting_market_group_object = betting_market_group_obj;
      });
}

//////////// end betting_market_group_object ///////////////////
class persistent_event_object_helper : public secondary_index
{
   public:
      virtual ~persistent_event_object_helper() {}

      virtual void object_inserted(const object& obj) override;
      //virtual void object_removed( const object& obj ) override;
      //virtual void about_to_modify( const object& before ) override;
      virtual void object_modified(const object& after) override;
      void set_plugin_instance(bookie_plugin* instance) { _bookie_plugin = instance; }
   private:
      bookie_plugin* _bookie_plugin;
};

void persistent_event_object_helper::object_inserted(const object& obj) 
{
   const event_object& event_obj = *boost::polymorphic_downcast<const event_object*>(&obj);
   _bookie_plugin->database().create<persistent_event_object>([&](persistent_event_object& saved_event_obj) {
      saved_event_obj.ephemeral_event_object = event_obj;
   });
}
void persistent_event_object_helper::object_modified(const object& after) 
{
   database& db = _bookie_plugin->database();
   auto& persistent_events_by_event_id = db.get_index_type<persistent_event_index>().indices().get<by_event_id>();
   const event_object& event_obj = *boost::polymorphic_downcast<const event_object*>(&after);
   auto iter = persistent_events_by_event_id.find(event_obj.id);
   assert (iter != persistent_events_by_event_id.end());
   if (iter != persistent_events_by_event_id.end())
      db.modify(*iter, [&](persistent_event_object& saved_event_obj) {
         saved_event_obj.ephemeral_event_object = event_obj;
      });
}

//////////// end event_object ///////////////////
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

      void on_objects_new(const vector<object_id_type>& new_object_ids);
      void on_objects_removed(const vector<object_id_type>& removed_object_ids);

      /** this method is called as a callback after a block is applied
       * and will process/index all operations that were applied in the block.
       */
      void on_block_applied( const signed_block& b );

      asset get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id);

      void fill_localized_event_strings();

      std::vector<event_object> get_events_containing_sub_string(const std::string& sub_string, const std::string& language);

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
}

void bookie_plugin_impl::on_objects_new(const vector<object_id_type>& new_object_ids)
{
}

void bookie_plugin_impl::on_objects_removed(const vector<object_id_type>& removed_object_ids)
{
}

void bookie_plugin_impl::on_objects_changed(const vector<object_id_type>& changed_object_ids)
{
}

bool is_operation_history_object_stored(operation_history_id_type id)
{
   if (id == operation_history_id_type())
   {
      elog("Warning: the operation history object for an operation the bookie plugin needs to track "
           "has id of ${id}, which means the account history plugin isn't storing this operation, or that "
           "it is running after the bookie plugin. Make sure the account history plugin is tracking operations for "
           "all accounts,, and that it is loaded before the bookie plugin", ("id", id));
      return false;
   }
   else
      return true;
}

void bookie_plugin_impl::on_block_applied( const signed_block& )
{ try {

   graphene::chain::database& db = database();
   const vector<optional<operation_history_object> >& hist = db.get_applied_operations();
   for( const optional<operation_history_object>& o_op : hist )
   {
      if( !o_op.valid() )
         continue;

      const operation_history_object& op = *o_op;
      if( op.op.which() == operation::tag<bet_matched_operation>::value )
      {
         const bet_matched_operation& bet_matched_op = op.op.get<bet_matched_operation>();
         //idump((bet_matched_op));
         const asset& amount_bet = bet_matched_op.amount_bet;
         // object may no longer exist
         //const bet_object& bet = bet_matched_op.bet_id(db);
         auto& persistent_bets_by_bet_id = db.get_index_type<persistent_bet_index>().indices().get<by_bet_id>();
         auto bet_iter = persistent_bets_by_bet_id.find(bet_matched_op.bet_id);
         assert(bet_iter != persistent_bets_by_bet_id.end());
         if (bet_iter != persistent_bets_by_bet_id.end())
         {
            db.modify(*bet_iter, [&]( persistent_bet_object& obj ) {
               obj.amount_matched += amount_bet.amount;
               if (is_operation_history_object_stored(op.id))
                  obj.associated_operations.emplace_back(op.id);
            });
            const bet_object& bet_obj = bet_iter->ephemeral_bet_object;

            auto& persistent_betting_market_idx = db.get_index_type<persistent_betting_market_index>().indices().get<by_betting_market_id>();
            auto persistent_betting_market_object_iter = persistent_betting_market_idx.find(bet_obj.betting_market_id);
            FC_ASSERT(persistent_betting_market_object_iter != persistent_betting_market_idx.end());
            const betting_market_object& betting_market = persistent_betting_market_object_iter->ephemeral_betting_market_object;

            auto& persistent_betting_market_group_idx = db.get_index_type<persistent_betting_market_group_index>().indices().get<by_betting_market_group_id>();
            auto persistent_betting_market_group_object_iter = persistent_betting_market_group_idx.find(betting_market.group_id);
            FC_ASSERT(persistent_betting_market_group_object_iter != persistent_betting_market_group_idx.end());
            const betting_market_group_object& betting_market_group = persistent_betting_market_group_object_iter->ephemeral_betting_market_group_object;

            // if the object is still in the main database, keep the running total there
            // otherwise, add it directly to the persistent version
            auto& betting_market_group_idx = db.get_index_type<betting_market_group_object_index>().indices().get<by_id>();
            auto betting_market_group_iter = betting_market_group_idx.find(betting_market_group.id);
            if (betting_market_group_iter != betting_market_group_idx.end())
               db.modify( *betting_market_group_iter, [&]( betting_market_group_object& obj ){
                  obj.total_matched_bets_amount += amount_bet.amount;
               });
            else
               db.modify( *persistent_betting_market_group_object_iter, [&]( persistent_betting_market_group_object& obj ){
                  obj.ephemeral_betting_market_group_object.total_matched_bets_amount += amount_bet.amount;
               });
         }
      }
      else if( op.op.which() == operation::tag<event_create_operation>::value )
      {
         FC_ASSERT(op.result.which() == operation_result::tag<object_id_type>::value);
         //object_id_type object_id = op.result.get<object_id_type>();
         event_id_type object_id = op.result.get<object_id_type>();
         FC_ASSERT( db.find_object(object_id), "invalid event specified" );
         const event_create_operation& event_create_op = op.op.get<event_create_operation>();
         for(const std::pair<std::string, std::string>& pair : event_create_op.name)
            localized_event_strings[pair.first].insert(event_string(object_id, pair.second));
      }
      else if( op.op.which() == operation::tag<event_update_operation>::value )
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
      else if ( op.op.which() == operation::tag<bet_canceled_operation>::value )
      {
         const bet_canceled_operation& bet_canceled_op = op.op.get<bet_canceled_operation>();
         auto& persistent_bets_by_bet_id = db.get_index_type<persistent_bet_index>().indices().get<by_bet_id>();
         auto bet_iter = persistent_bets_by_bet_id.find(bet_canceled_op.bet_id);
         assert(bet_iter != persistent_bets_by_bet_id.end());
         if (bet_iter != persistent_bets_by_bet_id.end())
         {
            ilog("Adding bet_canceled_operation ${canceled_id} to bet ${bet_id}'s associated operations", 
                 ("canceled_id", op.id)("bet_id", bet_canceled_op.bet_id));
            if (is_operation_history_object_stored(op.id))
               db.modify(*bet_iter, [&]( persistent_bet_object& obj ) {
                  obj.associated_operations.emplace_back(op.id);
               });
         }
      }
      else if ( op.op.which() == operation::tag<bet_adjusted_operation>::value )
      {
         const bet_adjusted_operation& bet_adjusted_op = op.op.get<bet_adjusted_operation>();
         auto& persistent_bets_by_bet_id = db.get_index_type<persistent_bet_index>().indices().get<by_bet_id>();
         auto bet_iter = persistent_bets_by_bet_id.find(bet_adjusted_op.bet_id);
         assert(bet_iter != persistent_bets_by_bet_id.end());
         if (bet_iter != persistent_bets_by_bet_id.end())
         {
            ilog("Adding bet_adjusted_operation ${adjusted_id} to bet ${bet_id}'s associated operations", 
                 ("adjusted_id", op.id)("bet_id", bet_adjusted_op.bet_id));
            if (is_operation_history_object_stored(op.id))
               db.modify(*bet_iter, [&]( persistent_bet_object& obj ) {
                  obj.associated_operations.emplace_back(op.id);
               });
         }
      }

   }
} FC_RETHROW_EXCEPTIONS( warn, "" ) }

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

std::vector<event_object> bookie_plugin_impl::get_events_containing_sub_string(const std::string& sub_string, const std::string& language)
{
   graphene::chain::database& db = database();
   std::vector<event_object> events;
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
   return events;
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

void bookie_plugin::plugin_set_program_options(boost::program_options::options_description& cli, 
                                               boost::program_options::options_description& cfg)
{
   //cli.add_options()
   //      ("track-account", boost::program_options::value<std::vector<std::string>>()->composing()->multitoken(), "Account ID to track history for (may specify multiple times)")
   //      ;
   //cfg.add(cli);
}

void bookie_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
    ilog("bookie plugin: plugin_startup() begin");
    database().force_slow_replays();
    database().applied_block.connect( [&]( const signed_block& b){ my->on_block_applied(b); } );
    database().changed_objects.connect([&](const vector<object_id_type>& changed_object_ids, const fc::flat_set<graphene::chain::account_id_type>& impacted_accounts){ my->on_objects_changed(changed_object_ids); });
    database().new_objects.connect([this](const vector<object_id_type>& ids, const flat_set<account_id_type>& impacted_accounts) { my->on_objects_new(ids); });
    database().removed_objects.connect([this](const vector<object_id_type>& ids, const vector<const object*>& objs, const flat_set<account_id_type>& impacted_accounts) { my->on_objects_removed(ids); });


    //auto event_index =
    database().add_index<primary_index<detail::persistent_event_index> >();
    database().add_index<primary_index<detail::persistent_betting_market_group_index> >();
    database().add_index<primary_index<detail::persistent_betting_market_index> >();
    database().add_index<primary_index<detail::persistent_bet_index> >();
    const primary_index<bet_object_index>& bet_object_idx = database().get_index_type<primary_index<bet_object_index> >();
    primary_index<bet_object_index>& nonconst_bet_object_idx = const_cast<primary_index<bet_object_index>&>(bet_object_idx);
    detail::persistent_bet_object_helper* persistent_bet_object_helper_index = nonconst_bet_object_idx.add_secondary_index<detail::persistent_bet_object_helper>();
    persistent_bet_object_helper_index->set_plugin_instance(this);

    const primary_index<betting_market_object_index>& betting_market_object_idx = database().get_index_type<primary_index<betting_market_object_index> >();
    primary_index<betting_market_object_index>& nonconst_betting_market_object_idx = const_cast<primary_index<betting_market_object_index>&>(betting_market_object_idx);
    detail::persistent_betting_market_object_helper* persistent_betting_market_object_helper_index = nonconst_betting_market_object_idx.add_secondary_index<detail::persistent_betting_market_object_helper>();
    persistent_betting_market_object_helper_index->set_plugin_instance(this);

    const primary_index<betting_market_group_object_index>& betting_market_group_object_idx = database().get_index_type<primary_index<betting_market_group_object_index> >();
    primary_index<betting_market_group_object_index>& nonconst_betting_market_group_object_idx = const_cast<primary_index<betting_market_group_object_index>&>(betting_market_group_object_idx);
    detail::persistent_betting_market_group_object_helper* persistent_betting_market_group_object_helper_index = nonconst_betting_market_group_object_idx.add_secondary_index<detail::persistent_betting_market_group_object_helper>();
    persistent_betting_market_group_object_helper_index->set_plugin_instance(this);

    const primary_index<event_object_index>& event_object_idx = database().get_index_type<primary_index<event_object_index> >();
    primary_index<event_object_index>& nonconst_event_object_idx = const_cast<primary_index<event_object_index>&>(event_object_idx);
    detail::persistent_event_object_helper* persistent_event_object_helper_index = nonconst_event_object_idx.add_secondary_index<detail::persistent_event_object_helper>();
    persistent_event_object_helper_index->set_plugin_instance(this);

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
std::vector<event_object> bookie_plugin::get_events_containing_sub_string(const std::string& sub_string, const std::string& language)
{
    ilog("bookie plugin: get_events_containing_sub_string(${sub_string}, ${language})", (sub_string)(language));
    return my->get_events_containing_sub_string(sub_string, language);
}

} }

