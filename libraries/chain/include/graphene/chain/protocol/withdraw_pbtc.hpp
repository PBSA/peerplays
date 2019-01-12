#pragma once
#include <graphene/chain/protocol/base.hpp>

namespace graphene { namespace chain { 

   struct withdraw_pbtc_operation : public base_operation
   {
      struct fee_parameters_type { 
         uint64_t fee             = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = 10;
      };

      asset             fee;
      account_id_type   payer;
      
      std::string       data; // address or script
      uint64_t          amount;

      // object_id_type    tx_obj_id;

      account_id_type fee_payer() const { return payer; }
      void            validate() const {}
      share_type      calculate_fee( const fee_parameters_type& k )const {
         share_type fee_required = k.fee;
         fee_required += calculate_data_fee( fc::raw::pack_size(*this), k.price_per_kbyte );
         return fee_required;
      }
   };

} } // graphene::chain

FC_REFLECT( graphene::chain::withdraw_pbtc_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::withdraw_pbtc_operation, (fee)(payer)(data)(amount) )
