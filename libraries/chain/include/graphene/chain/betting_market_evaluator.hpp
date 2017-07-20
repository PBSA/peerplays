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

#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {

   class betting_market_rules_create_evaluator : public evaluator<betting_market_rules_create_evaluator>
   {
      public:
         typedef betting_market_rules_create_operation operation_type;

         void_result do_evaluate( const betting_market_rules_create_operation& o );
         object_id_type do_apply( const betting_market_rules_create_operation& o );
   };

   class betting_market_rules_update_evaluator : public evaluator<betting_market_rules_update_evaluator>
   {
      public:
         typedef betting_market_rules_update_operation operation_type;

         void_result do_evaluate( const betting_market_rules_update_operation& o );
         void_result do_apply( const betting_market_rules_update_operation& o );
   };

   class betting_market_group_create_evaluator : public evaluator<betting_market_group_create_evaluator>
   {
      public:
         typedef betting_market_group_create_operation operation_type;

         void_result do_evaluate( const betting_market_group_create_operation& o );
         object_id_type do_apply( const betting_market_group_create_operation& o );
      private:
         event_id_type event_id;
         betting_market_rules_id_type rules_id;
   };

   class betting_market_create_evaluator : public evaluator<betting_market_create_evaluator>
   {
      public:
         typedef betting_market_create_operation operation_type;

         void_result do_evaluate( const betting_market_create_operation& o );
         object_id_type do_apply( const betting_market_create_operation& o );
      private:
         betting_market_group_id_type group_id;
   };

   class bet_place_evaluator : public evaluator<bet_place_evaluator>
   {
      public:
         typedef bet_place_operation operation_type;

         void_result do_evaluate( const bet_place_operation& o );
         object_id_type do_apply( const bet_place_operation& o );
      private:
         const betting_market_group_object* _betting_market_group;
         const betting_market_object* _betting_market;
         const asset_object* _asset;
         share_type _stake_plus_fees;
   };

   class bet_cancel_evaluator : public evaluator<bet_cancel_evaluator>
   {
      public:
         typedef bet_cancel_operation operation_type;

         void_result do_evaluate( const bet_cancel_operation& o );
         void_result do_apply( const bet_cancel_operation& o );
      private:
         const bet_object* _bet_to_cancel;
   };

   class betting_market_group_resolve_evaluator : public evaluator<betting_market_group_resolve_evaluator>
   {
      public:
         typedef betting_market_group_resolve_operation operation_type;

         void_result do_evaluate( const betting_market_group_resolve_operation& o );
         void_result do_apply( const betting_market_group_resolve_operation& o );
      private:
         const betting_market_group_object* _betting_market_group;
   };

   class betting_market_group_freeze_evaluator : public evaluator<betting_market_group_freeze_evaluator>
   {
      public:
         typedef betting_market_group_freeze_operation operation_type;

         void_result do_evaluate( const betting_market_group_freeze_operation& o );
         void_result do_apply( const betting_market_group_freeze_operation& o );
      private:
         const betting_market_group_object* _betting_market_group;
   };


   class betting_market_group_cancel_unmatched_bets_evaluator : public evaluator<betting_market_group_cancel_unmatched_bets_evaluator>
   {
      public:
         typedef betting_market_group_cancel_unmatched_bets_operation operation_type;

         void_result do_evaluate( const betting_market_group_cancel_unmatched_bets_operation& o );
         void_result do_apply( const betting_market_group_cancel_unmatched_bets_operation& o );
      private:
         const betting_market_group_object* _betting_market_group;
   };


} } // graphene::chain
