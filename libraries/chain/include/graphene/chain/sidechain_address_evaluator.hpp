#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/protocol/sidechain_address.hpp>

namespace graphene { namespace chain {

class add_sidechain_address_evaluator : public evaluator<add_sidechain_address_evaluator>
{
public:
    typedef sidechain_address_add_operation operation_type;

    void_result do_evaluate(const sidechain_address_add_operation& o);
    object_id_type do_apply(const sidechain_address_add_operation& o);
};

class update_sidechain_address_evaluator : public evaluator<update_sidechain_address_evaluator>
{
public:
    typedef sidechain_address_update_operation operation_type;

    void_result do_evaluate(const sidechain_address_update_operation& o);
    object_id_type do_apply(const sidechain_address_update_operation& o);
};

class delete_sidechain_address_evaluator : public evaluator<delete_sidechain_address_evaluator>
{
public:
    typedef sidechain_address_delete_operation operation_type;

    void_result do_evaluate(const sidechain_address_delete_operation& o);
    void_result do_apply(const sidechain_address_delete_operation& o);
};

} } // namespace graphene::chain
