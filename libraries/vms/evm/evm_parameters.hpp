#pragma once
#include <graphene/chain/protocol/types.hpp>


namespace vms { namespace evm {

   struct evm_parameters_extension {
      uint64_t minimum_gas_price   = EVM_DEFAULT_MIN_GAS_PRICE;
      uint64_t block_gas_limit = BLOCK_GAS_LIMIT_EVM;
   };

} }

FC_REFLECT( vms::evm::evm_parameters_extension, 
            (minimum_gas_price)
            (block_gas_limit)
)
