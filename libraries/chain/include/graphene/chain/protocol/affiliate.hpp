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
#include <graphene/chain/protocol/account.hpp>
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/memo.hpp>

namespace graphene { namespace chain {

   /**
    * Virtual op generated when an affiliate receives payout.
    */

   struct affiliate_payout_operation : public base_operation
   {
      affiliate_payout_operation(){}
      affiliate_payout_operation( account_id_type a, app_tag t, const asset& amount )
      : affiliate(a), tag(t), payout(amount) {}

      struct fee_parameters_type { };
      asset           fee;

      // Account of the receiving affiliate
      account_id_type affiliate;
      // App-tag for which the payout was generated
      app_tag         tag;
      // Payout amount
      asset           payout;

      account_id_type fee_payer()const { return affiliate; }
      void            validate()const {
         FC_ASSERT( false, "Virtual operation" );
      }

      share_type calculate_fee(const fee_parameters_type& params)const
      { return 0; }
   };

   /**
    * Virtual op generated when a player generates an affiliate payout
    */
   struct affiliate_referral_payout_operation : public base_operation
   {
      affiliate_referral_payout_operation(){}
      affiliate_referral_payout_operation( account_id_type p, const asset& amount )
      : player(p), payout(amount) {}

      struct fee_parameters_type { };
      asset           fee;

      // Account of the winning player
      account_id_type player;
      // Payout amount
      asset           payout;

      account_id_type fee_payer()const { return player; }
      void            validate()const {
         FC_ASSERT( false, "virtual operation" );
      }

      share_type calculate_fee(const fee_parameters_type& params)const
      { return 0; }
   };

} } // graphene::chain

FC_REFLECT( graphene::chain::affiliate_payout_operation::fee_parameters_type, )
FC_REFLECT( graphene::chain::affiliate_referral_payout_operation::fee_parameters_type, )

FC_REFLECT( graphene::chain::affiliate_payout_operation, (fee)(affiliate)(tag)(payout) )
FC_REFLECT( graphene::chain::affiliate_referral_payout_operation, (fee)(player)(payout) )
