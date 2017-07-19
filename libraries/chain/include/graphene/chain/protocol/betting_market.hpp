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

#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/protocol/base.hpp>

namespace graphene { namespace chain {

struct betting_market_rules_create_operation : public base_operation
{
   struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
   asset             fee;

   /**
    * A short name for the rules, like "Premier League Rules 1.0", probably not 
    * displayed to the user
    */
   internationalized_string_type name;

   /**
    * The full text of the rules to be displayed to the user.  As yet, there is
    * no standard format (html, markdown, etc)
    */
   internationalized_string_type description;

   extensions_type   extensions;

   account_id_type fee_payer()const { return GRAPHENE_WITNESS_ACCOUNT; }
   void            validate()const;
};

struct betting_market_group_create_operation : public base_operation
{
   struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
   asset             fee;

   /**
    * A description of the betting market, like "Moneyline", "Over/Under 180",
    * used for display
    */
   internationalized_string_type description;

   /**
    * This can be a event_id_type, or a
    * relative object id that resolves to a event_id_type
    */
   object_id_type event_id;

   /**
    * This can be a betting_market_rules_id_type, or a
    * relative object id that resolves to a betting_market_rules_id_type
    */
   object_id_type rules_id;

   /**
    * The asset used to place bets for all betting markets in this group
    */
   asset_id_type asset_id;

   extensions_type   extensions;

   account_id_type fee_payer()const { return GRAPHENE_WITNESS_ACCOUNT; }
   void            validate()const;
};

struct betting_market_create_operation : public base_operation
{
   struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
   asset             fee;

   /**
    * This can be a betting_market_group_id_type, or a
    * relative object id that resolves to a betting_market_group_id_type
    */
   object_id_type group_id;

   internationalized_string_type payout_condition;

   extensions_type   extensions;

   account_id_type fee_payer()const { return GRAPHENE_WITNESS_ACCOUNT; }
   void            validate()const;
};

enum class betting_market_resolution_type {
   win,
   not_win,
   cancel,
   BETTING_MARKET_RESOLUTION_COUNT
};

struct betting_market_group_resolve_operation : public base_operation
{
   struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
   asset             fee;

   betting_market_group_id_type betting_market_group_id;

   std::map<betting_market_id_type, betting_market_resolution_type> resolutions;

   extensions_type   extensions;

   account_id_type fee_payer()const { return GRAPHENE_WITNESS_ACCOUNT; }
   void            validate()const;
};

struct betting_market_group_resolved_operation : public base_operation
{
   struct fee_parameters_type {};

   account_id_type bettor_id;
   betting_market_group_id_type betting_market_group_id;
   std::map<betting_market_id_type, betting_market_resolution_type> resolutions;

   share_type winnings; // always the asset type of the betting market group
   share_type fees_paid; // always the asset type of the betting market group

   asset             fee; // unused in a virtual operation

   betting_market_group_resolved_operation() {}
   betting_market_group_resolved_operation(account_id_type bettor_id,
                                           betting_market_group_id_type betting_market_group_id,
                                           const std::map<betting_market_id_type, betting_market_resolution_type>& resolutions,
                                           share_type winnings,
                                           share_type fees_paid) :
      bettor_id(bettor_id),
      betting_market_group_id(betting_market_group_id),
      resolutions(resolutions),
      winnings(winnings),
      fees_paid(fees_paid)
   {
   }

   account_id_type fee_payer()const { return bettor_id; }
   void            validate()const { FC_ASSERT(false, "virtual operation"); }
   /// This is a virtual operation; there is no fee
   share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
};

struct betting_market_group_freeze_operation : public base_operation
{
   struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
   asset             fee;

   betting_market_group_id_type betting_market_group_id;
   bool freeze; // the new state of the betting market

   extensions_type   extensions;

   account_id_type fee_payer()const { return GRAPHENE_WITNESS_ACCOUNT; }
   void            validate()const;
};

struct betting_market_group_cancel_unmatched_bets_operation : public base_operation
{
   struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
   asset             fee;

   betting_market_group_id_type betting_market_group_id;

   extensions_type   extensions;

   account_id_type fee_payer()const { return GRAPHENE_WITNESS_ACCOUNT; }
   void            validate()const;
};

enum class bet_type { back, lay };

struct bet_place_operation : public base_operation
{
   struct fee_parameters_type 
   { 
      uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;     // fixed fee charged upon placing the bet
      uint16_t percentage_fee = 2 * GRAPHENE_1_PERCENT; // charged when bet is matched
   };
   asset             fee;

   account_id_type bettor_id;
   
   betting_market_id_type betting_market_id;

   /// the bettor's stake
   asset amount_to_bet;

   // decimal odds as seen by the backer, even if this is a lay bet.
   // this is a fixed-precision number scaled by GRAPHENE_BETTING_ODDS_PRECISION.
   //
   // For example, an even 1/1 bet would be decimal odds 2.0, so backer_multiplier 
   // would be 2 * GRAPHENE_BETTING_ODDS_PRECISION.
   bet_multiplier_type backer_multiplier;

   // the amount the blockchain reserves to pay the percentage fee on matched bets.
   // when this bet is (partially) matched, the blockchain will take (part of) the fee.  If this bet is canceled
   // the remaining amount will be returned to the bettor (same asset type as the amount_to_bet)
   share_type amount_reserved_for_fees;

   bet_type back_or_lay;

   extensions_type   extensions;

   account_id_type fee_payer()const { return bettor_id; }
   void            validate()const;
};

/**
 * virtual op generated when a bet is matched
 */
struct bet_matched_operation : public base_operation 
{
   struct fee_parameters_type {};

   bet_matched_operation(){}
   bet_matched_operation(account_id_type bettor_id, bet_id_type bet_id, 
                         asset amount_bet, share_type fees_paid,
                         bet_multiplier_type backer_multiplier, 
                         share_type guaranteed_winnings_returned) :
      bettor_id(bettor_id),
      bet_id(bet_id),
      amount_bet(amount_bet),
      fees_paid(fees_paid),
      backer_multiplier(backer_multiplier),
      guaranteed_winnings_returned(guaranteed_winnings_returned)
   {}

   account_id_type     bettor_id;
   bet_id_type         bet_id;
   asset               amount_bet;
   share_type          fees_paid; // same asset type as amount_bet
   bet_multiplier_type backer_multiplier; // the actual odds received
   share_type          guaranteed_winnings_returned; // same asset type as amount_bet
   asset               fee; // unimportant for a virtual op

   account_id_type fee_payer()const { return bettor_id; }
   void            validate()const { FC_ASSERT(false, "virtual operation"); }

   /// This is a virtual operation; there is no fee
   share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
};

struct bet_cancel_operation : public base_operation
{
   struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
   asset             fee;

   /// the bettor who is cancelling the bet
   account_id_type bettor_id;
   /// the bet being canceled 
   bet_id_type bet_to_cancel;

   extensions_type   extensions;

   account_id_type fee_payer()const { return bettor_id; }
   void            validate()const;
};

/**
 * virtual op generated when a bet is canceled
 */
struct bet_canceled_operation : public base_operation 
{
   struct fee_parameters_type {};

   bet_canceled_operation(){}
   bet_canceled_operation(account_id_type bettor_id, bet_id_type bet_id, 
                          asset stake_returned, share_type unused_fees_returned) :
      bettor_id(bettor_id),
      bet_id(bet_id),
      stake_returned(stake_returned),
      unused_fees_returned(unused_fees_returned)
   {}

   account_id_type     bettor_id;
   bet_id_type         bet_id;
   asset               stake_returned;
   share_type          unused_fees_returned; // same asset type as stake_returned
   asset               fee; // unimportant for a virtual op

   account_id_type fee_payer()const { return bettor_id; }
   void            validate()const { FC_ASSERT(false, "virtual operation"); }

   /// This is a virtual operation; there is no fee
   share_type      calculate_fee(const fee_parameters_type& k)const { return 0; }
};


} }

FC_REFLECT( graphene::chain::betting_market_rules_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::betting_market_rules_create_operation, 
            (fee)(name)(description)(extensions) )

FC_REFLECT( graphene::chain::betting_market_group_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::betting_market_group_create_operation, 
            (fee)(description)(event_id)(rules_id)(asset_id)(extensions) )

FC_REFLECT( graphene::chain::betting_market_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::betting_market_create_operation, 
            (fee)(group_id)(payout_condition)(extensions) )

FC_REFLECT_ENUM( graphene::chain::betting_market_resolution_type, (win)(not_win)(cancel)(BETTING_MARKET_RESOLUTION_COUNT) )

FC_REFLECT( graphene::chain::betting_market_group_resolve_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::betting_market_group_resolve_operation,
            (fee)(betting_market_group_id)(resolutions)(extensions) )

FC_REFLECT( graphene::chain::betting_market_group_resolved_operation::fee_parameters_type, )
FC_REFLECT( graphene::chain::betting_market_group_resolved_operation,
            (bettor_id)(betting_market_group_id)(resolutions)(winnings)(fees_paid)(fee) )

FC_REFLECT( graphene::chain::betting_market_group_freeze_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::betting_market_group_freeze_operation,
            (fee)(betting_market_group_id)(freeze)(extensions) )

FC_REFLECT( graphene::chain::betting_market_group_cancel_unmatched_bets_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::betting_market_group_cancel_unmatched_bets_operation,
            (fee)(betting_market_group_id)(extensions) )

FC_REFLECT_ENUM( graphene::chain::bet_type, (back)(lay) )
FC_REFLECT( graphene::chain::bet_place_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::bet_place_operation, 
            (fee)(bettor_id)(betting_market_id)(amount_to_bet)(backer_multiplier)(amount_reserved_for_fees)(back_or_lay)(extensions) )

FC_REFLECT( graphene::chain::bet_matched_operation::fee_parameters_type, )
FC_REFLECT( graphene::chain::bet_matched_operation, (bettor_id)(bet_id)(amount_bet)(fees_paid)(backer_multiplier)(guaranteed_winnings_returned) )

FC_REFLECT( graphene::chain::bet_cancel_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::bet_cancel_operation, (bettor_id) (bet_to_cancel) (extensions) )

FC_REFLECT( graphene::chain::bet_canceled_operation::fee_parameters_type, )
FC_REFLECT( graphene::chain::bet_canceled_operation, (bettor_id)(bet_id)(stake_returned)(unused_fees_returned) )
