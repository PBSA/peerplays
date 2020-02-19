#include <graphene/chain/son_wallet_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/son_wallet_object.hpp>

namespace graphene { namespace chain {

void_result recreate_son_wallet_evaluator::do_evaluate(const son_wallet_recreate_operation& op)
{ try{
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");
   //FC_ASSERT(db().get_global_properties().parameters.get_son_btc_account_id() != GRAPHENE_NULL_ACCOUNT, "SON paying account not set.");
   FC_ASSERT( op.payer == db().get_global_properties().parameters.get_son_btc_account_id(), "SON paying account must be set as payer." );

   const auto& idx = db().get_index_type<son_wallet_index>().indices().get<by_id>();
   auto itr = idx.rbegin();
   if(itr != idx.rend())
   {
      // Compare current wallet SONs and to-be lists of active sons
      auto cur_wallet_sons = (*itr).sons;
      auto new_wallet_sons = op.sons;

      bool son_sets_equal = (cur_wallet_sons.size() == new_wallet_sons.size());
      if (son_sets_equal) {
         for( size_t i = 0; i < cur_wallet_sons.size(); i++ ) {
            son_sets_equal = son_sets_equal && cur_wallet_sons.at(i) == new_wallet_sons.at(i);
         }
      }

      FC_ASSERT(son_sets_equal == false, "Wallet recreation not needed, active SONs set is not changed.");
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type recreate_son_wallet_evaluator::do_apply(const son_wallet_recreate_operation& op)
{ try {
   const auto& idx = db().get_index_type<son_wallet_index>().indices().get<by_id>();
   auto itr = idx.rbegin();
   if(itr != idx.rend())
   {
       db().modify(*itr, [&, op](son_wallet_object &swo) {
           swo.expires = db().head_block_time();
       });
   }

   const auto& new_son_wallet_object = db().create<son_wallet_object>( [&]( son_wallet_object& obj ){
      obj.valid_from = db().head_block_time();
      obj.expires = time_point_sec::maximum();
      obj.sons = op.sons;
   });
   return new_son_wallet_object.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result update_son_wallet_evaluator::do_evaluate(const son_wallet_update_operation& op)
{ try{
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");
   //FC_ASSERT(db().get_global_properties().parameters.get_son_btc_account_id() != GRAPHENE_NULL_ACCOUNT, "SON paying account not set.");
   FC_ASSERT( op.payer == db().get_global_properties().parameters.get_son_btc_account_id(), "SON paying account must be set as payer." );

   const auto& idx = db().get_index_type<son_wallet_index>().indices().get<by_id>();
   FC_ASSERT( idx.find(op.son_wallet_id) != idx.end() );
   //auto itr = idx.find(op.son_wallet_id);
   //FC_ASSERT( itr->addresses.find(op.sidechain) == itr->addresses.end() ||
   //        itr->addresses.at(op.sidechain).empty(), "Sidechain wallet address already set");
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type update_son_wallet_evaluator::do_apply(const son_wallet_update_operation& op)
{ try {
   const auto& idx = db().get_index_type<son_wallet_index>().indices().get<by_id>();
   auto itr = idx.find(op.son_wallet_id);
   if (itr != idx.end())
   {
      if (itr->addresses.find(op.sidechain) == itr->addresses.end()) {
         db().modify(*itr, [&op](son_wallet_object &swo) {
            swo.addresses[op.sidechain] = op.address;
         });
      }
   }
   return op.son_wallet_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // namespace graphene::chain
