#pragma once
#include <sidechain/types.hpp>
#include <sidechain/input_withdrawal_info.hpp>
#include <graphene/chain/protocol/bitcoin_transaction.hpp>
#include <graphene/chain/protocol/operations.hpp>

namespace graphene { namespace chain { class database; } };

namespace sidechain {

class sidechain_proposal_checker
{

public:

   sidechain_proposal_checker( graphene::chain::database& _db ) : db(_db) {}

   bool check_bitcoin_transaction_send_operation( const bitcoin_transaction_send_operation& op );

   bool check_reuse( const graphene::chain::operation& op );
   
   bool check_witness_opportunity_to_approve( const witness_object& current_witness, const proposal_object& proposal );

   bool check_witnesses_keys( const witness_object& current_witness, const proposal_object& proposal );

   bool check_bitcoin_transaction_revert_operation( const bitcoin_transaction_revert_operation& op );

private:

   bool check_info_for_pw_vin( const info_for_vin& info_for_vin );

   bool check_info_for_vins( const std::vector<info_for_vin>& info_for_vins );

   bool check_info_for_vouts( const std::vector<info_for_vout_id_type>& info_for_vout_ids );

   bool check_transaction( const bitcoin_transaction_send_operation& btc_trx_op );

   bool check_reuse_pw_vin( const fc::sha256& pw_vin );

   bool check_reuse_user_vins( const std::vector<fc::sha256>& user_vin_identifiers );

   bool check_reuse_vouts( const std::vector<info_for_vout_id_type>& user_vout_ids );

   std::set<fc::sha256> pw_vin_ident;
   std::set<fc::sha256> user_vin_ident;
   std::set<info_for_vout_id_type> vout_ids;

   graphene::chain::database& db;

};

}
