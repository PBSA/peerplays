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

#include <graphene/chain/protocol/authority.hpp>
#include <graphene/app/impacted.hpp>

namespace graphene { namespace app {

using namespace fc;
using namespace graphene::chain;

// TODO:  Review all of these, especially no-ops
struct get_impacted_account_visitor
{
   flat_set<account_id_type>& _impacted;
   get_impacted_account_visitor( flat_set<account_id_type>& impact ):_impacted(impact) {}
   typedef void result_type;

   void operator()( const transfer_operation& op )
   {
      _impacted.insert( op.to );
   }

   void operator()( const asset_claim_fees_operation& op ){}
   void operator()( const limit_order_create_operation& op ) {}
   void operator()( const limit_order_cancel_operation& op )
   {
      _impacted.insert( op.fee_paying_account );
   }
   void operator()( const call_order_update_operation& op ) {}
   void operator()( const fill_order_operation& op )
   {
      _impacted.insert( op.account_id );
   }

   void operator()( const account_create_operation& op )
   {
      _impacted.insert( op.registrar );
      _impacted.insert( op.referrer );
      add_authority_accounts( _impacted, op.owner );
      add_authority_accounts( _impacted, op.active );
   }

   void operator()( const account_update_operation& op )
   {
      _impacted.insert( op.account );
      if( op.owner )
         add_authority_accounts( _impacted, *(op.owner) );
      if( op.active )
         add_authority_accounts( _impacted, *(op.active) );
   }

   void operator()( const account_whitelist_operation& op )
   {
      _impacted.insert( op.account_to_list );
   }

   void operator()( const account_upgrade_operation& op ) {}
   void operator()( const account_transfer_operation& op )
   {
      _impacted.insert( op.new_owner );
   }

   void operator()( const asset_create_operation& op ) {}
   void operator()( const asset_update_operation& op )
   {
      if( op.new_issuer )
         _impacted.insert( *(op.new_issuer) );
   }

   void operator()( const asset_update_bitasset_operation& op ) {}
   void operator()( const asset_update_dividend_operation& op ) {}
   void operator()( const asset_dividend_distribution_operation& op ) 
   {
      _impacted.insert( op.account_id );
   }

   void operator()( const asset_update_feed_producers_operation& op ) {}

   void operator()( const asset_issue_operation& op )
   {
      _impacted.insert( op.issue_to_account );
   }

   void operator()( const asset_reserve_operation& op ) {}
   void operator()( const asset_fund_fee_pool_operation& op ) {}
   void operator()( const asset_settle_operation& op ) {}
   void operator()( const asset_global_settle_operation& op ) {}
   void operator()( const asset_publish_feed_operation& op ) {}
   void operator()( const witness_create_operation& op )
   {
      _impacted.insert( op.witness_account );
   }
   void operator()( const witness_update_operation& op )
   {
      _impacted.insert( op.witness_account );
   }

   void operator()( const proposal_create_operation& op )
   {
      vector<authority> other;
      for( const auto& proposed_op : op.proposed_ops )
         operation_get_required_authorities( proposed_op.op, _impacted, _impacted, other );
      for( auto& o : other )
         add_authority_accounts( _impacted, o );
   }

   void operator()( const proposal_update_operation& op ) {}
   void operator()( const proposal_delete_operation& op ) {}

   void operator()( const withdraw_permission_create_operation& op )
   {
      _impacted.insert( op.authorized_account );
   }

   void operator()( const withdraw_permission_update_operation& op )
   {
      _impacted.insert( op.authorized_account );
   }

   void operator()( const withdraw_permission_claim_operation& op )
   {
      _impacted.insert( op.withdraw_from_account );
   }

   void operator()( const withdraw_permission_delete_operation& op )
   {
      _impacted.insert( op.authorized_account );
   }

   void operator()( const committee_member_create_operation& op )
   {
      _impacted.insert( op.committee_member_account );
   }
   void operator()( const committee_member_update_operation& op )
   {
      _impacted.insert( op.committee_member_account );
   }
   void operator()( const committee_member_update_global_parameters_operation& op ) {}

   void operator()( const vesting_balance_create_operation& op )
   {
      _impacted.insert( op.owner );
   }

   void operator()( const vesting_balance_withdraw_operation& op ) {}
   void operator()( const worker_create_operation& op ) {}
   void operator()( const custom_operation& op ) {}
   void operator()( const assert_operation& op ) {}
   void operator()( const balance_claim_operation& op ) {}

   void operator()( const override_transfer_operation& op )
   {
      _impacted.insert( op.to );
      _impacted.insert( op.from );
      _impacted.insert( op.issuer );
   }

   void operator()( const transfer_to_blind_operation& op )
   {
      _impacted.insert( op.from );
      for( const auto& out : op.outputs )
         add_authority_accounts( _impacted, out.owner );
   }

   void operator()( const blind_transfer_operation& op )
   {
      for( const auto& in : op.inputs )
         add_authority_accounts( _impacted, in.owner );
      for( const auto& out : op.outputs )
         add_authority_accounts( _impacted, out.owner );
   }

   void operator()( const transfer_from_blind_operation& op )
   {
      _impacted.insert( op.to );
      for( const auto& in : op.inputs )
         add_authority_accounts( _impacted, in.owner );
   }

   void operator()( const asset_settle_cancel_operation& op )
   {
      _impacted.insert( op.account );
   }

   void operator()( const fba_distribute_operation& op )
   {
      _impacted.insert( op.account_id );
   }

   void operator()( const sport_create_operation& op ) {}
   void operator()( const sport_update_operation& op ) {}
   void operator()( const sport_delete_operation& op ) {}
   void operator()( const event_group_create_operation& op ) {}
   void operator()( const event_group_update_operation& op ) {}
   void operator()( const event_group_delete_operation& op ) {}
   void operator()( const event_create_operation& op ) {}
   void operator()( const event_update_operation& op ) {}
   void operator()( const event_update_status_operation& op ) {}
   void operator()( const betting_market_rules_create_operation& op ) {}
   void operator()( const betting_market_rules_update_operation& op ) {}
   void operator()( const betting_market_group_create_operation& op ) {}
   void operator()( const betting_market_group_update_operation& op ) {}
   void operator()( const betting_market_create_operation& op ) {}
   void operator()( const betting_market_update_operation& op ) {}
   void operator()( const betting_market_group_resolve_operation& op ) {}
   void operator()( const betting_market_group_cancel_unmatched_bets_operation& op ) {}

   void operator()( const bet_place_operation& op )
   {
      _impacted.insert( op.bettor_id );
   }
   void operator()( const bet_cancel_operation& op )
   {
      _impacted.insert( op.bettor_id );
   }
   void operator()( const bet_canceled_operation& op )
   {
      _impacted.insert( op.bettor_id );
   }
   void operator()( const bet_adjusted_operation& op )
   {
      _impacted.insert( op.bettor_id );
   }
   void operator()( const bet_matched_operation& op )
   {
      _impacted.insert( op.bettor_id );
   }
   void operator()( const betting_market_group_resolved_operation& op )
   {
      _impacted.insert( op.bettor_id );
   }

   void operator()( const tournament_create_operation& op )
   {
      _impacted.insert( op.creator );
      _impacted.insert( op.options.whitelist.begin(), op.options.whitelist.end() );
   }
   void operator()( const tournament_join_operation& op )
   {
      _impacted.insert( op.payer_account_id );
      _impacted.insert( op.player_account_id );
   }
   void operator()( const tournament_leave_operation& op )
   {
      //if account canceling registration is not the player, it must be the payer
      if (op.canceling_account_id != op.player_account_id)
        _impacted.erase( op.canceling_account_id );
      _impacted.erase( op.player_account_id );
   }
   void operator()( const game_move_operation& op )
   {
      _impacted.insert( op.player_account_id );
   }
   void operator()( const tournament_payout_operation& op )
   {
      _impacted.insert( op.payout_account_id );
   }
   void operator()( const affiliate_payout_operation& op )
   {
      _impacted.insert( op.affiliate );
   }
   void operator()( const affiliate_referral_payout_operation& op ) { }
   void operator()( const lottery_asset_create_operation& op) { }
   void operator()( const ticket_purchase_operation& op )
   {
      _impacted.insert( op.buyer );
   }
   void operator()( const lottery_reward_operation& op ) {
      _impacted.insert( op.winner );
   }
   void operator()( const lottery_end_operation& op ) {
      for( auto participant : op.participants ) {
         _impacted.insert(participant.first); 
      }
   }
   void operator()( const sweeps_vesting_claim_operation& op ) { 
      _impacted.insert( op.account );
   }
   void operator()( const son_create_operation& op ){
        _impacted.insert( op.owner_account );
   }
   void operator()( const son_update_operation& op ){
      _impacted.insert( op.owner_account );
   }
   void operator()( const son_delete_operation& op ){
      _impacted.insert( op.owner_account );
   }
   void operator()( const son_heartbeat_operation& op ){
      _impacted.insert( op.owner_account );
   }
   void operator()( const son_report_down_operation& op ){
      _impacted.insert( op.payer );
   }
   void operator()( const son_maintenance_operation& op ){
      _impacted.insert( op.owner_account );
   }
   void operator()( const son_wallet_recreate_operation& op ){
      _impacted.insert( op.payer );
   }
   void operator()( const son_wallet_update_operation& op ){
      _impacted.insert( op.payer );
   }
   void operator()( const son_wallet_transfer_create_operation& op ){
      _impacted.insert( op.payer );
   }
   void operator()( const son_wallet_transfer_process_operation& op ){
      _impacted.insert( op.payer );
   }
   void operator()( const sidechain_address_add_operation& op ){
        _impacted.insert( op.sidechain_address_account );
   }
   void operator()( const sidechain_address_update_operation& op ){
      _impacted.insert( op.sidechain_address_account );
   }
   void operator()( const sidechain_address_delete_operation& op ){
      _impacted.insert( op.sidechain_address_account );
   }
};

void operation_get_impacted_accounts( const operation& op, flat_set<account_id_type>& result )
{
   get_impacted_account_visitor vtor = get_impacted_account_visitor( result );
   op.visit( vtor );
}

void transaction_get_impacted_accounts( const transaction& tx, flat_set<account_id_type>& result )
{
   for( const auto& op : tx.operations )
      operation_get_impacted_accounts( op, result );
}

} }
