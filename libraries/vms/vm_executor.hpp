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

   void execute( const contract_operation& o, const bool& commit );

private:

   database& db;

   std::map<uint8_t, std::unique_ptr<vm_interface>> registered_vms;

};

} }
