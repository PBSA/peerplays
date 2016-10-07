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
#include <graphene/generate_genesis/generate_genesis_plugin.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/genesis_state.hpp>
#include <graphene/chain/witness_object.hpp>

#include <graphene/utilities/key_conversion.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/thread/thread.hpp>

#include <graphene/chain/market_object.hpp>

#include <iostream>
#include <fstream>

using namespace graphene::generate_genesis_plugin;
using std::string;
using std::vector;

namespace bpo = boost::program_options;

void generate_genesis_plugin::plugin_set_program_options(
   boost::program_options::options_description& command_line_options,
   boost::program_options::options_description& config_file_options)
{
   command_line_options.add_options()
         ("output-genesis-file,o", bpo::value<std::string>()->default_value("genesis.json"), "Genesis file to create")
         ("snapshot-block-number", bpo::value<uint32_t>()->default_value(0), "Block number at which to snapshot balances")
         ;
   config_file_options.add(command_line_options);
}

std::string generate_genesis_plugin::plugin_name()const
{
   return "generate_genesis";
}

void generate_genesis_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{ try {
   ilog("generate genesis plugin:  plugin_initialize() begin");
   _options = &options;

   _genesis_filename = options["output-genesis-file"].as<std::string>();
   _block_to_snapshot = options["snapshot-block-number"].as<uint32_t>();
   database().applied_block.connect([this](const graphene::chain::signed_block& b){ block_applied(b); });
   ilog("generate genesis plugin:  plugin_initialize() end");
} FC_LOG_AND_RETHROW() }

void generate_genesis_plugin::plugin_startup()
{ try {
   ilog("generate genesis plugin:  plugin_startup() begin");
   chain::database& d = database();
   if (d.head_block_num() == _block_to_snapshot)
   {
      ilog("generate genesis plugin: already at snapshot block");
      generate_snapshot();
   }
   else if (d.head_block_num() > _block_to_snapshot)
      elog("generate genesis plugin: already passed snapshot block, you must reindex to return to the snapshot state");
   else
      elog("generate genesis plugin: waiting for block ${snapshot_block} to generate snapshot, current head is ${head}",
           ("snapshot_block", _block_to_snapshot)("head", d.head_block_num()));

   ilog("generate genesis plugin:  plugin_startup() end");
} FC_CAPTURE_AND_RETHROW() }

void generate_genesis_plugin::block_applied(const graphene::chain::signed_block& b)
{
   if (b.block_num() == _block_to_snapshot)
   {
      ilog("generate genesis plugin: snapshot block has arrived");
      generate_snapshot();
   }
}

std::string modify_account_name(const std::string& name)
{
   return std::string("bts-") + name;
}

bool is_special_account(const graphene::chain::account_id_type& account_id)
{
   return account_id.instance < 100;
}

bool is_exchange(const std::string& account_name)
{
   return account_name == "poloniexcoldstorage" ||
          account_name == "btc38-public-for-bts-cold" ||
          account_name == "poloniexwallet" ||
          account_name == "btercom" || 
          account_name == "yunbi-cold-wallet" ||
          account_name == "btc38-btsx-octo-72722" || 
          account_name == "bittrex-deposit" ||
          account_name == "btc38btsxwithdrawal";
}

class my_account_balance_object : public graphene::chain::account_balance_object
{
public:
   graphene::chain::share_type        initial_balance;
   graphene::chain::share_type        orders;
   graphene::chain::share_type        collaterals;
   graphene::chain::share_type        sharedrop;
};

void generate_genesis_plugin::generate_snapshot()
{
   ilog("generate genesis plugin: generating snapshot now");
   graphene::chain::genesis_state_type new_genesis_state;
   chain::database& d = database();

   // we'll distribute 5% of 1,000,000 tokens, so:
   graphene::chain::share_type total_amount_to_distribute = 50000 * GRAPHENE_BLOCKCHAIN_PRECISION;

   // we need collection of mutable objects
   std::vector<my_account_balance_object> db_balances;
   // copy const objects to our collection
   auto& balance_index = d.get_index_type<graphene::chain::account_balance_index>().indices().get<graphene::chain::by_asset_balance>();
   for (auto balance_iter = balance_index.begin(); balance_iter != balance_index.end() && balance_iter->asset_type == graphene::chain::asset_id_type(); ++balance_iter)
      if (!is_special_account(balance_iter->owner) && !is_exchange(balance_iter->owner(d).name))
         {
             // todo : can static cast be dangerous : consider using CRTP idiom
             db_balances.emplace_back(static_cast<const my_account_balance_object&>(*balance_iter));
         }

   // walk through the balances; this index has the largest BTS balances first
   // first, calculate orders and collaterals
   // second, update balance
   graphene::chain::share_type  orders;
   graphene::chain::share_type  collaterals;
   graphene::chain::share_type total_bts_balance;
   std::ofstream logfile;

   bool sort = false;
   for (auto balance_iter = db_balances.begin(); balance_iter != db_balances.end(); ++balance_iter)
      {
         orders = 0;
         collaterals = 0;
         // BTS tied up in market orders
         auto order_range = d.get_index_type<graphene::chain::limit_order_index>().indices().get<graphene::chain::by_account>().equal_range(balance_iter->owner);
         std::for_each(order_range.first, order_range.second,
                       [&orders] (const graphene::chain::limit_order_object& order) {
                          if (order.amount_for_sale().asset_id == graphene::chain::asset_id_type())
                            orders += order.amount_for_sale().amount;
                       });
         // BTS tied up in collateral for SmartCoins
         auto collateral_range = d.get_index_type<graphene::chain::call_order_index>().indices().get<graphene::chain::by_account>().equal_range(balance_iter->owner);

         std::for_each(collateral_range.first, collateral_range.second,
                       [&collaterals] (const graphene::chain::call_order_object& order) {
                          collaterals += order.collateral;
                       });

         balance_iter->initial_balance = balance_iter->balance;
         balance_iter->orders = orders;
         balance_iter->collaterals = collaterals;
         balance_iter->balance += orders + collaterals;
         sort = sort || orders.value > 0 || collaterals.value > 0;
         total_bts_balance += balance_iter->balance;
       }

   if (sort)
      {
           ilog("generate genesis plugin: sorting");
           std::sort(db_balances.begin(), db_balances.end(),
           [](const my_account_balance_object & a, const my_account_balance_object & b) -> bool
              {
                return a.balance.value > b.balance.value;
              });
      }

   graphene::chain::share_type total_shares_dropped;
   // Now, we assume we're distributing balances to all BTS holders proportionally, figure
   // the smallest balance we can distribute and still assign the user a satoshi of the share drop
   graphene::chain::share_type effective_total_bts_balance;
   auto balance_iter = db_balances.begin();
   for (balance_iter = db_balances.begin(); balance_iter != db_balances.end(); ++balance_iter)
      {
         fc::uint128 share_drop_amount = total_amount_to_distribute.value;
         share_drop_amount *= balance_iter->balance.value;
         share_drop_amount /= total_bts_balance.value;
         if (!share_drop_amount.to_uint64())
            break; // balances are decreasing, so every balance after will also round to zero
         total_shares_dropped += share_drop_amount.to_uint64();
         effective_total_bts_balance += balance_iter->balance;
      }

   // cosmetic, otherwise mess in tail of log
   for (auto iter = balance_iter; iter != db_balances.end(); ++iter)
      {
        iter->sharedrop = 0;
      }

   // our iterator is just after the smallest balance we will process, 
   // walk it backwards towards the larger balances, distributing the sharedrop as we go
   graphene::chain::share_type remaining_amount_to_distribute = total_amount_to_distribute;
   graphene::chain::share_type bts_balance_remaining = effective_total_bts_balance;
   std::map<graphene::chain::account_id_type, graphene::chain::share_type> sharedrop_balances;

   do {
         --balance_iter;
         fc::uint128 share_drop_amount = remaining_amount_to_distribute.value;
         share_drop_amount *= balance_iter->balance.value;
         share_drop_amount /= bts_balance_remaining.value;
         graphene::chain::share_type amount_distributed =  share_drop_amount.to_uint64();
         sharedrop_balances[balance_iter->owner] = amount_distributed;
         balance_iter->sharedrop = amount_distributed;

         remaining_amount_to_distribute -= amount_distributed;
         bts_balance_remaining -= balance_iter->balance.value;
   } while (balance_iter != db_balances.begin());
   assert(remaining_amount_to_distribute == 0);

   logfile.open("log.csv");
   assert(logfile.is_open());
   logfile << "name,balance+orders+collaterals,balance,orders,collaterals,sharedrop\n";
   char del = ',';
   char nl = '\n';
   for( auto& o : db_balances)
     {
       logfile << o.owner(d).name << del << o.balance.value << del << o.initial_balance.value << del << o.orders.value << del << o.collaterals.value << del << o.sharedrop.value << nl;
     }
   logfile.close();

   //auto& account_index = d.get_index_type<graphene::chain::account_index>();
   //auto& account_by_id_index = account_index.indices().get<graphene::chain::by_id>();
   // inefficient way of crawling the graph, but we only do it once
   std::set<graphene::chain::account_id_type> already_generated;
   for (;;)
   {
      unsigned accounts_generated_this_round = 0;
      for (const auto& sharedrop_value : sharedrop_balances)
      {
         const graphene::chain::account_id_type& account_id = sharedrop_value.first;
         const graphene::chain::share_type& sharedrop_amount = sharedrop_value.second;
         const graphene::chain::account_object& account_obj = account_id(d);
         if (already_generated.find(account_id) == already_generated.end())
         {
            graphene::chain::genesis_state_type::initial_bts_account_type::initial_authority owner;
            owner.weight_threshold = account_obj.owner.weight_threshold;
            owner.key_auths = account_obj.owner.key_auths;
            for (const auto& value : account_obj.owner.account_auths)
            {
               owner.account_auths.insert(std::make_pair(modify_account_name(value.first(d).name), value.second));
               sharedrop_balances[value.first] += 0; // make sure the account is generated, even if it has a zero balance
            }
            owner.key_auths = account_obj.owner.key_auths;
            owner.address_auths = account_obj.owner.address_auths;
            
            graphene::chain::genesis_state_type::initial_bts_account_type::initial_authority active;
            active.weight_threshold = account_obj.active.weight_threshold;
            active.key_auths = account_obj.active.key_auths;
            for (const auto& value : account_obj.active.account_auths)
            {
               active.account_auths.insert(std::make_pair(modify_account_name(value.first(d).name), value.second));
               sharedrop_balances[value.first] += 0; // make sure the account is generated, even if it has a zero balance
            }
            active.key_auths = account_obj.active.key_auths;
            active.address_auths = account_obj.active.address_auths;

            new_genesis_state.initial_bts_accounts.emplace_back(
               graphene::chain::genesis_state_type::initial_bts_account_type(modify_account_name(account_obj.name),
                                                                             owner, active, 
                                                                             sharedrop_amount));
            already_generated.insert(account_id);
            ++accounts_generated_this_round;
         }
      }
      if (accounts_generated_this_round == 0)
         break;
   }
   fc::json::save_to_file(new_genesis_state, _genesis_filename);
   ilog("New genesis state written to file ${filename}", ("filename", _genesis_filename));
}

void generate_genesis_plugin::plugin_shutdown()
{
}

