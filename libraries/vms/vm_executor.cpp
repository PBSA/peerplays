#include <vm_executor.hpp>
#include <graphene/chain/protocol/contract.hpp>
#include <wavm.hpp>

namespace vms { namespace base {

void vm_executor::init( const std::string& path )
{
   vms::wavm::wavm_adapter adapter( db );
   registered_vms[vm_types::WAVM] = std::unique_ptr<vm_interface>( new vms::wavm::wavm( adapter ) );
}

std::pair<uint64_t, bytes> vm_executor::execute( const contract_operation& o)
{
   return registered_vms.at( o.vm_type )->exec( o.data );
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

std::map<uint64_t, bytes> vm_executor::get_contracts( const vm_types& vm_type, const std::vector<contract_id_type>& ids ) const
{
   std::vector<uint64_t> instances;
   for( const auto& id : ids ) {
      instances.push_back( static_cast<uint64_t>( id.instance ) );
   }
   return registered_vms.at( vm_type )->get_contracts( instances );
}

bytes vm_executor::get_code( const vm_types& vm_type, const uint64_t& id ) const
{
   return registered_vms.at( vm_type )->get_code( id );
}

} }
