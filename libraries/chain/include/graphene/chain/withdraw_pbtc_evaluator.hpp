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

   void_result do_evaluate( const withdraw_pbtc_operation& op );

   void_result do_apply( const withdraw_pbtc_operation& op );

   void reserve_issue( const withdraw_pbtc_operation& op );

   bool check_amount_higher_than_fee( const withdraw_pbtc_operation& op );

   payment_type type;
};

} } // graphene::chain
