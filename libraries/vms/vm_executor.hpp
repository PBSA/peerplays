#pragma once

#include <map>
#include <vm_interface.hpp>

namespace graphene { namespace chain { class contract_operation; } }

namespace vms { namespace base {

class vm_executor
{

public:

   vm_executor( database& _db ) : db( _db ) {}

   void init( const std::string& path );

   std::pair<uint64_t, bytes> execute( const contract_operation& o, const bool& commit );

   std::vector< uint64_t > get_attracted_contracts( const uint64_t& version_vm ) const;

   std::string get_state_root_evm() const;

   void set_state_root_evm( const std::string& hash );

private:

   database& db;

   std::map<uint8_t, std::unique_ptr<vm_interface>> registered_vms;

};

} }
