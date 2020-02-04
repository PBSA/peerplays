#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/protocol/son_wallet.hpp>

namespace graphene { namespace chain {

class recreate_son_wallet_evaluator : public evaluator<recreate_son_wallet_evaluator>
{
public:
    typedef son_wallet_recreate_operation operation_type;

    void_result do_evaluate(const son_wallet_recreate_operation& o);
    object_id_type do_apply(const son_wallet_recreate_operation& o);
};

class update_son_wallet_evaluator : public evaluator<update_son_wallet_evaluator>
{
public:
    typedef son_wallet_update_operation operation_type;

    void_result do_evaluate(const son_wallet_update_operation& o);
    object_id_type do_apply(const son_wallet_update_operation& o);
};

} } // namespace graphene::chain
