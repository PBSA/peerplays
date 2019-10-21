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
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/ext.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <fc/smart_ref_fwd.hpp>

#include <graphene/chain/hardfork.hpp>

namespace graphene { namespace chain { struct fee_schedule; } }

namespace graphene { namespace chain {
   struct parameter_extension
   {
      optional< bet_multiplier_type > min_bet_multiplier;
      optional< bet_multiplier_type > max_bet_multiplier;
      optional< uint16_t >            betting_rake_fee_percentage;
      optional< flat_map<bet_multiplier_type, bet_multiplier_type> > permitted_betting_odds_increments;
      optional< uint16_t >            live_betting_delay_time;
      optional < uint16_t >           son_count;
      optional< uint16_t >            sweeps_distribution_percentage;
      optional< asset_id_type >       sweeps_distribution_asset;
      optional< account_id_type >     sweeps_vesting_accumulator_account;
      optional < uint32_t >           son_vesting_amount;
      optional < uint32_t >           son_vesting_period;
      optional < uint32_t >           son_pay_daily_max;
   };

   struct chain_parameters
   {
      /** using a smart ref breaks the circular dependency created between operations and the fee schedule */
      smart_ref<fee_schedule> current_fees;                       ///< current schedule of fees
      uint8_t                 block_interval                      = GRAPHENE_DEFAULT_BLOCK_INTERVAL; ///< interval in seconds between blocks
      uint32_t                maintenance_interval                = GRAPHENE_DEFAULT_MAINTENANCE_INTERVAL; ///< interval in sections between blockchain maintenance events
      uint8_t                 maintenance_skip_slots              = GRAPHENE_DEFAULT_MAINTENANCE_SKIP_SLOTS; ///< number of block_intervals to skip at maintenance time
      uint32_t                committee_proposal_review_period    = GRAPHENE_DEFAULT_COMMITTEE_PROPOSAL_REVIEW_PERIOD_SEC; ///< minimum time in seconds that a proposed transaction requiring committee authority may not be signed, prior to expiration
      uint32_t                maximum_transaction_size            = GRAPHENE_DEFAULT_MAX_TRANSACTION_SIZE; ///< maximum allowable size in bytes for a transaction
      uint32_t                maximum_block_size                  = GRAPHENE_DEFAULT_MAX_BLOCK_SIZE; ///< maximum allowable size in bytes for a block
      uint32_t                maximum_time_until_expiration       = GRAPHENE_DEFAULT_MAX_TIME_UNTIL_EXPIRATION; ///< maximum lifetime in seconds for transactions to be valid, before expiring
      uint32_t                maximum_proposal_lifetime           = GRAPHENE_DEFAULT_MAX_PROPOSAL_LIFETIME_SEC; ///< maximum lifetime in seconds for proposed transactions to be kept, before expiring
      uint8_t                 maximum_asset_whitelist_authorities = GRAPHENE_DEFAULT_MAX_ASSET_WHITELIST_AUTHORITIES; ///< maximum number of accounts which an asset may list as authorities for its whitelist OR blacklist
      uint8_t                 maximum_asset_feed_publishers       = GRAPHENE_DEFAULT_MAX_ASSET_FEED_PUBLISHERS; ///< the maximum number of feed publishers for a given asset
      uint16_t                maximum_witness_count               = GRAPHENE_DEFAULT_MAX_WITNESSES; ///< maximum number of active witnesses
      uint16_t                maximum_committee_count             = GRAPHENE_DEFAULT_MAX_COMMITTEE; ///< maximum number of active committee_members
      uint16_t                maximum_son_count                   = GRAPHENE_DEFAULT_MAX_SONS; ///< maximum number of active SONS
      uint16_t                maximum_authority_membership        = GRAPHENE_DEFAULT_MAX_AUTHORITY_MEMBERSHIP; ///< largest number of keys/accounts an authority can have
      uint16_t                reserve_percent_of_fee              = GRAPHENE_DEFAULT_BURN_PERCENT_OF_FEE; ///< the percentage of the network's allocation of a fee that is taken out of circulation
      uint16_t                network_percent_of_fee              = GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE; ///< percent of transaction fees paid to network
      uint16_t                lifetime_referrer_percent_of_fee    = GRAPHENE_DEFAULT_LIFETIME_REFERRER_PERCENT_OF_FEE; ///< percent of transaction fees paid to network
      uint32_t                cashback_vesting_period_seconds     = GRAPHENE_DEFAULT_CASHBACK_VESTING_PERIOD_SEC; ///< time after cashback rewards are accrued before they become liquid
      share_type              cashback_vesting_threshold          = GRAPHENE_DEFAULT_CASHBACK_VESTING_THRESHOLD; ///< the maximum cashback that can be received without vesting
      bool                    count_non_member_votes              = true; ///< set to false to restrict voting privlegages to member accounts
      bool                    allow_non_member_whitelists         = false; ///< true if non-member accounts may set whitelists and blacklists; false otherwise
      share_type              witness_pay_per_block               = GRAPHENE_DEFAULT_WITNESS_PAY_PER_BLOCK; ///< CORE to be allocated to witnesses (per block)
      uint32_t                witness_pay_vesting_seconds         = GRAPHENE_DEFAULT_WITNESS_PAY_VESTING_SECONDS; ///< vesting_seconds parameter for witness VBO's
      share_type              worker_budget_per_day               = GRAPHENE_DEFAULT_WORKER_BUDGET_PER_DAY; ///< CORE to be allocated to workers (per day)
      uint16_t                max_predicate_opcode                = GRAPHENE_DEFAULT_MAX_ASSERT_OPCODE; ///< predicate_opcode must be less than this number
      share_type              fee_liquidation_threshold           = GRAPHENE_DEFAULT_FEE_LIQUIDATION_THRESHOLD; ///< value in CORE at which accumulated fees in blockchain-issued market assets should be liquidated
      uint16_t                accounts_per_fee_scale              = GRAPHENE_DEFAULT_ACCOUNTS_PER_FEE_SCALE; ///< number of accounts between fee scalings
      uint8_t                 account_fee_scale_bitshifts         = GRAPHENE_DEFAULT_ACCOUNT_FEE_SCALE_BITSHIFTS; ///< number of times to left bitshift account registration fee at each scaling
      uint8_t                 max_authority_depth                 = GRAPHENE_MAX_SIG_CHECK_DEPTH;
      uint8_t                 witness_schedule_algorithm          = GRAPHENE_WITNESS_SCHEDULED_ALGORITHM; ///< 0 shuffled, 1 scheduled
      /* rps tournament parameters constraints */
      uint32_t                min_round_delay                     = TOURNAMENT_MIN_ROUND_DELAY; ///< miniaml delay between games
      uint32_t                max_round_delay                     = TOURNAMENT_MAX_ROUND_DELAY; ///< maximal delay between games
      uint32_t                min_time_per_commit_move            = TOURNAMENT_MIN_TIME_PER_COMMIT_MOVE; ///< minimal time to commit the next move
      uint32_t                max_time_per_commit_move            = TOURNAMENT_MAN_TIME_PER_COMMIT_MOVE; ///< maximal time to commit the next move
      uint32_t                min_time_per_reveal_move            = TOURNAMENT_MIN_TIME_PER_REVEAL_MOVE; ///< minimal time to reveal move
      uint32_t                max_time_per_reveal_move            = TOURNAMENT_MAX_TIME_PER_REVEAL_MOVE; ///< maximal time to reveal move
      uint16_t                rake_fee_percentage                 = TOURNAMENT_DEFAULT_RAKE_FEE_PERCENTAGE; ///< part of prize paid into the dividend account for the core token holders
      uint32_t                maximum_registration_deadline       = TOURNAMENT_MAXIMAL_REGISTRATION_DEADLINE; ///< value registration deadline must be before
      uint16_t                maximum_players_in_tournament       = TOURNAMENT_MAX_PLAYERS_NUMBER; ///< maximal count of players in tournament
      uint16_t                maximum_tournament_whitelist_length = TOURNAMENT_MAX_WHITELIST_LENGTH; ///< maximal tournament whitelist length
      uint32_t                maximum_tournament_start_time_in_future = TOURNAMENT_MAX_START_TIME_IN_FUTURE;
      uint32_t                maximum_tournament_start_delay      = TOURNAMENT_MAX_START_DELAY;
      uint16_t                maximum_tournament_number_of_wins   = TOURNAMENT_MAX_NUMBER_OF_WINS;
      
      extension<parameter_extension> extensions;

      /** defined in fee_schedule.cpp */
      void validate()const;
      inline bet_multiplier_type min_bet_multiplier()const {
         return extensions.value.min_bet_multiplier.valid() ? *extensions.value.min_bet_multiplier : GRAPHENE_DEFAULT_MIN_BET_MULTIPLIER;
      }
      inline bet_multiplier_type max_bet_multiplier()const {
         return extensions.value.max_bet_multiplier.valid() ? *extensions.value.max_bet_multiplier : GRAPHENE_DEFAULT_MAX_BET_MULTIPLIER;
      }
      inline uint16_t betting_rake_fee_percentage()const {
         return extensions.value.betting_rake_fee_percentage.valid() ? *extensions.value.betting_rake_fee_percentage : GRAPHENE_DEFAULT_RAKE_FEE_PERCENTAGE;
      }
      inline const flat_map<bet_multiplier_type, bet_multiplier_type>& permitted_betting_odds_increments()const {
         static const flat_map<bet_multiplier_type, bet_multiplier_type> _default = GRAPHENE_DEFAULT_PERMITTED_BETTING_ODDS_INCREMENTS;
         return extensions.value.permitted_betting_odds_increments.valid() ? *extensions.value.permitted_betting_odds_increments : _default;
      }
      inline uint16_t live_betting_delay_time()const {
         return extensions.value.live_betting_delay_time.valid() ? *extensions.value.live_betting_delay_time : GRAPHENE_DEFAULT_LIVE_BETTING_DELAY_TIME;
      }
      inline uint16_t sweeps_distribution_percentage()const {
         return extensions.value.sweeps_distribution_percentage.valid() ? *extensions.value.sweeps_distribution_percentage : SWEEPS_DEFAULT_DISTRIBUTION_PERCENTAGE;
      }
      inline asset_id_type sweeps_distribution_asset()const {
         return extensions.value.sweeps_distribution_asset.valid() ? *extensions.value.sweeps_distribution_asset : SWEEPS_DEFAULT_DISTRIBUTION_ASSET;
      }
      inline account_id_type sweeps_vesting_accumulator_account()const {
         return extensions.value.sweeps_vesting_accumulator_account.valid() ? *extensions.value.sweeps_vesting_accumulator_account : SWEEPS_ACCUMULATOR_ACCOUNT;
      }
      inline uint16_t son_count()const {
         return extensions.value.son_count.valid() ? *extensions.value.son_count : MIN_SON_MEMBER_COUNT;
      }
      inline uint32_t son_vesting_amount()const {
         return extensions.value.son_vesting_amount.valid() ? *extensions.value.son_vesting_amount : SON_VESTING_AMOUNT; /// current period start date
      }
      inline uint32_t son_vesting_period()const {
         return extensions.value.son_vesting_period.valid() ? *extensions.value.son_vesting_period : SON_VESTING_PERIOD; /// current period start date
      }
      inline uint16_t son_pay_daily_max()const {
         return extensions.value.son_pay_daily_max.valid() ? *extensions.value.son_pay_daily_max : MIN_SON_PAY_DAILY_MAX;
      }
   };

} }  // graphene::chain

FC_REFLECT( graphene::chain::parameter_extension,
   (min_bet_multiplier)
   (max_bet_multiplier)
   (betting_rake_fee_percentage)
   (permitted_betting_odds_increments)
   (live_betting_delay_time)
   (son_count)
   (sweeps_distribution_percentage)
   (sweeps_distribution_asset)
   (sweeps_vesting_accumulator_account)
   (son_vesting_amount)
   (son_vesting_period)
   (son_pay_daily_max)
)

FC_REFLECT( graphene::chain::chain_parameters,
            (current_fees)
            (block_interval)
            (maintenance_interval)
            (maintenance_skip_slots)
            (committee_proposal_review_period)
            (maximum_transaction_size)
            (maximum_block_size)
            (maximum_time_until_expiration)
            (maximum_proposal_lifetime)
            (maximum_asset_whitelist_authorities)
            (maximum_asset_feed_publishers)
            (maximum_witness_count)
            (maximum_committee_count)
            (maximum_son_count)
            (maximum_authority_membership)
            (reserve_percent_of_fee)
            (network_percent_of_fee)
            (lifetime_referrer_percent_of_fee)
            (cashback_vesting_period_seconds)
            (cashback_vesting_threshold)
            (count_non_member_votes)
            (allow_non_member_whitelists)
            (witness_pay_per_block)
            (worker_budget_per_day)
            (max_predicate_opcode)
            (fee_liquidation_threshold)
            (accounts_per_fee_scale)
            (account_fee_scale_bitshifts)
            (max_authority_depth)
            (witness_schedule_algorithm)
            (min_round_delay)
            (max_round_delay)
            (min_time_per_commit_move)
            (max_time_per_commit_move)
            (min_time_per_reveal_move)
            (max_time_per_reveal_move)
            (rake_fee_percentage)
            (maximum_registration_deadline)
            (maximum_players_in_tournament)
            (maximum_tournament_whitelist_length)
            (maximum_tournament_start_time_in_future)
            (maximum_tournament_start_delay)
            (maximum_tournament_number_of_wins)
            (extensions)
          )
