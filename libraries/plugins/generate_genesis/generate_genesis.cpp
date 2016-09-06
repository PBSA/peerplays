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
#include <graphene/generate_genesis/generate_genesis.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/genesis_state.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/time/time.hpp>

#include <graphene/utilities/key_conversion.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/thread/thread.hpp>

#include <iostream>

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
         ("snapshot-block-number", bpo::value<uint32_t>()->default_value(1), "Block number at which to snapshot balances")
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

void generate_genesis_plugin::generate_snapshot()
{
   ilog("generate genesis plugin: generating snapshot now");
   graphene::chain::genesis_state_type new_genesis_state;
   chain::database& d = database();
   auto& account_index = d.get_index_type<graphene::chain::account_index>();
   auto& account_by_id_index = account_index.indices().get<graphene::chain::by_id>();
   for (const graphene::chain::account_object& account_obj : account_by_id_index)
   {
      //new_genesis_state.initial_accounts.emplace_back(genesis_state_type::initial_account_type(account_obj.name, account
   }
}

void generate_genesis_plugin::plugin_shutdown()
{
}

