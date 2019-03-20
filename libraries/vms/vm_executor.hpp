#pragma once

#include <map>
#include <vm_interface.hpp>

namespace graphene { namespace chain { class contract_operation; struct database_fixture; } }

namespace vms { namespace base {

class vm_executor
{

public:

   vm_executor( database& _db ) : db( _db ) {}

   void init( const std::string& path );

   std::pair<uint64_t, bytes> execute( const contract_operation& o, const bool& commit );

   std::vector< uint64_t > get_attracted_contracts( const vms::base::vm_types& vm_type ) const;

   std::string get_state_root_evm() const;

   void set_state_root_evm( const std::string& hash );

   friend struct graphene::chain::database_fixture;

private:

   database& db;

   std::map<vms::base::vm_types, std::unique_ptr<vm_interface>> registered_vms;

};

} }
