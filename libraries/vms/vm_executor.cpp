#include "vm_executor.hpp"
#include <graphene/chain/protocol/contract.hpp>
#include <evm.hpp>

namespace vms { namespace base {

void vm_executor::init( const std::string& path )
{
   vms::evm::evm_adapter adapter( db );
   registered_vms[vms::base::vm_types::EVM] = std::unique_ptr<vm_interface>( new vms::evm::evm( path, adapter ) );
}

std::pair<uint64_t, bytes> vm_executor::execute( const contract_operation& o, const bool& commit )
{
   return registered_vms.at( o.vm_type )->exec( o.data, commit );
}

std::vector< uint64_t > vm_executor::get_attracted_contracts( const vms::base::vm_types& vm_type ) const
{
   return registered_vms.at( vm_type )->get_attracted_contracts();
}

std::string vm_executor::get_state_root_evm() const
{
   return registered_vms.at( vms::base::vm_types::EVM )->get_state_root();
}

void vm_executor::set_state_root_evm( const std::string& hash )
{
   registered_vms.at( vms::base::vm_types::EVM )->set_state_root( hash );
}

} }
