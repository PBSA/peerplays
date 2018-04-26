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

#include <graphene/affiliate_stats/affiliate_stats_plugin.hpp>
#include <graphene/affiliate_stats/affiliate_stats_objects.hpp>

#include <graphene/app/impacted.hpp>

#include <graphene/chain/account_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/config.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/thread/thread.hpp>

namespace graphene { namespace affiliate_stats {

namespace detail {

class affiliate_reward_index : public graphene::db::index_observer
{
   public:
      affiliate_reward_index( graphene::chain::database& _db ) : db(_db) {}
      virtual void on_add( const graphene::db::object& obj ) override;
      virtual void on_remove( const graphene::db::object& obj ) override;
      virtual void on_modify( const graphene::db::object& before ) override{};

      std::map<graphene::chain::account_id_type, std::set<graphene::chain::operation_history_id_type> > _history_by_account;
   private:
      graphene::chain::database& db;
};

class affiliate_stats_plugin_impl
{
   public:
      affiliate_stats_plugin_impl(affiliate_stats_plugin& _plugin)
         : _self( _plugin ) { }
      virtual ~affiliate_stats_plugin_impl();

      /** this method is called as a callback after a block is applied
       * and will process/index all operations that were applied in the block.
       */
      void update_affiliate_stats( const signed_block& b );

      graphene::chain::database& database()
      {
         return _self.database();
      }

      const std::set<graphene::chain::operation_history_id_type>& get_reward_history( account_id_type& affiliate )const;

      typedef void result_type;
      template<typename Operation>
      void operator()( const Operation& op ) {}

      shared_ptr<affiliate_reward_index> _fr_index;
      affiliate_stats_plugin&            _self;
      app_reward_index*                  _ar_index;
      referral_reward_index*             _rr_index;
   private:
};

affiliate_stats_plugin_impl::~affiliate_stats_plugin_impl() {}

template<>
void affiliate_stats_plugin_impl::operator()( const affiliate_payout_operation& op )
{
    auto& by_app = _ar_index->indices().get<by_app_asset>();
    auto itr = by_app.find( boost::make_tuple( op.tag, op.payout.asset_id ) );
    if( itr == by_app.end() )
    {
       database().create<app_reward_object>( [&op]( app_reward_object& aro ) {
          aro.app = op.tag;
          aro.total_payout = op.payout;
       });
    }
    else
    {
       database().modify( *itr, [&op]( app_reward_object& aro ) {
          aro.total_payout += op.payout;
       });
    }
}

template<>
void affiliate_stats_plugin_impl::operator()( const affiliate_referral_payout_operation& op )
{
    auto& by_referral = _rr_index->indices().get<by_referral_asset>();
    auto itr = by_referral.find( boost::make_tuple( op.player, op.payout.asset_id ) );
    if( itr == by_referral.end() )
    {
       database().create<referral_reward_object>( [&op]( referral_reward_object& rro ) {
          rro.referral = op.player;
          rro.total_payout = op.payout;
       });
    }
    else
    {
       database().modify( *itr, [&op]( referral_reward_object& rro ) {
          rro.total_payout += op.payout;
       });
    }
}

void affiliate_stats_plugin_impl::update_affiliate_stats( const signed_block& b )
{
   vector<optional< operation_history_object > >& hist = database().get_applied_operations();
   for( optional< operation_history_object >& o_op : hist )
   {
      if( !o_op.valid() )
         continue;

      o_op->op.visit( *this );
   }
}

static const std::set<graphene::chain::operation_history_id_type> EMPTY;
const std::set<graphene::chain::operation_history_id_type>& affiliate_stats_plugin_impl::get_reward_history( account_id_type& affiliate )const
{
    auto itr = _fr_index->_history_by_account.find( affiliate );
    if( itr == _fr_index->_history_by_account.end() )
       return EMPTY;
    return itr->second;
}


static optional<std::pair<account_id_type, operation_history_id_type>> get_account( const database& db, const object& obj )
{
   FC_ASSERT( dynamic_cast<const account_transaction_history_object*>(&obj) );
   const account_transaction_history_object& ath = static_cast<const account_transaction_history_object&>(obj);
   const operation_history_object& oho = db.get<operation_history_object>( ath.operation_id );
   if( oho.op.which() == operation::tag<affiliate_payout_operation>::value )
      return std::make_pair( ath.account, ath.operation_id );
   return optional<std::pair<account_id_type, operation_history_id_type>>();
}

void affiliate_reward_index::on_add( const object& obj )
{
   optional<std::pair<account_id_type, operation_history_id_type>> acct_ath = get_account( db, obj );
   if( !acct_ath.valid() ) return;
   _history_by_account[acct_ath->first].insert( acct_ath->second );
}

void affiliate_reward_index::on_remove( const object& obj )
{
   optional<std::pair<account_id_type, operation_history_id_type>> acct_ath = get_account( db, obj );
   if( !acct_ath.valid() ) return;
   _history_by_account[acct_ath->first].erase( acct_ath->second );
}

} // end namespace detail

affiliate_stats_plugin::affiliate_stats_plugin()
   : my( new detail::affiliate_stats_plugin_impl(*this) ) {}

affiliate_stats_plugin::~affiliate_stats_plugin() {}

std::string affiliate_stats_plugin::plugin_name()const
{
   return "affiliate_stats";
}

void affiliate_stats_plugin::plugin_set_program_options(
   boost::program_options::options_description& cli,
   boost::program_options::options_description& cfg
   )
{
   cli.add_options()
         ;
   cfg.add(cli);
}

void affiliate_stats_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
   database().applied_block.connect( [this]( const signed_block& b){ my->update_affiliate_stats(b); } );

   my->_ar_index = database().add_index< primary_index< app_reward_index > >();
   my->_rr_index = database().add_index< primary_index< referral_reward_index > >();
   my->_fr_index = shared_ptr<detail::affiliate_reward_index>( new detail::affiliate_reward_index( database() ) );
   const_cast<primary_index<account_transaction_history_index>&>(database().get_index_type<primary_index<account_transaction_history_index>>()).add_observer( my->_fr_index );
}

void affiliate_stats_plugin::plugin_startup() {}

const std::set<graphene::chain::operation_history_id_type>& affiliate_stats_plugin::get_reward_history( account_id_type& affiliate )const
{
   return my->get_reward_history( affiliate );
}

} } // graphene::affiliate_stats
