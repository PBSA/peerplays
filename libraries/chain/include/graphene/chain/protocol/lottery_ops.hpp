/*
 * Copyright (c) 2017 Peerplays, Inc., and contributors.
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
#include <graphene/chain/protocol/base.hpp>

namespace graphene { namespace chain { 

   /**
    * @ingroup operations
    */
   struct ticket_purchase_operation : public base_operation
   {
      struct fee_parameters_type { 
         uint64_t fee = 0;
      };

      asset                   fee;
      // from what lottery is ticket
      asset_id_type           lottery;
      account_id_type         buyer;
      // count of tickets to buy
      uint64_t                tickets_to_buy;
      // amount that can spent
      asset                   amount;
      
      extensions_type         extensions;

      account_id_type fee_payer()const { return buyer; }
      void            validate()const;
      share_type      calculate_fee( const fee_parameters_type& k )const;
   };

} } // graphene::chain


FC_REFLECT( graphene::chain::ticket_purchase_operation,
            (fee)
            (lottery)
            (buyer)
            (tickets_to_buy)
            (amount)
            (extensions)
          )
FC_REFLECT( graphene::chain::ticket_purchase_operation::fee_parameters_type, (fee) )