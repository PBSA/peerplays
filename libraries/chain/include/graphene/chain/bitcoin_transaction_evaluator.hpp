#pragma once

#include <graphene/chain/evaluator.hpp>

namespace sidechain { class info_for_vin; }

namespace graphene { namespace chain {

class bitcoin_transaction_send_evaluator : public evaluator<bitcoin_transaction_send_evaluator>
{
public:
   typedef bitcoin_transaction_send_operation operation_type;

   void_result do_evaluate( const bitcoin_transaction_send_operation& op );

   object_id_type do_apply( const bitcoin_transaction_send_operation& op );

   std::vector<fc::sha256> create_transaction_vins( bitcoin_transaction_send_operation& op );

   void finalize_bitcoin_transaction( bitcoin_transaction_send_operation& op );

   void send_bitcoin_transaction( const bitcoin_transaction_object& btc_tx );
};

class bitcoin_transaction_sign_evaluator : public evaluator<bitcoin_transaction_sign_evaluator>
{

public:

   typedef bitcoin_transaction_sign_operation operation_type;

   void_result do_evaluate( const bitcoin_transaction_sign_operation& op );

   void_result do_apply( const bitcoin_transaction_sign_operation& op );
   
   void update_proposal( const bitcoin_transaction_sign_operation& op );

   bool check_sigs( const sidechain::bytes& key_data, const std::vector<sidechain::bytes>& sigs,
                    const std::vector<sidechain::info_for_vin>& info_for_vins, const sidechain::bitcoin_transaction& tx );

};

class bitcoin_transaction_revert_evaluator : public evaluator<bitcoin_transaction_revert_evaluator>
{

public:

   typedef bitcoin_transaction_revert_operation operation_type;

   void_result do_evaluate( const bitcoin_transaction_revert_operation& op );

   void_result do_apply( const bitcoin_transaction_revert_operation& op );
};

} } // graphene::chain