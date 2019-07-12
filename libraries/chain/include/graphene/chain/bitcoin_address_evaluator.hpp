#pragma once

#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/exceptions.hpp>

namespace graphene { namespace chain {

class bitcoin_address_create_evaluator : public evaluator<bitcoin_address_create_evaluator>
{
public:
   typedef bitcoin_address_create_operation operation_type;

   void_result do_evaluate(const bitcoin_address_create_operation& op);

   object_id_type do_apply(const bitcoin_address_create_operation& op);
   
   public_key_type pubkey_from_id( object_id_type id );
};

} } // graphene::chain
