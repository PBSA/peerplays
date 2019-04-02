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

   std::pair<uint64_t, bytes> execute( const contract_operation& o);

   std::vector< uint64_t > get_attracted_contracts( const vm_types& vm_type ) const;

   void roll_back_db( const uint32_t& block_number );

   std::map<uint64_t, bytes> get_contracts( const vm_types& vm_type, const std::vector<contract_id_type>& ids ) const;

   bytes get_code( const vm_types& vm_type, const uint64_t& id ) const;

   friend struct graphene::chain::database_fixture;

private:

   database& db;

   std::map<vm_types, std::unique_ptr<vm_interface>> registered_vms;

};

} }
