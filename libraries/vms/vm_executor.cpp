#include "vm_executor.hpp"
#include <graphene/chain/protocol/contract.hpp>
#include <evm.hpp>

namespace vms { namespace base {

void vm_executor::init( const std::string& path )
{
   vms::evm::evm_adapter adapter( db );
   registered_vms[1] = std::unique_ptr<vm_interface>( new vms::evm::evm( path, adapter ) );
}

void vm_executor::execute( const contract_operation& o, const bool& commit )
{
   registered_vms.at( o.version_vm )->exec( o.data, commit );
}

} }
