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

#pragma once

#include <fc/api.hpp>
#include <fc/variant_object.hpp>

#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/protocol/asset.hpp>
#include <graphene/chain/event_object.hpp>

using namespace graphene::chain;

namespace graphene { namespace app {
   class application;
} }

namespace graphene { namespace affiliate_stats {

namespace detail {
   class affiliate_stats_api_impl;
}

class affiliate_stats_api
{
   public:
      affiliate_stats_api(graphene::app::application& app);

      std::shared_ptr<detail::affiliate_stats_api_impl> my;
};

} } // graphene::affiliate_stats
/*
FC_REFLECT(graphene::bookie::order_bin, (amount_to_bet)(backer_multiplier))
FC_REFLECT(graphene::bookie::binned_order_book, (aggregated_back_bets)(aggregated_lay_bets))
FC_REFLECT(graphene::bookie::matched_bet_object, (id)(bettor_id)(betting_market_id)(amount_to_bet)(backer_multiplier)(back_or_lay)(end_of_delay)(amount_matched)(associated_operations))
*/
FC_API(graphene::affiliate_stats::affiliate_stats_api,
       )
