#pragma once
#include <graphene/chain/protocol/base.hpp>

namespace graphene { namespace chain {

   struct bitcoin_issue_operation : public base_operation
   {
      struct fee_parameters_type { 
         uint64_t fee             = 0;
      };

      asset                          fee;
      account_id_type                payer;
      std::vector<fc::sha256>        transaction_ids;


      account_id_type fee_payer()const { return payer; }
      void            validate()const {}
      share_type      calculate_fee( const fee_parameters_type& k )const {
         share_type fee_required = k.fee;
         return fee_required;
      }
   };

} } // graphene::chain

FC_REFLECT( graphene::chain::bitcoin_issue_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::bitcoin_issue_operation, (fee)(payer)(transaction_ids) )
