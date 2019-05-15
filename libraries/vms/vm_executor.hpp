#pragma once

#include <map>
#include <vm_interface.hpp>

namespace graphene { namespace chain { class contract_operation; struct database_fixture; } }

namespace vms { namespace base {

/**
 * @class vm_executor
 * @brief Manager class for VM
 * 
 * It stores virtual machine instances.
 * The class invokes the desired virtual machine depending on the input for execute method.
 */
class vm_executor
{

public:

   vm_executor( database& _db ) : db( _db ) {}

   /**
    * @brief Init VM objects with given path
    * 
    * @param path path to VM db directory
    */
   void init( const std::string& path );

   /**
    * @brief Executions of given `contract_operation`
    * 
    * @param o `contract_operation` to execute
    * @return execution result pair (first - fee for operation, second - exec result)
    */
   std::pair<uint64_t, bytes> execute( const contract_operation& o);

   /**
    * @brief Get the attracted contracts in recent VM run
    * 
    * @param vm_type VM type for which to get 
    * @return uint64_t form of of contract id
    */
   std::vector< uint64_t > get_attracted_contracts( const vm_types& vm_type ) const;

   /**
    * @brief Roll back db for all VMs by given block number
    * 
    * @param block_number number of block for which to roll
    */
   void roll_back_db( const uint32_t& block_number );

   /**
    * @brief Get the contracts from VM db
    * 
    * @param vm_type VM type for which to get 
    * @param ids contract ids to get
    * @return std::map where key is address and value is serialized contract
    */
   std::map<uint64_t, bytes> get_contracts( const vm_types& vm_type, const std::vector<contract_id_type>& ids ) const;

   /**
    * @brief Get the contract code
    * 
    * @param vm_type VM type for which to get 
    * @param id uint64_t form of of contract id
    * @return bytes contract code
    */
   bytes get_code( const vm_types& vm_type, const uint64_t& id ) const;

   friend struct graphene::chain::database_fixture;

private:

   database& db;

   /// Here stored all VMs objects
   std::map<vm_types, std::unique_ptr<vm_interface>> registered_vms;

};

} }
