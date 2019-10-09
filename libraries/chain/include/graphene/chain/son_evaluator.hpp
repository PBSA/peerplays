#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/protocol/son.hpp>

namespace graphene { namespace chain {

class create_son_evaluator : public evaluator<create_son_evaluator>
{
public:
    typedef son_create_operation operation_type;

    void_result do_evaluate(const son_create_operation& o);
    object_id_type do_apply(const son_create_operation& o);
};

class update_son_evaluator : public evaluator<update_son_evaluator>
{
public:
    typedef son_update_operation operation_type;

    void_result do_evaluate(const son_update_operation& o);
    object_id_type do_apply(const son_update_operation& o);
};

class delete_son_evaluator : public evaluator<delete_son_evaluator>
{
public:
    typedef son_delete_operation operation_type;

    void_result do_evaluate(const son_delete_operation& o);
    void_result do_apply(const son_delete_operation& o);
};

} } // namespace graphene::chain
