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

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>

#include <fc/thread/future.hpp>

namespace graphene { namespace bookie {
using namespace chain;

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//
enum spaces {
   bookie_objects = 6
};
enum bookie_object_type
{
   persistent_event_object_type,
   BOOKIE_OBJECT_TYPE_COUNT ///< Sentry value which contains the number of different object types
};


namespace detail
{
    class bookie_plugin_impl;
}

class bookie_plugin : public graphene::app::plugin
{
   public:
      bookie_plugin();
      virtual ~bookie_plugin();

      std::string plugin_name()const override;
      virtual void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg) override;
      virtual void plugin_initialize(const boost::program_options::variables_map& options) override;
      virtual void plugin_startup() override;

      flat_set<account_id_type> tracked_accounts()const;
      asset get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id);
      void get_events_containing_sub_string(std::vector<event_object>& events, const std::string& sub_string, const std::string& language);

      friend class detail::bookie_plugin_impl;
      std::unique_ptr<detail::bookie_plugin_impl> my;
};

} } //graphene::bookie

