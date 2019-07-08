#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/btc-sidechain/son_operations.hpp>

namespace graphene { namespace chain {

class create_son_member_evaluator : public evaluator<create_son_member_evaluator>
{
public:
    typedef son_member_create_operation operation_type;

    void_result do_evaluate(const son_member_create_operation& o);
    object_id_type do_apply(const son_member_create_operation& o);
};

class delete_son_member_evaluator : public evaluator<delete_son_member_evaluator>
{
public:
    typedef son_member_delete_operation operation_type;

    void_result do_evaluate(const son_member_delete_operation& o);
    void_result do_apply(const son_member_delete_operation& o);
};

} } // namespace graphene::chain
