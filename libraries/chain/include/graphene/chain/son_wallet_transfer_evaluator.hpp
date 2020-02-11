#pragma once
#include <graphene/chain/evaluator.hpp>

namespace graphene { namespace chain {

class create_son_wallet_transfer_evaluator : public evaluator<create_son_wallet_transfer_evaluator>
{
public:
    typedef son_wallet_transfer_create_operation operation_type;

    void_result do_evaluate(const son_wallet_transfer_create_operation& o);
    object_id_type do_apply(const son_wallet_transfer_create_operation& o);
};

class process_son_wallet_transfer_evaluator : public evaluator<process_son_wallet_transfer_evaluator>
{
public:
    typedef son_wallet_transfer_process_operation operation_type;

    void_result do_evaluate(const son_wallet_transfer_process_operation& o);
    object_id_type do_apply(const son_wallet_transfer_process_operation& o);
};

} } // namespace graphene::chain
