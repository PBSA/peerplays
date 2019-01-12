#pragma once

#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <sidechain/types.hpp>

using namespace sidechain;

namespace graphene { namespace chain {

class withdraw_pbtc_evaluator : public evaluator<withdraw_pbtc_evaluator>
{
public:
   typedef withdraw_pbtc_operation operation_type;

   void_result do_evaluate(const withdraw_pbtc_operation& op);

   object_id_type do_apply(const withdraw_pbtc_operation& op);

   void reserve_issue( const withdraw_pbtc_operation& op );

   bool check_amount(  const withdraw_pbtc_operation& op  );

   payment_type type;
};

} } // graphene::chain
