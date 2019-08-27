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
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {

   class ticket_purchase_evaluator : public evaluator<ticket_purchase_evaluator>
   {
      public:
         typedef ticket_purchase_operation operation_type;

         void_result do_evaluate( const ticket_purchase_operation& o );
         void_result do_apply( const ticket_purchase_operation& o );

         const asset_object* lottery;
         const asset_dynamic_data_object* asset_dynamic_data;
   };

   class lottery_reward_evaluator : public evaluator<lottery_reward_evaluator>
   {
      public:
         typedef lottery_reward_operation operation_type;

         void_result do_evaluate( const lottery_reward_operation& o );
         void_result do_apply( const lottery_reward_operation& o );

         const asset_object* lottery;
         const asset_dynamic_data_object* asset_dynamic_data;
   };

   class lottery_end_evaluator : public evaluator<lottery_end_evaluator>
   {
      public:
         typedef lottery_end_operation operation_type;

         void_result do_evaluate( const lottery_end_operation& o );
         void_result do_apply( const lottery_end_operation& o );

         const asset_object* lottery;
         const asset_dynamic_data_object* asset_dynamic_data;
   };
   
   class sweeps_vesting_claim_evaluator : public evaluator<sweeps_vesting_claim_evaluator>
   {
      public:
         typedef sweeps_vesting_claim_operation operation_type;

         void_result do_evaluate( const sweeps_vesting_claim_operation& o );
         void_result do_apply( const sweeps_vesting_claim_operation& o );

//         const asset_object* lottery;
//         const asset_dynamic_data_object* asset_dynamic_data;
   };

} } // graphene::chain
