#pragma once

#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/exceptions.hpp>

namespace graphene { namespace chain {

struct bitcoin_issue_evaluator : public evaluator< bitcoin_issue_evaluator >
{
   typedef bitcoin_issue_operation operation_type;
   
   void_result do_evaluate( const bitcoin_issue_operation& op );

   void_result do_apply( const bitcoin_issue_operation& op );

   void add_issue( const bitcoin_transaction_object& btc_obj );

   void clear_btc_transaction_information( const bitcoin_transaction_object& btc_obj );

   std::vector<uint64_t> get_amounts_to_issue( std::vector<fc::sha256> vins_identifier );

   std::vector<account_id_type> get_accounts_to_issue( std::vector<fc::sha256> vins_identifier );
};

} } // graphene::chain