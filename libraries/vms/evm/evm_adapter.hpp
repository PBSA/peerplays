#pragma once

#include <utility>
#include <chain_adapter.hpp>
#include <string>
#include <vector>
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/protocol/block.hpp>

#include <fc/crypto/sha256.hpp>

namespace vms { namespace evm {

using namespace vms::base;

/**
 * @class evm_adapter
 * @breif Chaid adapter for EVM
 * 
 * This class represents functional for EVM and chain database interaction ( inheritor of `chain_adapter` )
 */
class evm_adapter : public chain_adapter
{

public:

   evm_adapter( database& _db ) : chain_adapter( _db ) {}

   evm_adapter( const evm_adapter& adapter ): chain_adapter( adapter ) {}

   /**
    * @brief Create contract_transfer_operation to publish contract transfer in blockchain
    * @param from from which contract
    * @param to to which object
    * @param amount asset id to transfer
    * @param value asset amount to transfer
    */
   void publish_contract_transfer( const contract_or_account_id& from_id, const contract_or_account_id& to_id,
                                   const uint64_t& asset_id, const int64_t& value );

   /**
    * @brief Get contract balances
    * @param id uint64_t form of contract id
    * @return assets std::vector in form of pairs( first - amout, second - asset id in uint64_t form of )
    */
   std::vector<std::pair<int64_t, uint64_t>> get_contract_balances( const uint64_t& from ) const;

   /**
    * @brief Delete contract balance
    * 
    * @param id uint64_t form of contract id
    */
   void delete_contract_balances( const uint64_t& id );

   /**
    * @brief Contract suicide
    * 
    * @param id uint64_t form of contract id
    */
   void contract_suicide( const uint64_t& id );

   /**
    * @return std::vector of block_id_type    * 
    */
   vector<graphene::chain::block_id_type> get_last_block_hashes() const;

   /**
    * @brief Get the current block
    * 
    * @return signed_block 
    */
   signed_block get_current_block() const;

   /**
    * @brief Get the last valid state root
    * 
    * @param block_number number of block
    * @return fc::sha256 if it exists 
    */
   fc::optional<fc::sha256> get_last_valid_state_root( const uint32_t& block_number );

   /**
    * @brief Get the allowed assets for contract
    * 
    * @param id uint64_t form of of contract id
    * @return std::set<uint64_t> 
    */
   std::set<uint64_t> get_allowed_assets( const uint64_t& id );

   uint64_t get_block_gas_limit() const;

   uint64_t get_block_gas_used() const;

private:

};

} }
