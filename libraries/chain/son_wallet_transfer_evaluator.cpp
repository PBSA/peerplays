#include <graphene/chain/son_wallet_transfer_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/son_wallet_transfer_object.hpp>

namespace graphene { namespace chain {

void_result create_son_wallet_transfer_evaluator::do_evaluate(const son_wallet_transfer_create_operation& op)
{ try{
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");
   //FC_ASSERT(db().get_global_properties().parameters.get_son_btc_account_id() != GRAPHENE_NULL_ACCOUNT, "SON paying account not set.");
   FC_ASSERT( op.payer == db().get_global_properties().parameters.get_son_btc_account_id(), "SON paying account must be set as payer." );

   //const auto& idx = db().get_index_type<son_wallet_transfer_index>().indices().get<by_sidechain_uid>();
   //FC_ASSERT(idx.find(op.sidechain_uid) == idx.end(), "Already registered " + op.sidechain_uid);
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type create_son_wallet_transfer_evaluator::do_apply(const son_wallet_transfer_create_operation& op)
{ try {
   const auto& idx = db().get_index_type<son_wallet_transfer_index>().indices().get<by_sidechain_uid>();
   auto itr = idx.find(op.sidechain_uid);
   if (itr == idx.end()) {
      const auto& new_son_wallet_transfer_object = db().create<son_wallet_transfer_object>( [&]( son_wallet_transfer_object& swto ){
         swto.timestamp = op.timestamp;
         swto.sidechain = op.sidechain;
         swto.confirmations = 1;
         swto.sidechain_uid = op.sidechain_uid;
         swto.sidechain_transaction_id = op.sidechain_transaction_id;
         swto.sidechain_from = op.sidechain_from;
         swto.sidechain_to = op.sidechain_to;
         swto.sidechain_amount = op.sidechain_amount;
         swto.peerplays_from = op.peerplays_from;
         swto.peerplays_to = op.peerplays_to;
         swto.peerplays_amount = op.peerplays_amount;
         swto.processed = false;
      });
      return new_son_wallet_transfer_object.id;
   } else {
      db().modify(*itr, [&op](son_wallet_transfer_object &swto) {
         swto.confirmations = swto.confirmations + 1;
      });
      return (*itr).id;
   }
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result process_son_wallet_transfer_evaluator::do_evaluate(const son_wallet_transfer_process_operation& op)
{ try{
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");
   //FC_ASSERT(db().get_global_properties().parameters.get_son_btc_account_id() != GRAPHENE_NULL_ACCOUNT, "SON paying account not set.");
   FC_ASSERT( op.payer == db().get_global_properties().parameters.get_son_btc_account_id(), "SON paying account must be set as payer." );

   const auto& idx = db().get_index_type<son_wallet_transfer_index>().indices().get<by_id>();
   const auto& itr = idx.find(op.son_wallet_transfer_id);
   FC_ASSERT(itr != idx.end(), "Son wallet transfer not found");
   //FC_ASSERT(itr->processed == false, "Son wallet transfer is already processed");

   const database& d = db();

   const account_object& from_account    = itr->peerplays_to(d); // reversed, for deposit
   const account_object& to_account      = itr->peerplays_from(d); // reversed, for deposit
   const asset_object&   asset_type      = itr->peerplays_amount.asset_id(d);

   try {

      GRAPHENE_ASSERT(
         is_authorized_asset( d, from_account, asset_type ),
         transfer_from_account_not_whitelisted,
         "'from' account ${from} is not whitelisted for asset ${asset}",
         ("from",from_account.id)
         ("asset",itr->peerplays_amount.asset_id)
         );
      GRAPHENE_ASSERT(
         is_authorized_asset( d, to_account, asset_type ),
         transfer_to_account_not_whitelisted,
         "'to' account ${to} is not whitelisted for asset ${asset}",
         ("to",to_account.id)
         ("asset",itr->peerplays_amount.asset_id)
         );

      if( asset_type.is_transfer_restricted() )
      {
         GRAPHENE_ASSERT(
            from_account.id == asset_type.issuer || to_account.id == asset_type.issuer,
            transfer_restricted_transfer_asset,
            "Asset {asset} has transfer_restricted flag enabled",
            ("asset", itr->peerplays_amount.asset_id)
          );
      }

      bool insufficient_balance = d.get_balance( from_account, asset_type ).amount >= itr->peerplays_amount.amount;
      FC_ASSERT( insufficient_balance,
                 "Insufficient Balance: ${balance}, unable to transfer '${total_transfer}' from account '${a}' to '${t}'",
                 ("a",from_account.name)("t",to_account.name)("total_transfer",d.to_pretty_string(itr->peerplays_amount))("balance",d.to_pretty_string(d.get_balance(from_account, asset_type))) );

      return void_result();
   } FC_RETHROW_EXCEPTIONS( error, "Unable to transfer ${a} from ${f} to ${t}", ("a",d.to_pretty_string(itr->peerplays_amount))("f",from_account.name)("t",to_account.name) );
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type process_son_wallet_transfer_evaluator::do_apply(const son_wallet_transfer_process_operation& op)
{ try {
   const auto& idx = db().get_index_type<son_wallet_transfer_index>().indices().get<by_id>();
   auto itr = idx.find(op.son_wallet_transfer_id);
   if(itr != idx.end())
   {
       if (itr->processed == false) {
          db().modify(*itr, [&op](son_wallet_transfer_object &swto) {
             swto.processed = true;
          });

          const account_id_type from_account    = itr->peerplays_to; // reversed, for deposit
          const account_id_type to_account      = itr->peerplays_from; // reversed, for deposit

          db().adjust_balance( from_account, -itr->peerplays_amount );
          db().adjust_balance( to_account, itr->peerplays_amount );
       }
   }
   return op.son_wallet_transfer_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // namespace graphene::chain
