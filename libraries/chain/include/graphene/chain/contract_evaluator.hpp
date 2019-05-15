#pragma once

#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/protocol/contract.hpp>
#include <graphene/chain/contract_object.hpp>

namespace graphene { namespace chain {

    class contract_evaluator : public evaluator<contract_evaluator>
    {
        public:
        typedef contract_operation operation_type;

        void_result do_evaluate( const contract_operation& o );
        object_id_type do_apply( const contract_operation& o );
    };

} } // graphene::chain
