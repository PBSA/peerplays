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

#include <fc/optional.hpp>
#include <fc/variant_object.hpp>
#include <fc/smart_ref_impl.hpp>

#include <graphene/app/application.hpp>

#include <graphene/chain/block_database.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/betting_market_object.hpp>

#include <graphene/chain/witness_object.hpp>

#include <graphene/utilities/key_conversion.hpp>

#include <graphene/affiliate_stats/affiliate_stats_api.hpp>

#include <graphene/account_history/account_history_plugin.hpp>

namespace graphene { namespace affiliate_stats {

namespace detail {

class affiliate_stats_api_impl
{
   public:
      affiliate_stats_api_impl(graphene::app::application& _app);

      std::vector<top_referred_account> list_top_referred_accounts( asset_id_type asset, uint16_t limit )const
      {
         std::vector<top_referred_account> result;
         result.reserve( limit );
         auto& idx = app.chain_database()->get_index_type<referral_reward_index>().indices().get<by_asset>();
         auto itr = idx.lower_bound( boost::make_tuple( asset, share_type(GRAPHENE_MAX_SHARE_SUPPLY) ) );
         while( itr != idx.end() && itr->get_asset_id() == asset && limit-- > 0 )
             result.push_back( *itr++ );
         return result;
      }

      std::vector<top_app> list_top_rewards_per_app( asset_id_type asset, uint16_t limit )const
      {
         std::vector<top_app> result;
         result.reserve( limit );
         auto& idx = app.chain_database()->get_index_type<app_reward_index>().indices().get<by_asset>();
         auto itr = idx.lower_bound( boost::make_tuple( asset, share_type(GRAPHENE_MAX_SHARE_SUPPLY) ) );
         while( itr != idx.end() && itr->get_asset_id() == asset && limit-- > 0 )
             result.push_back( *itr++ );
         return result;
      }

      std::vector<referral_payment> list_historic_referral_rewards( account_id_type affiliate, operation_history_id_type start, uint16_t limit )const
      {
         shared_ptr<const affiliate_stats_plugin> plugin = app.get_plugin<const affiliate_stats_plugin>( "affiliate_stats" );

         std::vector<referral_payment> result;
         const auto& list = plugin->get_reward_history( affiliate );
         result.reserve( limit );
         auto inner = list.lower_bound( start );
         while( inner != list.end() && result.size() < limit )
            result.push_back( referral_payment( (*inner++)(*app.chain_database()) ) );
         return result;
      }

      graphene::app::application& app;
};

affiliate_stats_api_impl::affiliate_stats_api_impl(graphene::app::application& _app)
   : app(_app) {}

} // detail

top_referred_account::top_referred_account() {}

top_referred_account::top_referred_account( const referral_reward_object& rro )
   : referral( rro.referral ), total_payout( rro.total_payout ) {}

top_app::top_app() {}

top_app::top_app( const app_reward_object& aro )
   : app( aro.app ), total_payout( aro.total_payout ) {}


affiliate_stats_api::affiliate_stats_api(graphene::app::application& app)
   : my(std::make_shared<detail::affiliate_stats_api_impl>(app)) {}

std::vector<top_referred_account> affiliate_stats_api::list_top_referred_accounts( asset_id_type asset, uint16_t limit )const
{
   FC_ASSERT( limit <= 100 );
   return my->list_top_referred_accounts( asset, limit );
}

std::vector<top_app> affiliate_stats_api::list_top_rewards_per_app( asset_id_type asset, uint16_t limit )const
{
   FC_ASSERT( limit <= 100 );
   return my->list_top_rewards_per_app( asset, limit );
}

std::vector<referral_payment> affiliate_stats_api::list_historic_referral_rewards( account_id_type affiliate, operation_history_id_type start, uint16_t limit )const
{
   FC_ASSERT( limit <= 100 );
   return my->list_historic_referral_rewards( affiliate, start, limit );
}


referral_payment::referral_payment() {}

referral_payment::referral_payment( const operation_history_object& oho )
   : id(oho.id), block_num(oho.block_num), tag(oho.op.get<affiliate_payout_operation>().tag),
     payout(oho.op.get<affiliate_payout_operation>().payout) {}

} } // graphene::affiliate_stats


