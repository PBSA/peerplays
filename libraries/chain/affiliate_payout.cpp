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
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/affiliate_payout.hpp>
#include <graphene/chain/database.hpp>

#include <fc_pp/uint128.hpp>

namespace graphene { namespace chain {

   share_type affiliate_payout_helper::payout( account_id_type player, share_type amount )
   {
      return payout( player(_db), amount );
   }

   share_type affiliate_payout_helper::payout( const account_object& player, share_type amount )
   {
      if( !player.affiliate_distributions.valid() )
         return 0;
      const auto& dist = player.affiliate_distributions->_dists.find( tag );
      if( dist == player.affiliate_distributions->_dists.end() || dist->second._dist.empty() )
         return 0;

      amount = amount.value / 5; // 20% fixed
      if( amount <= 0 )
         return 0;

      uint16_t remaining = GRAPHENE_100_PERCENT;
      share_type paid = 0;
      share_type to_pay = amount;
      for( const auto& entry : dist->second._dist )
      {
          const account_id_type affiliate = entry.first;
          const uint16_t share = entry.second;
          fc_pp::uint128_t payout = to_pay.value;
          if( share != remaining )
          {
             FC_ASSERT( share < remaining );
             payout *= share;
             payout /= remaining;
             //ilog("Paying ${p} of ${P} for ${s} of ${r}", ("p",payout.to_uint64())("P",to_pay.value)("s",share)("r",remaining) );
             remaining -= share;
          }
          FC_ASSERT( payout.to_uint64() <= to_pay );
          if( payout > 0 )
          {
             if ( accumulator.find(affiliate) == accumulator.end() )
                accumulator[affiliate] = payout.to_uint64();
             else
                accumulator[affiliate] += payout.to_uint64();
             to_pay -= payout.to_uint64();
             paid += payout.to_uint64();
          }
      }
      FC_ASSERT( to_pay == 0 );
      FC_ASSERT( paid == amount );

      _db.push_applied_operation( affiliate_referral_payout_operation( player.id, asset( amount, payout_asset ) ) );

      return paid;
   }

   void affiliate_payout_helper::commit()
   {
      for( const auto& entry : accumulator )
      {
          asset payout = asset( entry.second, payout_asset );
         _db.adjust_balance( entry.first, payout );
         _db.push_applied_operation( affiliate_payout_operation( entry.first, tag, payout ) );
      }
      accumulator.clear();
   }

} } // graphene::chain
