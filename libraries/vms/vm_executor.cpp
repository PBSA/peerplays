#include "vm_executor.hpp"
#include <graphene/chain/protocol/contract.hpp>
#include <evm.hpp>

namespace vms { namespace base {

void vm_executor::init( const std::string& path )
{
   vms::evm::evm_adapter adapter( db );
   registered_vms[vm_types::EVM] = std::unique_ptr<vm_interface>( new vms::evm::evm( path, adapter ) );
}

std::pair<uint64_t, bytes> vm_executor::execute( const contract_operation& o, const bool& commit )
{
   return registered_vms.at( o.vm_type )->exec( o.data, commit );
}

std::vector< uint64_t > vm_executor::get_attracted_contracts( const vm_types& vm_type ) const
{
   return registered_vms.at( vm_type )->get_attracted_contracts();
}

void vm_executor::roll_back_db( const uint32_t& block_number )
{
   for( const auto& vm : registered_vms ) {
      vm.second->roll_back_db( block_number );
   }
}

std::vector<bytes> vm_executor::get_contracts( const vm_types& vm_type ) const
{
   return registered_vms.at( vm_type )->get_contracts();
}

} }
