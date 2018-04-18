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

class affiliate_stats_plugin_impl
{
   public:
      affiliate_stats_plugin_impl(affiliate_stats_plugin& _plugin)
         : _self( _plugin )
      { }
      virtual ~affiliate_stats_plugin_impl();


      /** this method is called as a callback after a block is applied
       * and will process/index all operations that were applied in the block.
       */
      void update_affiliate_stats( const signed_block& b );

      graphene::chain::database& database()
      {
         return _self.database();
      }

      affiliate_stats_plugin& _self;
   private:
};

affiliate_stats_plugin_impl::~affiliate_stats_plugin_impl() {}

void affiliate_stats_plugin_impl::update_affiliate_stats( const signed_block& b )
{
   graphene::chain::database& db = database();
   vector<optional< operation_history_object > >& hist = db.get_applied_operations();
   for( optional< operation_history_object >& o_op : hist )
   {
      if( !o_op.valid() )
         continue;

      const operation_history_object& op = *o_op;

   }
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

   // FIXME
   // my->_oho_index = database().add_index< primary_index< simple_index< operation_history_object > > >();
   // database().add_index< primary_index< account_transaction_history_index > >();
}

void affiliate_stats_plugin::plugin_startup() {}

} } // graphene::affiliate_stats
