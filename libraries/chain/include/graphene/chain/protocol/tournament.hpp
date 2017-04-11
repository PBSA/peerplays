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

#include <cassert>
#include <cstdint>
#include <string>

#include <fc/container/flat.hpp>
#include <fc/reflect/reflect.hpp>
#include <graphene/chain/protocol/asset.hpp>
#include <graphene/chain/protocol/rock_paper_scissors.hpp>
#include <graphene/chain/protocol/base.hpp>

namespace graphene { namespace chain {

    enum class payout_type
    {
        prize_award,
        buyin_refund,
        rake_fee
    };

   typedef fc::static_variant<rock_paper_scissors_game_options> game_specific_options;

   /**
    * @brief Options specified when creating a new tournament
    */
   struct tournament_options
   {
      /// If there aren't enough players registered for the tournament before this time,
      /// the tournament is canceled
      fc::time_point_sec registration_deadline;

      /// Number of players in the tournament. Must be greater than 1. Currently max is 255, should it be committe-settable?
      uint32_t number_of_players;

      /// Each player must pay this much to join the tournament.  This can be 
      /// in any asset supported by the blockchain.  If the tournament is canceled,
      /// the buy-in will be returned.
      asset buy_in;

      /// A list of all accounts allowed to register for this tournament.  If empty,
      /// anyone can register for the tournament
      flat_set<account_id_type> whitelist;

      /// If specified, this is the time the tourament will start (must not be before the registration 
      /// deadline). If this is not specified, the creator must specify `start_delay` instead.
      optional<fc::time_point_sec> start_time;

      /// If specified, this is the number of seconds after the final player registers before the 
      /// tournament begins.  If this is not specified, the creator must specify an absolute `start_time`
      optional<uint32_t> start_delay;

      /// The delay, in seconds, between the end of the last game in one round of the tournament and the
      /// start of all the games in the next round
      uint32_t round_delay;

      /// The winner of a round in the tournament is the first to reach this number of wins
      uint32_t number_of_wins;

      /// Metadata about this tournament.  This can be empty or it can contain any keys the creator desires.
      /// The GUI will standardize on displaying a few keys, likely:
      ///  "name"
      ///  "description"
      ///  "url"
      fc::variant_object meta;

      /// Parameters that are specific to the type_of_game in this tournament
      /// The type stored in this static_variant field determines what type of game is being
      /// played, so each different supported game must have a unique game_options data type
      game_specific_options game_options;

      void validate() const;
   };

   struct tournament_create_operation : public base_operation
   {
      struct fee_parameters_type { 
         share_type fee = GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = 10;
      };

      asset fee;

      /// The account that created the tournament
      account_id_type creator;

      /// Options for the tournament
      tournament_options options;

      extensions_type extensions;

      account_id_type fee_payer()const { return creator; }
      share_type calculate_fee(const fee_parameters_type& k)const;
      void            validate()const;
   };

   struct tournament_join_operation : public base_operation
   {
      struct fee_parameters_type { 
         share_type fee = GRAPHENE_BLOCKCHAIN_PRECISION;
      };

      asset fee;

      /// The account that is paying the buy-in for the tournament, if the tournament is
      /// canceled, will be refunded the buy-in.
      account_id_type payer_account_id;

      /// The account that will play in the tournament, will receive any winnings.
      account_id_type player_account_id;

      /// The tournament `player_account_id` is joining
      tournament_id_type tournament_id;

      /// The buy-in paid by the `payer_account_id`
      asset buy_in;

      extensions_type extensions;
      account_id_type fee_payer()const { return payer_account_id; }
      share_type calculate_fee(const fee_parameters_type& k)const;
      void            validate()const;
   };

   struct tournament_leave_operation : public base_operation
   {
      struct fee_parameters_type {
         share_type fee = GRAPHENE_BLOCKCHAIN_PRECISION;
      };

      asset fee;

      /// The account that is unregistering the player from tournament (must be payer or player)
      account_id_type canceling_account_id;

      /// The account that would play in the tournament, would receive any winnings.
      account_id_type player_account_id;

      /// The tournament `player_account_id` is leaving
      tournament_id_type tournament_id;

      extensions_type extensions;
      account_id_type fee_payer()const { return canceling_account_id; }
      share_type calculate_fee(const fee_parameters_type& k)const;
      void            validate()const;
   };


   typedef fc::static_variant<rock_paper_scissors_throw_commit, rock_paper_scissors_throw_reveal> game_specific_moves;

   struct game_move_operation : public base_operation
   {
      struct fee_parameters_type { 
         share_type fee = GRAPHENE_BLOCKCHAIN_PRECISION;
      };
      asset fee;

      /// the id of the game 
      game_id_type game_id;

      /// The account of the player making this move
      account_id_type player_account_id;

      /// the move itself
      game_specific_moves move;

      extensions_type extensions;

      account_id_type fee_payer()const { return player_account_id; }
      share_type calculate_fee(const fee_parameters_type& k)const;
      void            validate()const;
   };

   struct tournament_payout_operation : public base_operation
   {
      struct fee_parameters_type {};

      asset fee;

      /// The account received payout
      account_id_type payout_account_id;

      /// The tournament generated payout
      tournament_id_type tournament_id;

      /// The payout amount
      asset payout_amount;

      payout_type type;

      extensions_type extensions;

      account_id_type fee_payer()const { return payout_account_id; }
      share_type calculate_fee(const fee_parameters_type&)const { return 0; }
      void            validate()const {}
   };

} }

FC_REFLECT_ENUM(graphene::chain::payout_type,
                (prize_award)
                (buyin_refund)
                (rake_fee)
                )

FC_REFLECT_TYPENAME( graphene::chain::game_specific_options )
FC_REFLECT_TYPENAME( graphene::chain::game_specific_moves )
FC_REFLECT( graphene::chain::tournament_options, 
            (registration_deadline)
            (number_of_players)
            (buy_in)
            (whitelist)
            (start_time)
            (start_delay)
            (round_delay)
            (number_of_wins)
            (meta)
            (game_options))
FC_REFLECT( graphene::chain::tournament_create_operation,
            (fee)
            (creator)
            (options)
            (extensions))
FC_REFLECT( graphene::chain::tournament_join_operation,
            (fee)
            (payer_account_id)
            (player_account_id)
            (tournament_id)
            (buy_in)
            (extensions))
FC_REFLECT( graphene::chain::tournament_leave_operation,
            (fee)
            (canceling_account_id)
            (player_account_id)
            (tournament_id)
            (extensions))
FC_REFLECT( graphene::chain::game_move_operation,
            (fee)
            (game_id)
            (player_account_id)
            (move)
            (extensions))
FC_REFLECT( graphene::chain::tournament_payout_operation,
            (fee)
            (payout_account_id)
            (tournament_id)
            (payout_amount)
            (type)
            (extensions))

FC_REFLECT( graphene::chain::tournament_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::tournament_join_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::tournament_leave_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::game_move_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::tournament_payout_operation::fee_parameters_type,  )

