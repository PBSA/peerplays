#pragma once

#include <cstdint>
#include <memory>
#include <set>

namespace graphene { namespace chain { class database; } }

using namespace graphene::chain;

namespace vms { namespace base {

/**
 * @brief contract_or_account_id::first - type, contract_or_account_id::second id/address
 * Address contains two fields. First says is it contract or account and second represends the address 
 */
using contract_or_account_id = std::pair<uint64_t, uint64_t>;

/**
 * @class chain_adapter
 * @breif Chaid adapter for VM
 * 
 * This class represents base functional for VM and chain database interaction
 */
class chain_adapter {

public:

   chain_adapter() = delete;

   chain_adapter( database& _db ) : db( _db ) {}

   chain_adapter( const chain_adapter& adapter ): db( adapter.db ) {}

   virtual ~chain_adapter() {};

   /**
    * Check is there account or not
    * @param account_id uint64_t form of of object id
    * @return true if `Yes`
    */
   bool is_there_account( const uint64_t& account_id ) const;

   /**
    * Check is there contract or not
    * @param account_id uint64_t form of of object id
    * @return true if `Yes`
    */
   bool is_there_contract( const uint64_t& contract_id ) const;

   /**
    * Create contract object in database
    * @param allowed_assets allowed assets for contract
    * @return instance number of new object
    */
   uint32_t create_contract_obj( const std::set<uint64_t>& allowed_assets );

   /**
    * Delete contract object in database
    * @param contract_id uint64_t form of of contract id
    */
   void delete_contract_obj( const uint64_t& contract_id );

   /**
    * Get next contract id
    * @return instance number of next contract object
    */
   uint64_t get_next_contract_id() const;

   /**
    * Get account balance from chain database
    * @param account_id uint64_t form of of account id
    * @param account_id uint64_t form of of asset id
    * @return amount of balance
    */
   int64_t get_account_balance( const uint64_t& account_id, const uint64_t& asset_id ) const;

   /**
    * Get contract balance from chain database
    * @param account_id uint64_t form of of contract id
    * @param account_id uint64_t form of of asset id
    * @return amount of balance
    */
   int64_t get_contract_balance( const uint64_t& contract_id, const uint64_t& asset_id ) const;

   /**
    * @brief Change balance of account or contract
    * Sub or add balance of account or contract, depends on amount sign
    * @param from_id address of object to bew changed
    * @param asset_id uint64_t form of of asset id
    * @param amount amount to add(or sub) to balance
    */
   void change_balance( const contract_or_account_id& from_id, const uint64_t& asset_id, const int64_t& amount );

   /// Get head block number
   uint32_t head_block_num() const;

   /// Get uint64_t form of of witness(last block creator) id
   uint64_t get_id_author_block() const;

   /// Get head block time
   uint32_t head_block_time() const;

   /// Is it evalution from block or not
   bool evaluating_from_block() const;

protected:

   database& db;

};

} }
