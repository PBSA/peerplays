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

#include <boost/multiprecision/integer.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/uint128.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/fba_accumulator_id.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/budget_record_object.hpp>
#include <graphene/chain/buyback_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/fba_object.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/vote_count.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/witness_schedule_object.hpp>
#include <graphene/chain/worker_object.hpp>

#define USE_VESTING_OBJECT_BY_ASSET_BALANCE_INDEX // vesting_balance_object by_asset_balance index needed

namespace graphene { namespace chain {

template<class Index>
vector<std::reference_wrapper<const typename Index::object_type>> database::sort_votable_objects(size_t count) const
{
   using ObjectType = typename Index::object_type;
   const auto& all_objects = get_index_type<Index>().indices();
   count = std::min(count, all_objects.size());
   vector<std::reference_wrapper<const ObjectType>> refs;
   refs.reserve(all_objects.size());
   std::transform(all_objects.begin(), all_objects.end(),
                  std::back_inserter(refs),
                  [](const ObjectType& o) { return std::cref(o); });
   std::partial_sort(refs.begin(), refs.begin() + count, refs.end(),
                   [this](const ObjectType& a, const ObjectType& b)->bool {
      share_type oa_vote = _vote_tally_buffer[a.vote_id];
      share_type ob_vote = _vote_tally_buffer[b.vote_id];
      if( oa_vote != ob_vote )
         return oa_vote > ob_vote;
      return a.vote_id < b.vote_id;
   });

   refs.resize(count, refs.front());
   return refs;
}

template<class... Types>
void database::perform_account_maintenance(std::tuple<Types...> helpers)
{
   const auto& idx = get_index_type<account_index>().indices().get<by_name>();
   for( const account_object& a : idx )
      detail::for_each(helpers, a, detail::gen_seq<sizeof...(Types)>());
}

/// @brief A visitor for @ref worker_type which calls pay_worker on the worker within
struct worker_pay_visitor
{
   private:
      share_type pay;
      database& db;

   public:
      worker_pay_visitor(share_type pay, database& db)
         : pay(pay), db(db) {}

      typedef void result_type;
      template<typename W>
      void operator()(W& worker)const
      {
         worker.pay_worker(pay, db);
      }
};
void database::update_worker_votes()
{
   auto& idx = get_index_type<worker_index>();
   auto itr = idx.indices().get<by_account>().begin();
   bool allow_negative_votes = (head_block_time() < HARDFORK_607_TIME);
   while( itr != idx.indices().get<by_account>().end() )
   {
      modify( *itr, [&]( worker_object& obj ){
         obj.total_votes_for = _vote_tally_buffer[obj.vote_for];
         obj.total_votes_against = allow_negative_votes ? _vote_tally_buffer[obj.vote_against] : 0;
      });
      ++itr;
   }
}

void database::pay_sons()
{
   time_point_sec now = head_block_time();
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   // Current requirement is that we have to pay every 24 hours, so the following check
   if( dpo.son_budget.value > 0 && now - dpo.last_son_payout_time >= fc::days(1)) {
      uint64_t total_txs_signed = 0;
      share_type son_budget = dpo.son_budget;
      get_index_type<son_stats_index>().inspect_all_objects([this, &total_txs_signed](const object& o) {
         const son_statistics_object& s = static_cast<const son_statistics_object&>(o);
         total_txs_signed += s.txs_signed;
      });


      // Now pay off each SON proportional to the number of transactions signed.
      get_index_type<son_stats_index>().inspect_all_objects([this, &total_txs_signed, &dpo, &son_budget](const object& o) {
         const son_statistics_object& s = static_cast<const son_statistics_object&>(o);
         if(s.txs_signed > 0){
            auto son_params = get_global_properties().parameters;
            share_type pay = (s.txs_signed * son_budget.value)/total_txs_signed;

            const auto& idx = get_index_type<son_index>().indices().get<by_id>();
            auto son_obj = idx.find( s.owner );
            modify( *son_obj, [&]( son_object& _son_obj)
            {
               _son_obj.pay_son_fee(pay, *this);
            });
            //Remove the amount paid out to SON from global SON Budget
            modify( dpo, [&]( dynamic_global_property_object& _dpo )
            {
               _dpo.son_budget -= pay;
            } );
            //Reset the tx counter in each son statistics object
            modify( s, [&]( son_statistics_object& _s)
            {
               _s.txs_signed = 0;
            });
         }
      });
      //Note the last son pay out time
      modify( dpo, [&]( dynamic_global_property_object& _dpo )
      {
         _dpo.last_son_payout_time = now;
      });
   }
}

void database::update_son_metrics()
{
   const auto& son_idx = get_index_type<son_index>().indices().get< by_id >();
   for( auto& son : son_idx )
   {
      auto& stats = son.statistics(*this);
      modify( stats, [&]( son_statistics_object& _stats)
      {
         _stats.total_downtime += _stats.current_interval_downtime;
         _stats.current_interval_downtime = 0;
      });
   }
}

void database::pay_workers( share_type& budget )
{
//   ilog("Processing payroll! Available budget is ${b}", ("b", budget));
   vector<std::reference_wrapper<const worker_object>> active_workers;
   get_index_type<worker_index>().inspect_all_objects([this, &active_workers](const object& o) {
      const worker_object& w = static_cast<const worker_object&>(o);
      auto now = head_block_time();
      if( w.is_active(now) && w.approving_stake() > 0 )
         active_workers.emplace_back(w);
   });

   // worker with more votes is preferred
   // if two workers exactly tie for votes, worker with lower ID is preferred
   std::sort(active_workers.begin(), active_workers.end(), [this](const worker_object& wa, const worker_object& wb) {
      share_type wa_vote = wa.approving_stake();
      share_type wb_vote = wb.approving_stake();
      if( wa_vote != wb_vote )
         return wa_vote > wb_vote;
      return wa.id < wb.id;
   });

   for( uint32_t i = 0; i < active_workers.size() && budget > 0; ++i )
   {
      const worker_object& active_worker = active_workers[i];
      share_type requested_pay = active_worker.daily_pay;
      if( head_block_time() - get_dynamic_global_properties().last_budget_time != fc::days(1) )
      {
         fc::uint128 pay(requested_pay.value);
         pay *= (head_block_time() - get_dynamic_global_properties().last_budget_time).count();
         pay /= fc::days(1).count();
         requested_pay = pay.to_uint64();
      }

      share_type actual_pay = std::min(budget, requested_pay);
      //ilog(" ==> Paying ${a} to worker ${w}", ("w", active_worker.id)("a", actual_pay));
      modify(active_worker, [&](worker_object& w) {
         w.worker.visit(worker_pay_visitor(actual_pay, *this));
      });

      budget -= actual_pay;
   }
}

void database::update_active_witnesses()
{ try {
   assert( _witness_count_histogram_buffer.size() > 0 );
   share_type stake_target = (_total_voting_stake-_witness_count_histogram_buffer[0]) / 2;

   /// accounts that vote for 0 or 1 witness do not get to express an opinion on
   /// the number of witnesses to have (they abstain and are non-voting accounts)

   share_type stake_tally = 0; 

   size_t witness_count = 0;
   if( stake_target > 0 )
   {
      while( (witness_count < _witness_count_histogram_buffer.size() - 1)
             && (stake_tally <= stake_target) )
      {
         stake_tally += _witness_count_histogram_buffer[++witness_count];
      }
   }

   const chain_property_object& cpo = get_chain_properties();
   auto wits = sort_votable_objects<witness_index>(std::max(witness_count*2+1, (size_t)cpo.immutable_parameters.min_witness_count));

   const global_property_object& gpo = get_global_properties();

   const auto& all_witnesses = get_index_type<witness_index>().indices();

   for( const witness_object& wit : all_witnesses )
   {
      modify( wit, [&]( witness_object& obj ){
              obj.total_votes = _vote_tally_buffer[wit.vote_id];
              });
   }

   // Update witness authority
   modify( get(GRAPHENE_WITNESS_ACCOUNT), [&]( account_object& a )
   {
      if( head_block_time() < HARDFORK_533_TIME )
      {
         uint64_t total_votes = 0;
         map<account_id_type, uint64_t> weights;
         a.active.weight_threshold = 0;
         a.active.clear();

         for( const witness_object& wit : wits )
         {
            weights.emplace(wit.witness_account, _vote_tally_buffer[wit.vote_id]);
            total_votes += _vote_tally_buffer[wit.vote_id];
         }

         // total_votes is 64 bits. Subtract the number of leading low bits from 64 to get the number of useful bits,
         // then I want to keep the most significant 16 bits of what's left.
         int8_t bits_to_drop = std::max(int(boost::multiprecision::detail::find_msb(total_votes)) - 15, 0);
         for( const auto& weight : weights )
         {
            // Ensure that everyone has at least one vote. Zero weights aren't allowed.
            uint16_t votes = std::max((weight.second >> bits_to_drop), uint64_t(1) );
            a.active.account_auths[weight.first] += votes;
            a.active.weight_threshold += votes;
         }

         a.active.weight_threshold /= 2;
         a.active.weight_threshold += 1;
      }
      else
      {
         vote_counter vc;
         for( const witness_object& wit : wits )
            vc.add( wit.witness_account, std::max(_vote_tally_buffer[wit.vote_id], UINT64_C(1)) );
         vc.finish( a.active );
      }
   } );

   modify(gpo, [&]( global_property_object& gp ){
      gp.active_witnesses.clear();
      gp.active_witnesses.reserve(wits.size());
      std::transform(wits.begin(), wits.end(),
                     std::inserter(gp.active_witnesses, gp.active_witnesses.end()),
                     [](const witness_object& w) {
         return w.id;
      });
   });

   const witness_schedule_object& wso = witness_schedule_id_type()(*this);
   modify(wso, [&](witness_schedule_object& _wso)
   {
      _wso.scheduler.update(gpo.active_witnesses);
   });
} FC_CAPTURE_AND_RETHROW() }

void database::update_active_committee_members()
{ try {
   assert( _committee_count_histogram_buffer.size() > 0 );
   share_type stake_target = (_total_voting_stake-_committee_count_histogram_buffer[0]) / 2;

   /// accounts that vote for 0 or 1 witness do not get to express an opinion on
   /// the number of witnesses to have (they abstain and are non-voting accounts)
   uint64_t stake_tally = 0; // _committee_count_histogram_buffer[0];
   size_t committee_member_count = 0;
   if( stake_target > 0 )
      while( (committee_member_count < _committee_count_histogram_buffer.size() - 1)
             && (stake_tally <= stake_target) )
         stake_tally += _committee_count_histogram_buffer[++committee_member_count];

   const chain_property_object& cpo = get_chain_properties();
   auto committee_members = sort_votable_objects<committee_member_index>(std::max(committee_member_count*2+1, (size_t)cpo.immutable_parameters.min_committee_member_count));

   for( const committee_member_object& del : committee_members )
   {
      modify( del, [&]( committee_member_object& obj ){
              obj.total_votes = _vote_tally_buffer[del.vote_id];
              });
   }

   // Update committee authorities
   if( !committee_members.empty() )
   {
      modify(get(GRAPHENE_COMMITTEE_ACCOUNT), [&](account_object& a)
      {
         if( head_block_time() < HARDFORK_533_TIME )
         {
            uint64_t total_votes = 0;
            map<account_id_type, uint64_t> weights;
            a.active.weight_threshold = 0;
            a.active.clear();

            for( const committee_member_object& del : committee_members )
            {
               weights.emplace(del.committee_member_account, _vote_tally_buffer[del.vote_id]);
               total_votes += _vote_tally_buffer[del.vote_id];
            }

            // total_votes is 64 bits. Subtract the number of leading low bits from 64 to get the number of useful bits,
            // then I want to keep the most significant 16 bits of what's left.
            int8_t bits_to_drop = std::max(int(boost::multiprecision::detail::find_msb(total_votes)) - 15, 0);
            for( const auto& weight : weights )
            {
               // Ensure that everyone has at least one vote. Zero weights aren't allowed.
               uint16_t votes = std::max((weight.second >> bits_to_drop), uint64_t(1) );
               a.active.account_auths[weight.first] += votes;
               a.active.weight_threshold += votes;
            }

            a.active.weight_threshold /= 2;
            a.active.weight_threshold += 1;
         }
         else
         {
            vote_counter vc;
            for( const committee_member_object& cm : committee_members )
               vc.add( cm.committee_member_account, std::max(_vote_tally_buffer[cm.vote_id], UINT64_C(1)) );
            vc.finish( a.active );
         }
      } );
      modify(get(GRAPHENE_RELAXED_COMMITTEE_ACCOUNT), [&](account_object& a) {
         a.active = get(GRAPHENE_COMMITTEE_ACCOUNT).active;
      });
   }
   modify(get_global_properties(), [&](global_property_object& gp) {
      gp.active_committee_members.clear();
      std::transform(committee_members.begin(), committee_members.end(),
                     std::inserter(gp.active_committee_members, gp.active_committee_members.begin()),
                     [](const committee_member_object& d) { return d.id; });
   });
} FC_CAPTURE_AND_RETHROW() }

void database::update_active_sons()
{ try {
   assert( _son_count_histogram_buffer.size() > 0 );
   share_type stake_target = (_total_voting_stake-_son_count_histogram_buffer[0]) / 2;

   /// accounts that vote for 0 or 1 son do not get to express an opinion on
   /// the number of sons to have (they abstain and are non-voting accounts)

   share_type stake_tally = 0;

   size_t son_count = 0;
   if( stake_target > 0 )
   {
      while( (son_count < _son_count_histogram_buffer.size() - 1)
             && (stake_tally <= stake_target) )
      {
         stake_tally += _son_count_histogram_buffer[++son_count];
      }
   }

   const global_property_object& gpo = get_global_properties();
   const chain_parameters& cp = gpo.parameters;
   auto sons = sort_votable_objects<son_index>(cp.maximum_son_count);

   const auto& all_sons = get_index_type<son_index>().indices();

   auto& local_vote_buffer_ref = _vote_tally_buffer;
   for( const son_object& son : all_sons )
   {
      if(son.status == son_status::request_maintenance)
      {
         auto& stats = son.statistics(*this);
         modify( stats, [&]( son_statistics_object& _s){
               _s.last_down_timestamp = head_block_time();
            });
      }
      modify( son, [local_vote_buffer_ref]( son_object& obj ){
              obj.total_votes = local_vote_buffer_ref[obj.vote_id];
              if(obj.status == son_status::request_maintenance)
                 obj.status = son_status::in_maintenance;
              });
   }

   // Update SON authority
   modify( get(GRAPHENE_SON_ACCOUNT_ID), [&]( account_object& a )
   {
      if( head_block_time() < HARDFORK_533_TIME )
      {
         uint64_t total_votes = 0;
         map<account_id_type, uint64_t> weights;
         a.active.weight_threshold = 0;
         a.active.clear();

         for( const son_object& son : sons )
         {
            weights.emplace(son.son_account, _vote_tally_buffer[son.vote_id]);
            total_votes += _vote_tally_buffer[son.vote_id];
         }

         // total_votes is 64 bits. Subtract the number of leading low bits from 64 to get the number of useful bits,
         // then I want to keep the most significant 16 bits of what's left.
         int8_t bits_to_drop = std::max(int(boost::multiprecision::detail::find_msb(total_votes)) - 15, 0);
         for( const auto& weight : weights )
         {
            // Ensure that everyone has at least one vote. Zero weights aren't allowed.
            uint16_t votes = std::max((weight.second >> bits_to_drop), uint64_t(1) );
            a.active.account_auths[weight.first] += votes;
            a.active.weight_threshold += votes;
         }

         a.active.weight_threshold /= 2;
         a.active.weight_threshold += 1;
      }
      else
      {
         vote_counter vc;
         for( const son_object& son : sons )
            vc.add( son.son_account, std::max(_vote_tally_buffer[son.vote_id], UINT64_C(1)) );
         vc.finish( a.active );
      }
   } );

   // Compare current and to-be lists of active sons
   //const global_property_object& gpo = get_global_properties();
   auto cur_active_sons = gpo.active_sons;
   vector<son_info> new_active_sons;
   for( const son_object& son : sons ) {
      son_info swi;
      swi.son_id = son.id;
      swi.total_votes = son.total_votes;
      swi.signing_key = son.signing_key;
      swi.sidechain_public_keys = son.sidechain_public_keys;
      new_active_sons.push_back(swi);
   }

   bool son_sets_equal = (cur_active_sons.size() == new_active_sons.size());
   if (son_sets_equal) {
      for( size_t i = 0; i < cur_active_sons.size(); i++ ) {
         son_sets_equal = son_sets_equal && cur_active_sons.at(i) == new_active_sons.at(i);
      }
   }

   if (son_sets_equal) {
      ilog( "Active SONs set NOT CHANGED" );
   } else {
      ilog( "Active SONs set CHANGED" );

      bool should_recreate_pw = true;

      // Expire for current son_wallet_object wallet, if exists
      const auto& idx_swi = get_index_type<son_wallet_index>().indices().get<by_id>();
      auto obj = idx_swi.rbegin();
      if (obj != idx_swi.rend()) {
         // Compare current wallet SONs and to-be lists of active sons
         auto cur_wallet_sons = (*obj).sons;

         bool wallet_son_sets_equal = (cur_wallet_sons.size() == new_active_sons.size());
         if (wallet_son_sets_equal) {
            for( size_t i = 0; i < cur_wallet_sons.size(); i++ ) {
               wallet_son_sets_equal = wallet_son_sets_equal && cur_wallet_sons.at(i) == new_active_sons.at(i);
            }
         }

         should_recreate_pw = !wallet_son_sets_equal;

         if (should_recreate_pw) {
            modify(*obj, [&, obj](son_wallet_object &swo) {
               swo.expires = head_block_time();
            });
         }
      }

      if (should_recreate_pw) {
         // Create new son_wallet_object, to initiate wallet recreation
         create<son_wallet_object>( [&]( son_wallet_object& obj ) {
            obj.valid_from = head_block_time();
            obj.expires = time_point_sec::maximum();
            obj.sons.insert(obj.sons.end(), new_active_sons.begin(), new_active_sons.end());
         });
      }

      vector<son_info> sons_to_remove;
      // find all cur_active_sons members that is not in new_active_sons
      for_each(cur_active_sons.begin(), cur_active_sons.end(),
               [&sons_to_remove, &new_active_sons](const son_info& si)
               {
                  if(std::find(new_active_sons.begin(), new_active_sons.end(), si) ==
                        new_active_sons.end())
                  {
                     sons_to_remove.push_back(si);
                  }
               }
      );
      const auto& idx = get_index_type<son_index>().indices().get<by_id>();
      for( const son_info& si : sons_to_remove )
      {
         auto son = idx.find( si.son_id );
         if(son == idx.end()) // SON is deleted already
            continue;
         // keep maintenance status for nodes becoming inactive
         if(son->status == son_status::active)
         {
            modify( *son, [&]( son_object& obj ){
                    obj.status = son_status::inactive;
                    });
         }
      }
      vector<son_info> sons_to_add;
      // find all new_active_sons members that is not in cur_active_sons
      for_each(new_active_sons.begin(), new_active_sons.end(),
               [&sons_to_add, &cur_active_sons](const son_info& si)
               {
                  if(std::find(cur_active_sons.begin(), cur_active_sons.end(), si) ==
                        cur_active_sons.end())
                  {
                     sons_to_add.push_back(si);
                  }
               }
      );
      for( const son_info& si : sons_to_add )
      {
         auto son = idx.find( si.son_id );
         FC_ASSERT(son != idx.end(), "Invalid SON in active list, id={sonid}.", ("sonid", si.son_id));
         // keep maintenance status for new nodes
         if(son->status == son_status::inactive)
         {
            modify( *son, [&]( son_object& obj ){
                    obj.status = son_status::active;
                    });
         }
      }
   }

   modify(gpo, [&]( global_property_object& gp ){
      gp.active_sons.clear();
      gp.active_sons.reserve(new_active_sons.size());
      gp.active_sons.insert(gp.active_sons.end(), new_active_sons.begin(), new_active_sons.end());
   });

   const son_schedule_object& sso = son_schedule_id_type()(*this);
   modify(sso, [&](son_schedule_object& _sso)
   {
      flat_set<son_id_type> active_sons;
      active_sons.reserve(gpo.active_sons.size());
      std::transform(gpo.active_sons.begin(), gpo.active_sons.end(),
                     std::inserter(active_sons, active_sons.end()),
                     [](const son_info& swi) {
         return swi.son_id;
      });
      _sso.scheduler.update(active_sons);
   });

   update_son_metrics();

   if(gpo.active_sons.size() > 0 ) {
      if(gpo.parameters.get_son_btc_account_id() == GRAPHENE_NULL_ACCOUNT) {
         const auto& son_btc_account = create<account_object>( [&]( account_object& obj ) {
            uint64_t total_votes = 0;
            obj.name = "son_btc_account";
            obj.statistics = create<account_statistics_object>([&]( account_statistics_object& acc_stat ){ acc_stat.owner = obj.id; }).id;
            obj.membership_expiration_date = time_point_sec::maximum();
            obj.network_fee_percentage = GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;
            obj.lifetime_referrer_fee_percentage = GRAPHENE_100_PERCENT - GRAPHENE_DEFAULT_NETWORK_PERCENT_OF_FEE;

            for( const auto& son_info : gpo.active_sons )
            {
               const son_object& son = get(son_info.son_id);
               total_votes += _vote_tally_buffer[son.vote_id];
            }
            // total_votes is 64 bits. Subtract the number of leading low bits from 64 to get the number of useful bits,
            // then I want to keep the most significant 16 bits of what's left.
            int8_t bits_to_drop = std::max(int(boost::multiprecision::detail::find_msb(total_votes)) - 15, 0);

            for( const auto& son_info : gpo.active_sons )
            {
               // Ensure that everyone has at least one vote. Zero weights aren't allowed.
               const son_object& son = get(son_info.son_id);
               uint16_t votes = std::max((_vote_tally_buffer[son.vote_id] >> bits_to_drop), uint64_t(1) );
               obj.owner.account_auths[son.son_account] += votes;
               obj.owner.weight_threshold += votes;
               obj.active.account_auths[son.son_account] += votes;
               obj.active.weight_threshold += votes;
            }
            obj.owner.weight_threshold *= 2;
            obj.owner.weight_threshold /= 3;
            obj.owner.weight_threshold += 1;
            obj.active.weight_threshold *= 2;
            obj.active.weight_threshold /= 3;
            obj.active.weight_threshold += 1;
         });

         modify( gpo, [&]( global_property_object& gpo ) {
            gpo.parameters.extensions.value.son_btc_account = son_btc_account.get_id();
            if( gpo.pending_parameters )
               gpo.pending_parameters->extensions.value.son_btc_account = son_btc_account.get_id();
         });
      } else {
         modify( get(gpo.parameters.get_son_btc_account_id()), [&]( account_object& obj )
         {
            uint64_t total_votes = 0;
            obj.owner.weight_threshold = 0;
            obj.owner.account_auths.clear();
            obj.active.weight_threshold = 0;
            obj.active.account_auths.clear();
            for( const auto& son_info : gpo.active_sons )
            {
               const son_object& son = get(son_info.son_id);
               total_votes += _vote_tally_buffer[son.vote_id];
            }
            // total_votes is 64 bits. Subtract the number of leading low bits from 64 to get the number of useful bits,
            // then I want to keep the most significant 16 bits of what's left.
            int8_t bits_to_drop = std::max(int(boost::multiprecision::detail::find_msb(total_votes)) - 15, 0);
            for( const auto& son_info : gpo.active_sons )
            {
               // Ensure that everyone has at least one vote. Zero weights aren't allowed.
               const son_object& son = get(son_info.son_id);
               uint16_t votes = std::max((_vote_tally_buffer[son.vote_id] >> bits_to_drop), uint64_t(1) );
               obj.owner.account_auths[son.son_account] += votes;
               obj.owner.weight_threshold += votes;
               obj.active.account_auths[son.son_account] += votes;
               obj.active.weight_threshold += votes;
            }
            obj.owner.weight_threshold *= 2;
            obj.owner.weight_threshold /= 3;
            obj.owner.weight_threshold += 1;
            obj.active.weight_threshold *= 2;
            obj.active.weight_threshold /= 3;
            obj.active.weight_threshold += 1;
         });
      }
   }
} FC_CAPTURE_AND_RETHROW() }

void database::initialize_budget_record( fc::time_point_sec now, budget_record& rec )const
{
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   const asset_object& core = asset_id_type(0)(*this);
   const asset_dynamic_data_object& core_dd = core.dynamic_asset_data_id(*this);

   rec.from_initial_reserve = core.reserved(*this);
   rec.from_accumulated_fees = core_dd.accumulated_fees;
   rec.from_unused_witness_budget = dpo.witness_budget;

   if(    (dpo.last_budget_time == fc::time_point_sec())
       || (now <= dpo.last_budget_time) )
   {
      rec.time_since_last_budget = 0;
      return;
   }

   int64_t dt = (now - dpo.last_budget_time).to_seconds();
   rec.time_since_last_budget = uint64_t( dt );

   // We'll consider accumulated_fees to be reserved at the BEGINNING
   // of the maintenance interval.  However, for speed we only
   // call modify() on the asset_dynamic_data_object once at the
   // end of the maintenance interval.  Thus the accumulated_fees
   // are available for the budget at this point, but not included
   // in core.reserved().
   share_type reserve = rec.from_initial_reserve + core_dd.accumulated_fees;
   // Similarly, we consider leftover witness_budget to be burned
   // at the BEGINNING of the maintenance interval.
   reserve += dpo.witness_budget;

   fc::uint128_t budget_u128 = reserve.value;
   budget_u128 *= uint64_t(dt);
   budget_u128 *= GRAPHENE_CORE_ASSET_CYCLE_RATE;
   //round up to the nearest satoshi -- this is necessary to ensure
   //   there isn't an "untouchable" reserve, and we will eventually
   //   be able to use the entire reserve
   budget_u128 += ((uint64_t(1) << GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS) - 1);
   budget_u128 >>= GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS;
   share_type budget;
   if( budget_u128 < reserve.value )
      rec.total_budget = share_type(budget_u128.to_uint64());
   else
      rec.total_budget = reserve;

   return;
}

/**
 * Update the budget for witnesses and workers.
 */
void database::process_budget()
{
   try
   {
      const global_property_object& gpo = get_global_properties();
      const dynamic_global_property_object& dpo = get_dynamic_global_properties();
      const asset_dynamic_data_object& core =
         asset_id_type(0)(*this).dynamic_asset_data_id(*this);
      fc::time_point_sec now = head_block_time();

      int64_t time_to_maint = (dpo.next_maintenance_time - now).to_seconds();
      //
      // The code that generates the next maintenance time should
      //    only produce a result in the future.  If this assert
      //    fails, then the next maintenance time algorithm is buggy.
      //
      assert( time_to_maint > 0 );
      //
      // Code for setting chain parameters should validate
      //    block_interval > 0 (as well as the humans proposing /
      //    voting on changes to block interval).
      //
      assert( gpo.parameters.block_interval > 0 );
      uint64_t blocks_to_maint = (uint64_t(time_to_maint) + gpo.parameters.block_interval - 1) / gpo.parameters.block_interval;

      // blocks_to_maint > 0 because time_to_maint > 0,
      // which means numerator is at least equal to block_interval

      budget_record rec;
      initialize_budget_record( now, rec );
      share_type available_funds = rec.total_budget;

      share_type witness_budget = gpo.parameters.witness_pay_per_block.value * blocks_to_maint;
      rec.requested_witness_budget = witness_budget;
      witness_budget = std::min(witness_budget, available_funds);
      rec.witness_budget = witness_budget;
      available_funds -= witness_budget;

      // We should not factor-in the son budget before SON HARDFORK
      share_type son_budget = 0;
      if(now >= HARDFORK_SON_TIME){
         // Before making a budget we should pay out SONs for the last day
         // This function should check if its time to pay sons
         // and modify the global son funds accordingly, whatever is left is passed on to next budget
         pay_sons();
         rec.leftover_son_funds = dpo.son_budget;
         available_funds += rec.leftover_son_funds;
         son_budget = gpo.parameters.son_pay_daily_max();
         son_budget = std::min(son_budget, available_funds);
         rec.son_budget = son_budget;
         available_funds -= son_budget;
      }

      fc::uint128_t worker_budget_u128 = gpo.parameters.worker_budget_per_day.value;
      worker_budget_u128 *= uint64_t(time_to_maint);
      worker_budget_u128 /= 60*60*24;

      share_type worker_budget;
      if( worker_budget_u128 >= available_funds.value )
         worker_budget = available_funds;
      else
         worker_budget = worker_budget_u128.to_uint64();
      rec.worker_budget = worker_budget;
      available_funds -= worker_budget;

      share_type leftover_worker_funds = worker_budget;
      pay_workers(leftover_worker_funds);
      rec.leftover_worker_funds = leftover_worker_funds;
      available_funds += leftover_worker_funds;

      rec.supply_delta = rec.witness_budget
         + rec.worker_budget
         + rec.son_budget
         - rec.leftover_worker_funds
         - rec.from_accumulated_fees
         - rec.from_unused_witness_budget
         - rec.leftover_son_funds;

      modify(core, [&]( asset_dynamic_data_object& _core )
      {
         _core.current_supply = (_core.current_supply + rec.supply_delta );

         assert( rec.supply_delta ==
                                   witness_budget
                                 + worker_budget
                                 + son_budget
                                 - leftover_worker_funds
                                 - _core.accumulated_fees
                                 - dpo.witness_budget
                                 - dpo.son_budget
                                );
         _core.accumulated_fees = 0;
      });

      modify(dpo, [&]( dynamic_global_property_object& _dpo )
      {
         // Since initial witness_budget was rolled into
         // available_funds, we replace it with witness_budget
         // instead of adding it.
         _dpo.witness_budget = witness_budget;
         _dpo.son_budget = son_budget;
         _dpo.last_budget_time = now;
      });

      create< budget_record_object >( [&]( budget_record_object& _rec )
      {
         _rec.time = head_block_time();
         _rec.record = rec;
      });

      // available_funds is money we could spend, but don't want to.
      // we simply let it evaporate back into the reserve.
   }
   FC_CAPTURE_AND_RETHROW()
}

template< typename Visitor >
void visit_special_authorities( const database& db, Visitor visit )
{
   const auto& sa_idx = db.get_index_type< special_authority_index >().indices().get<by_id>();

   for( const special_authority_object& sao : sa_idx )
   {
      const account_object& acct = sao.account(db);
      if( acct.owner_special_authority.which() != special_authority::tag< no_special_authority >::value )
      {
         visit( acct, true, acct.owner_special_authority );
      }
      if( acct.active_special_authority.which() != special_authority::tag< no_special_authority >::value )
      {
         visit( acct, false, acct.active_special_authority );
      }
   }
}

void update_top_n_authorities( database& db )
{
   visit_special_authorities( db,
   [&]( const account_object& acct, bool is_owner, const special_authority& auth )
   {
      if( auth.which() == special_authority::tag< top_holders_special_authority >::value )
      {
         // use index to grab the top N holders of the asset and vote_counter to obtain the weights

         const top_holders_special_authority& tha = auth.get< top_holders_special_authority >();
         vote_counter vc;
         const auto& bal_idx = db.get_index_type< account_balance_index >().indices().get< by_asset_balance >();
         uint8_t num_needed = tha.num_top_holders;
         if( num_needed == 0 )
            return;

         // find accounts
         const auto range = bal_idx.equal_range( boost::make_tuple( tha.asset ) );
         for( const account_balance_object& bal : boost::make_iterator_range( range.first, range.second ) )
         {
             assert( bal.asset_type == tha.asset );
             if( bal.owner == acct.id )
                continue;
             vc.add( bal.owner, bal.balance.value );
             --num_needed;
             if( num_needed == 0 )
                break;
         }

         db.modify( acct, [&]( account_object& a )
         {
            vc.finish( is_owner ? a.owner : a.active );
            if( !vc.is_empty() )
               a.top_n_control_flags |= (is_owner ? account_object::top_n_control_owner : account_object::top_n_control_active);
         } );
      }
   } );
}

void split_fba_balance(
   database& db,
   uint64_t fba_id,
   uint16_t network_pct,
   uint16_t designated_asset_buyback_pct,
   uint16_t designated_asset_issuer_pct
)
{
   FC_ASSERT( uint32_t(network_pct) + uint32_t(designated_asset_buyback_pct) + uint32_t(designated_asset_issuer_pct) == GRAPHENE_100_PERCENT );
   const fba_accumulator_object& fba = fba_accumulator_id_type( fba_id )(db);
   if( fba.accumulated_fba_fees == 0 )
      return;

   const asset_object& core = asset_id_type(0)(db);
   const asset_dynamic_data_object& core_dd = core.dynamic_asset_data_id(db);

   if( !fba.is_configured(db) )
   {
      ilog( "${n} core given to network at block ${b} due to non-configured FBA", ("n", fba.accumulated_fba_fees)("b", db.head_block_time()) );
      db.modify( core_dd, [&]( asset_dynamic_data_object& _core_dd )
      {
         _core_dd.current_supply -= fba.accumulated_fba_fees;
      } );
      db.modify( fba, [&]( fba_accumulator_object& _fba )
      {
         _fba.accumulated_fba_fees = 0;
      } );
      return;
   }

   fc::uint128_t buyback_amount_128 = fba.accumulated_fba_fees.value;
   buyback_amount_128 *= designated_asset_buyback_pct;
   buyback_amount_128 /= GRAPHENE_100_PERCENT;
   share_type buyback_amount = buyback_amount_128.to_uint64();

   fc::uint128_t issuer_amount_128 = fba.accumulated_fba_fees.value;
   issuer_amount_128 *= designated_asset_issuer_pct;
   issuer_amount_128 /= GRAPHENE_100_PERCENT;
   share_type issuer_amount = issuer_amount_128.to_uint64();

   // this assert should never fail
   FC_ASSERT( buyback_amount + issuer_amount <= fba.accumulated_fba_fees );

   share_type network_amount = fba.accumulated_fba_fees - (buyback_amount + issuer_amount);

   const asset_object& designated_asset = (*fba.designated_asset)(db);

   if( network_amount != 0 )
   {
      db.modify( core_dd, [&]( asset_dynamic_data_object& _core_dd )
      {
         _core_dd.current_supply -= network_amount;
      } );
   }

   fba_distribute_operation vop;
   vop.account_id = *designated_asset.buyback_account;
   vop.fba_id = fba.id;
   vop.amount = buyback_amount;
   if( vop.amount != 0 )
   {
      db.adjust_balance( *designated_asset.buyback_account, asset(buyback_amount) );
      db.push_applied_operation(vop);
   }

   vop.account_id = designated_asset.issuer;
   vop.fba_id = fba.id;
   vop.amount = issuer_amount;
   if( vop.amount != 0 )
   {
      db.adjust_balance( designated_asset.issuer, asset(issuer_amount) );
      db.push_applied_operation(vop);
   }

   db.modify( fba, [&]( fba_accumulator_object& _fba )
   {
      _fba.accumulated_fba_fees = 0;
   } );
}

void distribute_fba_balances( database& db )
{
   split_fba_balance( db, fba_accumulator_id_transfer_to_blind  , 20*GRAPHENE_1_PERCENT, 60*GRAPHENE_1_PERCENT, 20*GRAPHENE_1_PERCENT );
   split_fba_balance( db, fba_accumulator_id_blind_transfer     , 20*GRAPHENE_1_PERCENT, 60*GRAPHENE_1_PERCENT, 20*GRAPHENE_1_PERCENT );
   split_fba_balance( db, fba_accumulator_id_transfer_from_blind, 20*GRAPHENE_1_PERCENT, 60*GRAPHENE_1_PERCENT, 20*GRAPHENE_1_PERCENT );
}

void create_buyback_orders( database& db )
{
   const auto& bbo_idx = db.get_index_type< buyback_index >().indices().get<by_id>();
   const auto& bal_idx = db.get_index_type< primary_index< account_balance_index > >().get_secondary_index< balances_by_account_index >();

   for( const buyback_object& bbo : bbo_idx )
   {
      const asset_object& asset_to_buy = bbo.asset_to_buy(db);
      assert( asset_to_buy.buyback_account.valid() );

      const account_object& buyback_account = (*(asset_to_buy.buyback_account))(db);

      if( !buyback_account.allowed_assets.valid() )
      {
         wlog( "skipping buyback account ${b} at block ${n} because allowed_assets does not exist", ("b", buyback_account)("n", db.head_block_num()) );
         continue;
      }

      for( const auto& entry : bal_idx.get_account_balances( buyback_account.id ) )
      {
         const auto* it = entry.second;
         asset_id_type asset_to_sell = it->asset_type;
         share_type amount_to_sell = it->balance;
         if( asset_to_sell == asset_to_buy.id )
            continue;
         if( amount_to_sell == 0 )
            continue;
         if( buyback_account.allowed_assets->find( asset_to_sell ) == buyback_account.allowed_assets->end() )
         {
            wlog( "buyback account ${b} not selling disallowed holdings of asset ${a} at block ${n}", ("b", buyback_account)("a", asset_to_sell)("n", db.head_block_num()) );
            continue;
         }

         try
         {
            transaction_evaluation_state buyback_context(&db);
            buyback_context.skip_fee_schedule_check = true;

            limit_order_create_operation create_vop;
            create_vop.fee = asset( 0, asset_id_type() );
            create_vop.seller = buyback_account.id;
            create_vop.amount_to_sell = asset( amount_to_sell, asset_to_sell );
            create_vop.min_to_receive = asset( 1, asset_to_buy.id );
            create_vop.expiration = time_point_sec::maximum();
            create_vop.fill_or_kill = false;

            limit_order_id_type order_id = db.apply_operation( buyback_context, create_vop ).get< object_id_type >();

            if( db.find( order_id ) != nullptr )
            {
               limit_order_cancel_operation cancel_vop;
               cancel_vop.fee = asset( 0, asset_id_type() );
               cancel_vop.order = order_id;
               cancel_vop.fee_paying_account = buyback_account.id;

               db.apply_operation( buyback_context, cancel_vop );
            }
         }
         catch( const fc::exception& e )
         {
            // we can in fact get here, e.g. if asset issuer of buy/sell asset blacklists/whitelists the buyback account
            wlog( "Skipping buyback processing selling ${as} for ${ab} for buyback account ${b} at block ${n}; exception was ${e}",
                  ("as", asset_to_sell)("ab", asset_to_buy)("b", buyback_account)("n", db.head_block_num())("e", e.to_detail_string()) );
            continue;
         }
      }
   }
   return;
}

void deprecate_annual_members( database& db )
{
   const auto& account_idx = db.get_index_type<account_index>().indices().get<by_id>();
   fc::time_point_sec now = db.head_block_time();
   for( const account_object& acct : account_idx )
   {
      try
      {
         transaction_evaluation_state upgrade_context(&db);
         upgrade_context.skip_fee_schedule_check = true;

         if( acct.is_annual_member( now ) )
         {
            account_upgrade_operation upgrade_vop;
            upgrade_vop.fee = asset( 0, asset_id_type() );
            upgrade_vop.account_to_upgrade = acct.id;
            upgrade_vop.upgrade_to_lifetime_member = true;
            db.apply_operation( upgrade_context, upgrade_vop );
         }
      }
      catch( const fc::exception& e )
      {
         // we can in fact get here, e.g. if asset issuer of buy/sell asset blacklists/whitelists the buyback account
         wlog( "Skipping annual member deprecate processing for account ${a} (${an}) at block ${n}; exception was ${e}",
               ("a", acct.id)("an", acct.name)("n", db.head_block_num())("e", e.to_detail_string()) );
         continue;
      }
   }
   return;
}

// Schedules payouts from a dividend distribution account to the current holders of the
// dividend-paying asset.  This takes any deposits made to the dividend distribution account
// since the last time it was called, and distributes them to the current owners of the
// dividend-paying asset according to the amount they own.
void schedule_pending_dividend_balances(database& db, 
                                        const asset_object& dividend_holder_asset_obj,
                                        const asset_dividend_data_object& dividend_data,
                                        const fc::time_point_sec& current_head_block_time, 
                                        const account_balance_index& balance_index,
                                        const vesting_balance_index& vesting_index,
                                        const total_distributed_dividend_balance_object_index& distributed_dividend_balance_index,
                                        const pending_dividend_payout_balance_for_holder_object_index& pending_payout_balance_index)
{ try {
   dlog("Processing dividend payments for dividend holder asset type ${holder_asset} at time ${t}",
        ("holder_asset", dividend_holder_asset_obj.symbol)("t", db.head_block_time()));
   auto balance_by_acc_index = db.get_index_type< primary_index< account_balance_index > >().get_secondary_index< balances_by_account_index >();
   auto current_distribution_account_balance_range = 
      //balance_index.indices().get<by_account_asset>().equal_range(boost::make_tuple(dividend_data.dividend_distribution_account));
      balance_by_acc_index.get_account_balances(dividend_data.dividend_distribution_account);
   auto previous_distribution_account_balance_range =
      distributed_dividend_balance_index.indices().get<by_dividend_payout_asset>().equal_range(boost::make_tuple(dividend_holder_asset_obj.id));
   // the current range is now all current balances for the distribution account, sorted by asset_type
   // the previous range is now all previous balances for this account, sorted by asset type

   const auto& gpo = db.get_global_properties();

   // get the list of accounts that hold nonzero balances of the dividend asset
   auto holder_balances_begin = 
      balance_index.indices().get<by_asset_balance>().lower_bound(boost::make_tuple(dividend_holder_asset_obj.id));
   auto holder_balances_end =
      balance_index.indices().get<by_asset_balance>().upper_bound(boost::make_tuple(dividend_holder_asset_obj.id, share_type()));
   uint32_t holder_account_count = std::distance(holder_balances_begin, holder_balances_end);
   uint64_t distribution_base_fee = gpo.parameters.current_fees->get<asset_dividend_distribution_operation>().distribution_base_fee;
   uint32_t distribution_fee_per_holder = gpo.parameters.current_fees->get<asset_dividend_distribution_operation>().distribution_fee_per_holder;
   // the fee, in BTS, for distributing each asset in the account
   uint64_t total_fee_per_asset_in_core = distribution_base_fee + holder_account_count * (uint64_t)distribution_fee_per_holder;

   std::map<account_id_type, share_type> vesting_amounts;
#ifdef USE_VESTING_OBJECT_BY_ASSET_BALANCE_INDEX
   // get only once a collection of accounts that hold nonzero vesting balances of the dividend asset
   auto vesting_balances_begin =
      vesting_index.indices().get<by_asset_balance>().lower_bound(boost::make_tuple(dividend_holder_asset_obj.id));
   auto vesting_balances_end =
      vesting_index.indices().get<by_asset_balance>().upper_bound(boost::make_tuple(dividend_holder_asset_obj.id, share_type()));
   for (const vesting_balance_object& vesting_balance_obj : boost::make_iterator_range(vesting_balances_begin, vesting_balances_end))
   {
        vesting_amounts[vesting_balance_obj.owner] += vesting_balance_obj.balance.amount;
        //dlog("Vesting balance for account: ${owner}, amount: ${amount}",
        //     ("owner", vesting_balance_obj.owner(db).name)
        //     ("amount", vesting_balance_obj.balance.amount));
   }
#else
   // get only once a collection of accounts that hold nonzero vesting balances of the dividend asset
   const auto& vesting_balances = vesting_index.indices().get<by_id>();
   for (const vesting_balance_object& vesting_balance_obj : vesting_balances)
   {
        if (vesting_balance_obj.balance.asset_id == dividend_holder_asset_obj.id && vesting_balance_obj.balance.amount)
        {
            vesting_amounts[vesting_balance_obj.owner] += vesting_balance_obj.balance.amount;
            dlog("Vesting balance for account: ${owner}, amount: ${amount}",
                 ("owner", vesting_balance_obj.owner(db).name)
                 ("amount", vesting_balance_obj.balance.amount));
        }
   }
#endif

   auto current_distribution_account_balance_iter = current_distribution_account_balance_range.begin();
   auto previous_distribution_account_balance_iter = previous_distribution_account_balance_range.first;
   dlog("Current balances in distribution account: ${current}, Previous balances: ${previous}",
        ("current", (int64_t)std::distance(current_distribution_account_balance_range.begin(), current_distribution_account_balance_range.end()))
        ("previous", (int64_t)std::distance(previous_distribution_account_balance_range.first, previous_distribution_account_balance_range.second)));

   // when we pay out the dividends to the holders, we need to know the total balance of the dividend asset in all
   // accounts other than the distribution account (it would be silly to distribute dividends back to 
   // the distribution account)
   share_type total_balance_of_dividend_asset;
   for (const account_balance_object& holder_balance_object : boost::make_iterator_range(holder_balances_begin, holder_balances_end))
      if (holder_balance_object.owner != dividend_data.dividend_distribution_account)
      {
         total_balance_of_dividend_asset += holder_balance_object.balance;
         auto itr = vesting_amounts.find(holder_balance_object.owner);
         if (itr != vesting_amounts.end())
             total_balance_of_dividend_asset += itr->second;
      }
   // loop through all of the assets currently or previously held in the distribution account
   while (current_distribution_account_balance_iter != current_distribution_account_balance_range.end() ||
          previous_distribution_account_balance_iter != previous_distribution_account_balance_range.second)
   {
      try
      {
         // First, figure out how much the balance on this asset has changed since the last sharing out
         share_type current_balance;
         share_type previous_balance;
         asset_id_type payout_asset_type;

         if (previous_distribution_account_balance_iter == previous_distribution_account_balance_range.second || 
             current_distribution_account_balance_iter->second->asset_type < previous_distribution_account_balance_iter->dividend_payout_asset_type)
         {
            // there are no more previous balances or there is no previous balance for this particular asset type
            payout_asset_type = current_distribution_account_balance_iter->second->asset_type;
            current_balance = current_distribution_account_balance_iter->second->balance;
            idump((payout_asset_type)(current_balance));
         }
         else if (current_distribution_account_balance_iter == current_distribution_account_balance_range.end() || 
                  previous_distribution_account_balance_iter->dividend_payout_asset_type < current_distribution_account_balance_iter->second->asset_type)
         {
            // there are no more current balances or there is no current balance for this particular previous asset type
            payout_asset_type = previous_distribution_account_balance_iter->dividend_payout_asset_type;
            previous_balance = previous_distribution_account_balance_iter->balance_at_last_maintenance_interval;
            idump((payout_asset_type)(previous_balance));
         }
         else
         {
            // we have both a previous and a current balance for this asset type
            payout_asset_type = current_distribution_account_balance_iter->second->asset_type;
            current_balance = current_distribution_account_balance_iter->second->balance;
            previous_balance = previous_distribution_account_balance_iter->balance_at_last_maintenance_interval;
            idump((payout_asset_type)(current_balance)(previous_balance));
         }

         share_type delta_balance = current_balance - previous_balance;

         // Next, figure out if we want to share this out -- if the amount added to the distribution 
         // account since last payout is too small, we won't bother.

         share_type total_fee_per_asset_in_payout_asset;
         const asset_object* payout_asset_object = nullptr;
         if (payout_asset_type == asset_id_type())
         {
            payout_asset_object = &db.get_core_asset();
            total_fee_per_asset_in_payout_asset = total_fee_per_asset_in_core;
            dlog("Fee for distributing ${payout_asset_type}: ${fee}", 
                 ("payout_asset_type", asset_id_type()(db).symbol)
                 ("fee", asset(total_fee_per_asset_in_core, asset_id_type())));
         }
         else
         {
            // figure out what the total fee is in terms of the payout asset
            const asset_index& asset_object_index = db.get_index_type<asset_index>();
            auto payout_asset_object_iter = asset_object_index.indices().find(payout_asset_type);
            FC_ASSERT(payout_asset_object_iter != asset_object_index.indices().end());

            payout_asset_object = &*payout_asset_object_iter;
            asset total_fee_per_asset = asset(total_fee_per_asset_in_core, asset_id_type()) * payout_asset_object->options.core_exchange_rate;
            FC_ASSERT(total_fee_per_asset.asset_id == payout_asset_type);

            total_fee_per_asset_in_payout_asset = total_fee_per_asset.amount;
            dlog("Fee for distributing ${payout_asset_type}: ${fee}", 
                 ("payout_asset_type", payout_asset_type(db).symbol)("fee", total_fee_per_asset_in_payout_asset));
         }

         share_type minimum_shares_to_distribute;
         if (dividend_data.options.minimum_fee_percentage)
         {
            fc::uint128_t minimum_amount_to_distribute = total_fee_per_asset_in_payout_asset.value;
            minimum_amount_to_distribute *= 100 * GRAPHENE_1_PERCENT;
            minimum_amount_to_distribute /= dividend_data.options.minimum_fee_percentage;
            wdump((total_fee_per_asset_in_payout_asset)(dividend_data.options));
            minimum_shares_to_distribute = minimum_amount_to_distribute.to_uint64();
         }
         
         dlog("Processing dividend payments of asset type ${payout_asset_type}, delta balance is ${delta_balance}", ("payout_asset_type", payout_asset_type(db).symbol)("delta_balance", delta_balance));
         if (delta_balance > 0)
         {
            if (delta_balance >= minimum_shares_to_distribute)
            {
               // first, pay the fee for scheduling these dividend  payments
               if (payout_asset_type == asset_id_type())
               {
                  // pay fee to network
                  db.modify(asset_dynamic_data_id_type()(db), [total_fee_per_asset_in_core](asset_dynamic_data_object& d) {
                     d.accumulated_fees += total_fee_per_asset_in_core;
                  });
                  db.adjust_balance(dividend_data.dividend_distribution_account, 
                                    asset(-total_fee_per_asset_in_core, asset_id_type()));
                  delta_balance -= total_fee_per_asset_in_core;
               }
               else
               {
                  const asset_dynamic_data_object& dynamic_data = payout_asset_object->dynamic_data(db);
                  if (dynamic_data.fee_pool < total_fee_per_asset_in_core)
                     FC_THROW("Not distributing dividends for ${holder_asset_type} in asset ${payout_asset_type} "
                              "because insufficient funds in fee pool (need: ${need}, have: ${have})",
                              ("holder_asset_type", dividend_holder_asset_obj.symbol)
                              ("payout_asset_type", payout_asset_object->symbol)
                              ("need", asset(total_fee_per_asset_in_core, asset_id_type()))
                              ("have", asset(dynamic_data.fee_pool, payout_asset_type)));
                  // deduct the fee from the dividend distribution account
                  db.adjust_balance(dividend_data.dividend_distribution_account, 
                                    asset(-total_fee_per_asset_in_payout_asset, payout_asset_type));
                  // convert it to core
                  db.modify(payout_asset_object->dynamic_data(db), [total_fee_per_asset_in_core, total_fee_per_asset_in_payout_asset](asset_dynamic_data_object& d) {
                     d.fee_pool -= total_fee_per_asset_in_core;
                     d.accumulated_fees += total_fee_per_asset_in_payout_asset;
                  });
                  // and pay it to the network
                  db.modify(asset_dynamic_data_id_type()(db), [total_fee_per_asset_in_core](asset_dynamic_data_object& d) {
                     d.accumulated_fees += total_fee_per_asset_in_core;
                  });
                  delta_balance -= total_fee_per_asset_in_payout_asset;
               }

               dlog("There are ${count} holders of the dividend-paying asset, with a total balance of ${total}", 
                    ("count", holder_account_count)
                    ("total", total_balance_of_dividend_asset));
               share_type remaining_amount_to_distribute = delta_balance;

               // credit each account with their portion, don't send any back to the dividend distribution account
               for (const account_balance_object& holder_balance_object : boost::make_iterator_range(holder_balances_begin, holder_balances_end))
               {
                  if (holder_balance_object.owner == dividend_data.dividend_distribution_account) continue;

                  auto holder_balance = holder_balance_object.balance;

                  auto itr = vesting_amounts.find(holder_balance_object.owner);
                  if (itr != vesting_amounts.end())
                      holder_balance += itr->second;

                  fc::uint128_t amount_to_credit(delta_balance.value);
                  amount_to_credit *= holder_balance.value;
                  amount_to_credit /= total_balance_of_dividend_asset.value;
                  share_type shares_to_credit((int64_t)amount_to_credit.to_uint64());
                  if (shares_to_credit.value)
                  {
                     wdump((delta_balance.value)(holder_balance)(total_balance_of_dividend_asset));

                     remaining_amount_to_distribute -= shares_to_credit;

                     dlog("Crediting account ${account} with ${amount}", 
                          ("account", holder_balance_object.owner(db).name)
                          ("amount", asset(shares_to_credit, payout_asset_type)));
                     auto pending_payout_iter = 
                        pending_payout_balance_index.indices().get<by_dividend_payout_account>().find(boost::make_tuple(dividend_holder_asset_obj.id, payout_asset_type, holder_balance_object.owner));
                     if (pending_payout_iter == pending_payout_balance_index.indices().get<by_dividend_payout_account>().end())
                        db.create<pending_dividend_payout_balance_for_holder_object>( [&]( pending_dividend_payout_balance_for_holder_object& obj ){
                           obj.owner = holder_balance_object.owner;
                           obj.dividend_holder_asset_type = dividend_holder_asset_obj.id;
                           obj.dividend_payout_asset_type = payout_asset_type;
                           obj.pending_balance = shares_to_credit;
                        });
                     else
                        db.modify(*pending_payout_iter, [&]( pending_dividend_payout_balance_for_holder_object& pending_balance ){
                           pending_balance.pending_balance += shares_to_credit;
                        });
                  }
               }

               for (const auto& pending_payout : pending_payout_balance_index.indices())
                  if (pending_payout.pending_balance.value)
                      dlog("Pending payout: ${account_name}   ->   ${amount}",
                           ("account_name", pending_payout.owner(db).name)
                           ("amount", asset(pending_payout.pending_balance, pending_payout.dividend_payout_asset_type)));
               dlog("Remaining balance not paid out: ${amount}", 
                    ("amount", asset(remaining_amount_to_distribute, payout_asset_type)));

               share_type distributed_amount = delta_balance - remaining_amount_to_distribute;
               if (previous_distribution_account_balance_iter == previous_distribution_account_balance_range.second ||
                   previous_distribution_account_balance_iter->dividend_payout_asset_type != payout_asset_type)
                  db.create<total_distributed_dividend_balance_object>( [&]( total_distributed_dividend_balance_object& obj ){
                     obj.dividend_holder_asset_type = dividend_holder_asset_obj.id;
                     obj.dividend_payout_asset_type = payout_asset_type;
                     obj.balance_at_last_maintenance_interval = distributed_amount;
                  });
               else
                  db.modify(*previous_distribution_account_balance_iter, [&]( total_distributed_dividend_balance_object& obj ){
                     obj.balance_at_last_maintenance_interval += distributed_amount;
                  });
            }
            else
               FC_THROW("Not distributing dividends for ${holder_asset_type} in asset ${payout_asset_type} "
                        "because amount ${delta_balance} is too small an amount to distribute.",
                        ("holder_asset_type", dividend_holder_asset_obj.symbol)
                        ("payout_asset_type", payout_asset_object->symbol)
                        ("delta_balance", asset(delta_balance, payout_asset_type)));
         }
         else if (delta_balance < 0)
         {
            // some amount of the asset has been withdrawn from the dividend_distribution_account,
            // meaning the current pending payout balances will add up to more than our current balance.
            // This should be extremely rare (caused by an override transfer by the asset owner).
            // Reduce all pending payouts proportionally
            share_type total_pending_balances;
            auto pending_payouts_range = 
               pending_payout_balance_index.indices().get<by_dividend_payout_account>().equal_range(boost::make_tuple(dividend_holder_asset_obj.id, payout_asset_type));

            for (const pending_dividend_payout_balance_for_holder_object& pending_balance_object : boost::make_iterator_range(pending_payouts_range.first, pending_payouts_range.second))
               total_pending_balances += pending_balance_object.pending_balance;

            share_type remaining_amount_to_recover = -delta_balance;
            share_type remaining_pending_balances = total_pending_balances;
            for (const pending_dividend_payout_balance_for_holder_object& pending_balance_object : boost::make_iterator_range(pending_payouts_range.first, pending_payouts_range.second))
            {
               fc::uint128_t amount_to_debit(remaining_amount_to_recover.value);
               amount_to_debit *= pending_balance_object.pending_balance.value;
               amount_to_debit /= remaining_pending_balances.value;
               share_type shares_to_debit((int64_t)amount_to_debit.to_uint64());

               remaining_amount_to_recover -= shares_to_debit;
               remaining_pending_balances -= pending_balance_object.pending_balance;

               db.modify(pending_balance_object, [&]( pending_dividend_payout_balance_for_holder_object& pending_balance ){
                  pending_balance.pending_balance -= shares_to_debit;
               });
            }

            // if we're here, we know there must be a previous balance, so just adjust it by the
            // amount we just reclaimed
            db.modify(*previous_distribution_account_balance_iter, [&]( total_distributed_dividend_balance_object& obj ){
               obj.balance_at_last_maintenance_interval += delta_balance;
               assert(obj.balance_at_last_maintenance_interval == current_balance);
            });
         } // end if deposit was large enough to distribute
      }
      catch (const fc::exception& e)
      {
         dlog("${e}", ("e", e));
      }

      // iterate
      if (previous_distribution_account_balance_iter == previous_distribution_account_balance_range.second || 
          current_distribution_account_balance_iter->second->asset_type < previous_distribution_account_balance_iter->dividend_payout_asset_type)
         ++current_distribution_account_balance_iter;
      else if (current_distribution_account_balance_iter == current_distribution_account_balance_range.end() || 
               previous_distribution_account_balance_iter->dividend_payout_asset_type < current_distribution_account_balance_iter->second->asset_type)
         ++previous_distribution_account_balance_iter;
      else
      {
         ++current_distribution_account_balance_iter;
         ++previous_distribution_account_balance_iter;
      }
   }
   db.modify(dividend_data, [current_head_block_time](asset_dividend_data_object& dividend_data_obj) {
      dividend_data_obj.last_scheduled_distribution_time = current_head_block_time;
      dividend_data_obj.last_distribution_time = current_head_block_time;
      });

} FC_CAPTURE_AND_RETHROW() }

void process_dividend_assets(database& db)
{ try {
   ilog("In process_dividend_assets time ${time}", ("time", db.head_block_time()));

   const account_balance_index& balance_index = db.get_index_type<account_balance_index>();
   //const auto& balance_index = db.get_index_type< primary_index< account_balance_index > >().get_secondary_index< balances_by_account_index >();
   const vesting_balance_index& vbalance_index = db.get_index_type<vesting_balance_index>();
   const total_distributed_dividend_balance_object_index& distributed_dividend_balance_index = db.get_index_type<total_distributed_dividend_balance_object_index>();
   const pending_dividend_payout_balance_for_holder_object_index& pending_payout_balance_index = db.get_index_type<pending_dividend_payout_balance_for_holder_object_index>();

   // TODO: switch to iterating over only dividend assets (generalize the by_type index)
   for( const asset_object& dividend_holder_asset_obj : db.get_index_type<asset_index>().indices() )
      if (dividend_holder_asset_obj.dividend_data_id)
      {
         const asset_dividend_data_object& dividend_data = dividend_holder_asset_obj.dividend_data(db);
         const account_object& dividend_distribution_account_object = dividend_data.dividend_distribution_account(db);

         fc::time_point_sec current_head_block_time = db.head_block_time();

         schedule_pending_dividend_balances(db, dividend_holder_asset_obj, dividend_data, current_head_block_time,
                                            balance_index, vbalance_index, distributed_dividend_balance_index, pending_payout_balance_index);
         if (dividend_data.options.next_payout_time &&
             db.head_block_time() >= *dividend_data.options.next_payout_time)
         {
            try
            {
               dlog("Dividend payout time has arrived for asset ${holder_asset}", 
                    ("holder_asset", dividend_holder_asset_obj.symbol));
#ifndef NDEBUG
               // dump balances before the payouts for debugging
               const auto& balance_index = db.get_index_type< primary_index< account_balance_index > >();
               const auto& balances = balance_index.get_secondary_index< balances_by_account_index >().get_account_balances( dividend_data.dividend_distribution_account );
               for( const auto balance : balances )
                  ilog("  Current balance: ${asset}", ("asset", asset(balance.second->balance, balance.second->asset_type)));
#endif

               // when we do the payouts, we first increase the balances in all of the receiving accounts
               // and use this map to keep track of the total amount of each asset paid out.
               // Afterwards, we decrease the distribution account's balance by the total amount paid out, 
               // and modify the distributed_balances accordingly
               std::map<asset_id_type, share_type> amounts_paid_out_by_asset;

               auto pending_payouts_range = 
                  pending_payout_balance_index.indices().get<by_dividend_account_payout>().equal_range(boost::make_tuple(dividend_holder_asset_obj.id));
               // the pending_payouts_range is all payouts for this dividend asset, sorted by the holder's account
               // we iterate in this order so we can build up a list of payouts for each account to put in the 
               // virtual op
               vector<asset> payouts_for_this_holder;
               fc::optional<account_id_type> last_holder_account_id;

               // cache the assets the distribution account is approved to send, we will be asking
               // for these often
               flat_map<asset_id_type, bool> approved_assets; // assets that the dividend distribution account is authorized to send/receive
               auto is_asset_approved_for_distribution_account = [&](const asset_id_type& asset_id) {
                  auto approved_assets_iter = approved_assets.find(asset_id);
                  if (approved_assets_iter != approved_assets.end())
                     return approved_assets_iter->second;
                  bool is_approved = is_authorized_asset(db, dividend_distribution_account_object, 
                                                         asset_id(db));
                  approved_assets[asset_id] = is_approved;
                  return is_approved;
               };

               for (auto pending_balance_object_iter = pending_payouts_range.first; pending_balance_object_iter != pending_payouts_range.second; )
               {
                  const pending_dividend_payout_balance_for_holder_object& pending_balance_object = *pending_balance_object_iter;

                  if (last_holder_account_id && *last_holder_account_id != pending_balance_object.owner && payouts_for_this_holder.size())
                  {
                     // we've moved on to a new account, generate the dividend payment virtual op for the previous one
                     db.push_applied_operation(asset_dividend_distribution_operation(dividend_holder_asset_obj.id, 
                                                                                     *last_holder_account_id, 
                                                                                     payouts_for_this_holder));
                     dlog("Just pushed virtual op for payout to ${account}", ("account", (*last_holder_account_id)(db).name));
                     payouts_for_this_holder.clear();
                     last_holder_account_id.reset();
                  }


                  if (pending_balance_object.pending_balance.value &&
                      is_authorized_asset(db, pending_balance_object.owner(db), pending_balance_object.dividend_payout_asset_type(db)) &&
                      is_asset_approved_for_distribution_account(pending_balance_object.dividend_payout_asset_type))
                  {
                     dlog("Processing payout of ${asset} to account ${account}", 
                          ("asset", asset(pending_balance_object.pending_balance, pending_balance_object.dividend_payout_asset_type))
                          ("account", pending_balance_object.owner(db).name));

                     db.adjust_balance(pending_balance_object.owner,
                                       asset(pending_balance_object.pending_balance, 
                                             pending_balance_object.dividend_payout_asset_type));
                     payouts_for_this_holder.push_back(asset(pending_balance_object.pending_balance, 
                                                             pending_balance_object.dividend_payout_asset_type));
                     last_holder_account_id = pending_balance_object.owner;
                     amounts_paid_out_by_asset[pending_balance_object.dividend_payout_asset_type] += pending_balance_object.pending_balance;

                     db.modify(pending_balance_object, [&]( pending_dividend_payout_balance_for_holder_object& pending_balance ){
                        pending_balance.pending_balance = 0;
                     });
                  }

                  ++pending_balance_object_iter;
               }
               // we will always be left with the last holder's data, generate the virtual op for it now.
               if (last_holder_account_id && payouts_for_this_holder.size())
               {
                  // we've moved on to a new account, generate the dividend payment virtual op for the previous one
                  db.push_applied_operation(asset_dividend_distribution_operation(dividend_holder_asset_obj.id, 
                                                                                  *last_holder_account_id, 
                                                                                  payouts_for_this_holder));
                  dlog("Just pushed virtual op for payout to ${account}", ("account", (*last_holder_account_id)(db).name));
               }

               // now debit the total amount of dividends paid out from the distribution account
               // and reduce the distributed_balances accordingly

               for (const auto& value : amounts_paid_out_by_asset)
               {
                  const asset_id_type& asset_paid_out = value.first;
                  const share_type& amount_paid_out = value.second;

                  db.adjust_balance(dividend_data.dividend_distribution_account, 
                                    asset(-amount_paid_out,
                                          asset_paid_out));
                  auto distributed_balance_iter = 
                     distributed_dividend_balance_index.indices().get<by_dividend_payout_asset>().find(boost::make_tuple(dividend_holder_asset_obj.id, 
                                                                                                                         asset_paid_out));
                  assert(distributed_balance_iter != distributed_dividend_balance_index.indices().get<by_dividend_payout_asset>().end());
                  if (distributed_balance_iter != distributed_dividend_balance_index.indices().get<by_dividend_payout_asset>().end())
                     db.modify(*distributed_balance_iter, [&]( total_distributed_dividend_balance_object& obj ){
                        obj.balance_at_last_maintenance_interval -= amount_paid_out; // now they've been paid out, reset to zero
                     });

               }

               // now schedule the next payout time
               db.modify(dividend_data, [current_head_block_time](asset_dividend_data_object& dividend_data_obj) {
                  dividend_data_obj.last_scheduled_payout_time = dividend_data_obj.options.next_payout_time;
                  dividend_data_obj.last_payout_time = current_head_block_time;
                  fc::optional<fc::time_point_sec> next_payout_time;
                  if (dividend_data_obj.options.payout_interval)
                  {
                     // if there was a previous payout, make our next payment one interval 
                     uint32_t current_time_sec = current_head_block_time.sec_since_epoch();
                     fc::time_point_sec reference_time = *dividend_data_obj.last_scheduled_payout_time;
                     uint32_t next_possible_time_sec = dividend_data_obj.last_scheduled_payout_time->sec_since_epoch();
                     do
                        next_possible_time_sec += *dividend_data_obj.options.payout_interval;
                     while (next_possible_time_sec <= current_time_sec);

                     next_payout_time = next_possible_time_sec;
                  }
                  dividend_data_obj.options.next_payout_time = next_payout_time;
                  idump((dividend_data_obj.last_scheduled_payout_time)
                        (dividend_data_obj.last_payout_time)
                        (dividend_data_obj.options.next_payout_time));
               });
            } 
            FC_RETHROW_EXCEPTIONS(error, "Error while paying out dividends for holder asset ${holder_asset}", ("holder_asset", dividend_holder_asset_obj.symbol))
         }
      }
} FC_CAPTURE_AND_RETHROW() }

void database::perform_chain_maintenance(const signed_block& next_block, const global_property_object& global_props)
{ try {
   const auto& gpo = get_global_properties();

   distribute_fba_balances(*this);
   create_buyback_orders(*this);

   process_dividend_assets(*this);

   struct vote_tally_helper {
      database& d;
      const global_property_object& props;
      std::map<account_id_type, share_type> vesting_amounts;

      vote_tally_helper(database& d, const global_property_object& gpo)
         : d(d), props(gpo)
      {
         d._vote_tally_buffer.resize(props.next_available_vote_id);
         d._witness_count_histogram_buffer.resize(props.parameters.maximum_witness_count / 2 + 1);
         d._committee_count_histogram_buffer.resize(props.parameters.maximum_committee_count / 2 + 1);
         d._son_count_histogram_buffer.resize(props.parameters.maximum_son_count / 2 + 1);
         d._total_voting_stake = 0;

         const vesting_balance_index& vesting_index = d.get_index_type<vesting_balance_index>();
#ifdef USE_VESTING_OBJECT_BY_ASSET_BALANCE_INDEX
         auto vesting_balances_begin =
              vesting_index.indices().get<by_asset_balance>().lower_bound(boost::make_tuple(asset_id_type()));
         auto vesting_balances_end =
              vesting_index.indices().get<by_asset_balance>().upper_bound(boost::make_tuple(asset_id_type(), share_type()));
         for (const vesting_balance_object& vesting_balance_obj : boost::make_iterator_range(vesting_balances_begin, vesting_balances_end))
         {
            vesting_amounts[vesting_balance_obj.owner] += vesting_balance_obj.balance.amount;
            //dlog("Vesting balance for account: ${owner}, amount: ${amount}",
            //     ("owner", vesting_balance_obj.owner(d).name)
            //     ("amount", vesting_balance_obj.balance.amount));
         }
#else
         const auto& vesting_balances = vesting_index.indices().get<by_id>();
         for (const vesting_balance_object& vesting_balance_obj : vesting_balances)
         {
            if (vesting_balance_obj.balance.asset_id == asset_id_type() && vesting_balance_obj.balance.amount)
            {
                vesting_amounts[vesting_balance_obj.owner] += vesting_balance_obj.balance.amount;
                dlog("Vesting balance for account: ${owner}, amount: ${amount}",
                     ("owner", vesting_balance_obj.owner(d).name)
                     ("amount", vesting_balance_obj.balance.amount));
            }
         }
#endif
      }

      void operator()(const account_object& stake_account) {
         if( props.parameters.count_non_member_votes || stake_account.is_member(d.head_block_time()) )
         {
            // There may be a difference between the account whose stake is voting and the one specifying opinions.
            // Usually they're the same, but if the stake account has specified a voting_account, that account is the one
            // specifying the opinions.
            const account_object* opinion_account_ptr =
                  (stake_account.options.voting_account ==
                   GRAPHENE_PROXY_TO_SELF_ACCOUNT)? &stake_account
                                     : d.find(stake_account.options.voting_account);

            if( !opinion_account_ptr ) // skip non-exist account
               return;

            const account_object& opinion_account = *opinion_account_ptr;

            const auto& stats = stake_account.statistics(d);
            uint64_t voting_stake = stats.total_core_in_orders.value
                  + (stake_account.cashback_vb.valid() ? (*stake_account.cashback_vb)(d).balance.amount.value: 0)
                  + d.get_balance(stake_account.get_id(), asset_id_type()).amount.value;

            auto itr = vesting_amounts.find(stake_account.id);
            if (itr != vesting_amounts.end())
                voting_stake += itr->second.value;
            for( vote_id_type id : opinion_account.options.votes )
            {
               uint32_t offset = id.instance();
               // if they somehow managed to specify an illegal offset, ignore it.
               if( offset < d._vote_tally_buffer.size() )
                  d._vote_tally_buffer[offset] += voting_stake;
            }

            if( opinion_account.options.num_witness <= props.parameters.maximum_witness_count )
            {
               uint16_t offset = std::min(size_t(opinion_account.options.num_witness/2),
                                          d._witness_count_histogram_buffer.size() - 1);
               // votes for a number greater than maximum_witness_count
               // are turned into votes for maximum_witness_count.
               //
               // in particular, this takes care of the case where a
               // member was voting for a high number, then the
               // parameter was lowered.
               d._witness_count_histogram_buffer[offset] += voting_stake;
            }
            if( opinion_account.options.num_committee <= props.parameters.maximum_committee_count )
            {
               uint16_t offset = std::min(size_t(opinion_account.options.num_committee/2),
                                          d._committee_count_histogram_buffer.size() - 1);
               // votes for a number greater than maximum_committee_count
               // are turned into votes for maximum_committee_count.
               //
               // same rationale as for witnesses
               d._committee_count_histogram_buffer[offset] += voting_stake;
            }
            if( opinion_account.options.num_son <= props.parameters.maximum_son_count )
            {
               uint16_t offset = std::min(size_t(opinion_account.options.num_son/2),
                                          d._son_count_histogram_buffer.size() - 1);
               // votes for a number greater than maximum_son_count
               // are turned into votes for maximum_son_count.
               //
               // in particular, this takes care of the case where a
               // member was voting for a high number, then the
               // parameter was lowered.
               d._son_count_histogram_buffer[offset] += voting_stake;
            }

            d._total_voting_stake += voting_stake;
         }
      }
   } tally_helper(*this, gpo);
   struct process_fees_helper {
      database& d;
      const global_property_object& props;

      process_fees_helper(database& d, const global_property_object& gpo)
         : d(d), props(gpo) {}

      void operator()(const account_object& a) {
         a.statistics(d).process_fees(a, d);
      }
   } fee_helper(*this, gpo);

   perform_account_maintenance(std::tie(
      tally_helper,
      fee_helper
      ));

   struct clear_canary {
      clear_canary(vector<uint64_t>& target): target(target){}
      ~clear_canary() { target.clear(); }
   private:
      vector<uint64_t>& target;
   };
   clear_canary a(_witness_count_histogram_buffer),
                b(_committee_count_histogram_buffer),
                d(_son_count_histogram_buffer),
                c(_vote_tally_buffer);

   update_top_n_authorities(*this);
   update_active_witnesses();
   update_active_committee_members();
   update_active_sons();
   update_worker_votes();

   modify(gpo, [this](global_property_object& p) {
      // Remove scaling of account registration fee
      const auto& dgpo = get_dynamic_global_properties();
      p.parameters.current_fees->get<account_create_operation>().basic_fee >>= p.parameters.account_fee_scale_bitshifts *
            (dgpo.accounts_registered_this_interval / p.parameters.accounts_per_fee_scale);

      if( p.pending_parameters )
      {
         if( !p.pending_parameters->extensions.value.min_bet_multiplier.valid() )
            p.pending_parameters->extensions.value.min_bet_multiplier = p.parameters.extensions.value.min_bet_multiplier;
         if( !p.pending_parameters->extensions.value.max_bet_multiplier.valid() )
            p.pending_parameters->extensions.value.max_bet_multiplier = p.parameters.extensions.value.max_bet_multiplier;
         if( !p.pending_parameters->extensions.value.betting_rake_fee_percentage.valid() )
            p.pending_parameters->extensions.value.betting_rake_fee_percentage = p.parameters.extensions.value.betting_rake_fee_percentage;
         if( !p.pending_parameters->extensions.value.permitted_betting_odds_increments.valid() )
            p.pending_parameters->extensions.value.permitted_betting_odds_increments = p.parameters.extensions.value.permitted_betting_odds_increments;
         if( !p.pending_parameters->extensions.value.live_betting_delay_time.valid() )
            p.pending_parameters->extensions.value.live_betting_delay_time = p.parameters.extensions.value.live_betting_delay_time;
         p.parameters = std::move(*p.pending_parameters);
         p.pending_parameters.reset();
      }
   });

   auto next_maintenance_time = get<dynamic_global_property_object>(dynamic_global_property_id_type()).next_maintenance_time;
   auto maintenance_interval = gpo.parameters.maintenance_interval;

   if( next_maintenance_time <= next_block.timestamp )
   {
      if( next_block.block_num() == 1 )
         next_maintenance_time = time_point_sec() +
               (((next_block.timestamp.sec_since_epoch() / maintenance_interval) + 1) * maintenance_interval);
      else
      {
         // We want to find the smallest k such that next_maintenance_time + k * maintenance_interval > head_block_time()
         //  This implies k > ( head_block_time() - next_maintenance_time ) / maintenance_interval
         //
         // Let y be the right-hand side of this inequality, i.e.
         // y = ( head_block_time() - next_maintenance_time ) / maintenance_interval
         //
         // and let the fractional part f be y-floor(y).  Clearly 0 <= f < 1.
         // We can rewrite f = y-floor(y) as floor(y) = y-f.
         //
         // Clearly k = floor(y)+1 has k > y as desired.  Now we must
         // show that this is the least such k, i.e. k-1 <= y.
         //
         // But k-1 = floor(y)+1-1 = floor(y) = y-f <= y.
         // So this k suffices.
         //
         auto y = (head_block_time() - next_maintenance_time).to_seconds() / maintenance_interval;
         next_maintenance_time += (y+1) * maintenance_interval;
      }
   }

   const dynamic_global_property_object& dgpo = get_dynamic_global_properties();

   if( (dgpo.next_maintenance_time < HARDFORK_613_TIME) && (next_maintenance_time >= HARDFORK_613_TIME) )
      deprecate_annual_members(*this);

   modify(dgpo, [next_maintenance_time](dynamic_global_property_object& d) {
      d.next_maintenance_time = next_maintenance_time;
      d.accounts_registered_this_interval = 0;
   });

   // Reset all BitAsset force settlement volumes to zero
   //for( const asset_bitasset_data_object* d : get_index_type<asset_bitasset_data_index>() )
   for( const auto& d : get_index_type<asset_bitasset_data_index>().indices() )
      modify( d, [](asset_bitasset_data_object& o) { o.force_settled_volume = 0; });

   // process_budget needs to run at the bottom because
   //   it needs to know the next_maintenance_time
   process_budget();
} FC_CAPTURE_AND_RETHROW() }

} }
