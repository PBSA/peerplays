#pragma once
#include <graphene/chain/protocol/base.hpp>

namespace graphene { namespace chain { 

   struct bitcoin_address_create_operation : public base_operation
   {
      struct fee_parameters_type { 
         uint64_t fee            = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
      };

      asset             fee;
      account_id_type   payer;
      account_id_type   owner;

      account_id_type fee_payer()const { return payer; }
      void            validate()const {}
      share_type      calculate_fee( const fee_parameters_type& k )const {
         return k.fee;
      }
   };

} } // graphene::chain

FC_REFLECT( graphene::chain::bitcoin_address_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::bitcoin_address_create_operation, (fee)(payer)(owner) )
