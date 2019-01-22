#pragma once

#include <graphene/chain/evaluator.hpp>

namespace graphene { namespace chain {

class bitcoin_transaction_send_evaluator : public evaluator<bitcoin_transaction_send_evaluator>
{
public:
   typedef bitcoin_transaction_send_operation operation_type;

   void_result do_evaluate( const bitcoin_transaction_send_operation& op );

   object_id_type do_apply( const bitcoin_transaction_send_operation& op );

   void send_bitcoin_transaction( const bitcoin_transaction_object& btc_tx );
};

} } // graphene::chain
