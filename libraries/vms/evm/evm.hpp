#pragma once

#include <vm_interface.hpp>
#include <pp_state.hpp>
#include <evm_adapter.hpp>

#include <aleth/libethereum/LastBlockHashesFace.h>

namespace graphene { namespace chain { struct database_fixture; } }

namespace vms { namespace evm {

using namespace vms::base;

/**
 * @brief Class for getting a list of recent block hashes
 */
class last_block_hashes: public dev::eth::LastBlockHashesFace {

public:

   explicit last_block_hashes( const vector<block_id_type>& _hashes ): hashes(_hashes) {}

   /**
    * @brief Get preceding hashes
    * 
    * @param _mostRecentHash hash to get preceding
    * @return h256s - vector of hashes
    */
   h256s precedingHashes(h256 const& _mostRecentHash) const override
   {
      h256s result;
      result.resize(256);
      for (unsigned i = 0; i < 255; ++i)
         result[i] = ( hashes[i] == block_id_type() ) ? h256() : h256( hashes[i].str(), FixedHash<32>::ConstructFromStringType::FromHex, FixedHash<32>::ConstructFromHashType::AlignRight );

      return result;
   }

   void clear() override {}

private:
   vector<block_id_type> hashes;

};

/**
 * @class evm
 * @brief EVM entry point
 * 
 * `evm` implements the entry point to the virtual machine and stores its internal state.
 * This class is also responsible for initializing the necessary environment for the execution of contracts.
 */
class evm : public vm_interface
{

public:

   /**
    * @brief Construct a new evm object
    * 
    * @param path path to data directory for state
    * @param e_adapter evm_adapter for blockchain db
    */
   evm( const std::string& path, vms::evm::evm_adapter e_adapter );

   /**
    * @brief Execute `eth_op`
    * 
    * @param data serialized `eth_op`
    * @return execution result pair (first - fee for operation, second - exec result)
    */
   std::pair<uint64_t, bytes> exec( const bytes& data ) override;

   /**
    * @brief Get the attracted contracts for last EVM run
    * 
    * @return std::vector of uint64_t contract id form
    */
   std::vector< uint64_t > get_attracted_contracts( ) const override { return attracted_contracts; };

   /**
    * @brief Roll back state db
    * 
    * @param block_number block number to roll back
    */
   void roll_back_db( const uint32_t& block_number ) override;

   /**
    * @brief Get the contracts
    * 
    * @param ids contract ids in uint64_t form to get
    * @return std::map where key is address and value is serialized contract
    */
   std::map<uint64_t, bytes> get_contracts( const std::vector<uint64_t>& ids ) const override;

   /**
    * @brief Get the code of contract
    * 
    * @param id contract id in uint64_t form
    * @return code of contract
    */
   bytes get_code( const uint64_t& id ) const override;

   /**
    * @brief Get the state root
    * 
    * @return root in hex string
    */
   std::string get_state_root() const;

   /**
    * @brief Set the state root
    * 
    * @param hash root in hex string
    */
   void set_state_root( const std::string& hash );

   friend struct graphene::chain::database_fixture;

private:

   /**
    * @brief Get the adapter
    * 
    * @return evm_adapter
    */
   evm_adapter& get_adapter() { return adapter; }

   /**
    * @brief Create a eth transaction from `eth_op`
    * 
    * @param eth eth_op object
    * @return Transaction 
    */
   Transaction create_eth_transaction(const eth_op& eth) const;

   /**
    * @brief Create a environment from EVM run
    * 
    * @param last_hashes last block hashes
    * @return EnvInfo for EVM run
    */
   EnvInfo create_environment( last_block_hashes const& last_hashes );

   /**
    * @brief Transfer balance from suicided contract
    * 
    * @param suicide_transfer std::vector of pairs( first - from address, second - to address )
    */
   void transfer_suicide_balances(const std::vector< std::pair< Address, Address > >& suicide_transfer);

   /**
    * @brief Delete balances of accounts
    * 
    * @param accounts std::unordered_map with key - address, value - account to delete
    */
   void delete_balances( const std::unordered_map< Address, Account >& accounts );

   /**
    * @brief Select specifide attracted contracts
    * 
    * @param accounts std::unordered_map with key - address, value - account to delete
    * @return std::vector of uint64_t form contract ids 
    */
   std::vector< uint64_t > select_attracted_contracts( const std::unordered_map< Address, Account >& accounts );

   /**
    * @brief Finalize EVM run
    * 
    * @param trx_excp TransactionException of EVM run
    */
   void finalize( dev::eth::TransactionException& trx_excp );

   /**
    * @brief Clear temp vars of enviroment
    * 
    */
   void clear_temporary_variables();

   std::vector< uint64_t > attracted_contracts;

   std::unique_ptr< SealEngineFace > se;

   pp_state state;

   vms::evm::evm_adapter adapter;

};

} }