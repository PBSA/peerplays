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

#include <graphene/accounts_list/accounts_list_plugin.hpp>

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

namespace graphene { namespace accounts_list {

namespace detail
{


class accounts_list_plugin_impl
{
   public:
      accounts_list_plugin_impl(accounts_list_plugin& _plugin)
         : _self( _plugin )
      { }
      virtual ~accounts_list_plugin_impl();


      /**
       */
      void list_accounts();

      graphene::chain::database& database()
      {
         return _self.database();
      }

      accounts_list_plugin& _self;
      vector<account_balance_object> _listed_balances;
};

accounts_list_plugin_impl::~accounts_list_plugin_impl()
{
   return;
}

void accounts_list_plugin_impl::list_accounts()
{
   graphene::chain::database& db = database();
   _listed_balances.clear();

   auto& balance_index = db.get_index_type<graphene::chain::account_balance_index>().indices().get<graphene::chain::by_asset_balance>();
   for (auto balance_iter = balance_index.begin();
             balance_iter != balance_index.end() &&
             balance_iter->asset_type == graphene::chain::asset_id_type() &&
             balance_iter->balance > 0; ++balance_iter)
   {
        //idump((balance_iter->owner(db) .name)(*balance_iter));
       _listed_balances.emplace_back(*balance_iter);
   }

}
} // end namespace detail

accounts_list_plugin::accounts_list_plugin() :
   my( new detail::accounts_list_plugin_impl(*this) )
{
}

accounts_list_plugin::~accounts_list_plugin()
{
}

std::string accounts_list_plugin::plugin_name()const
{
   return "accounts_list";
}

void accounts_list_plugin::plugin_set_program_options(
   boost::program_options::options_description& /*cli*/,
   boost::program_options::options_description& /*cfg*/
   )
{
//   cli.add_options()
//         ("list-account", boost::program_options::value<std::vector<std::string>>()->composing()->multitoken(), "Account ID to list (may specify multiple times)")
//         ;
//   cfg.add(cli);
}

void accounts_list_plugin::plugin_initialize(const boost::program_options::variables_map& /*options*/)
{
   //ilog("accounts list plugin:  plugin_initialize()");
   list_accounts();
}

void accounts_list_plugin::plugin_startup()
{
    //ilog("accounts list plugin:  plugin_startup()");
}

 vector<account_balance_object> accounts_list_plugin::list_accounts() const
{
     ilog("accounts list plugin:  list_accounts()");
     my->list_accounts();
     //idump((my->_listed_balances));
     return my->_listed_balances;
}

} }
