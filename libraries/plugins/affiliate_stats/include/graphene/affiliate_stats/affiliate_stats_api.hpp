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
#include <graphene/chain/operation_history_object.hpp>

#include <graphene/affiliate_stats/affiliate_stats_objects.hpp>

using namespace graphene::chain;

namespace graphene { namespace app {
   class application;
} }

namespace graphene { namespace affiliate_stats {

namespace detail {
   class affiliate_stats_api_impl;
}

class referral_payment {
public:
   referral_payment();
   referral_payment( const operation_history_object& oho );
   operation_history_id_type id;
   uint32_t                  block_num;
   app_tag                   tag;
   asset                     payout;
};

class top_referred_account {
public:
   top_referred_account();
   top_referred_account( const referral_reward_object& rro );

   account_id_type referral;
   asset           total_payout;
};

class top_app {
public:
   top_app();
   top_app( const app_reward_object& rro );

   app_tag app;
   asset   total_payout;
};

class affiliate_stats_api
{
   public:
      affiliate_stats_api(graphene::app::application& app);

      std::vector<referral_payment> list_historic_referral_rewards( account_id_type affiliate, operation_history_id_type start, uint16_t limit = 100 )const;
      // get_pending_referral_reward() - not implemented because we have continuous payouts
      // get_previous_referral_reward() - not implemented because we have continuous payouts
      std::vector<top_referred_account> list_top_referred_accounts( asset_id_type asset, uint16_t limit = 100 )const;
      std::vector<top_app> list_top_rewards_per_app( asset_id_type asset, uint16_t limit = 100 )const;

      std::shared_ptr<detail::affiliate_stats_api_impl> my;
};

} } // graphene::affiliate_stats

FC_REFLECT(graphene::affiliate_stats::referral_payment, (id)(block_num)(tag)(payout) )
FC_REFLECT(graphene::affiliate_stats::top_referred_account, (referral)(total_payout) )
FC_REFLECT(graphene::affiliate_stats::top_app, (app)(total_payout) )

FC_API(graphene::affiliate_stats::affiliate_stats_api,
       (list_historic_referral_rewards)
       (list_top_referred_accounts)
       (list_top_rewards_per_app)
       )
