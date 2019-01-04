#include "vm_executor.hpp"
#include <graphene/chain/protocol/contract.hpp>
#include <evm.hpp>

namespace vms { namespace base {

void vm_executor::init( const std::string& path )
{
   vms::evm::evm_adapter adapter( db );
   registered_vms[1] = std::unique_ptr<vm_interface>( new vms::evm::evm( path, adapter ) );
}

std::pair<uint64_t, bytes> vm_executor::execute( const contract_operation& o, const bool& commit )
{
   return registered_vms.at( o.version_vm )->exec( o.data, commit );
}

std::vector< uint64_t > vm_executor::get_attracted_contracts( const uint64_t& version_vm ) const
{
   return registered_vms.at( version_vm )->get_attracted_contracts();
}

std::string vm_executor::get_state_root_evm() const
{
   return registered_vms.at( 1 )->get_state_root(); // 1 evm_type. Make constant.
}

void vm_executor::set_state_root_evm( const std::string& hash )
{
   registered_vms.at( 1 )->set_state_root( hash );
}

} }
