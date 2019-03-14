#pragma once

#include <vm_interface.hpp>
#include <pp_state.hpp>
#include <graphene/chain/protocol/contract.hpp>

#include <aleth/libethereum/LastBlockHashesFace.h>

namespace graphene { namespace chain { struct database_fixture; } }

namespace vms { namespace evm {

using namespace vms::base;

class evm : public vm_interface
{

public:

   evm( const std::string& path, adapters adapter );

   std::pair<uint64_t, bytes> exec( const bytes& data, const bool commit ) override;

   std::vector< uint64_t > get_attracted_contracts( ) const override { return attracted_contracts; };

   bytes get_execute_result(/*result_obj_id*/) override {}

   void roll_back_db(/*hash or number block*/) override {}

   std::string get_state_root() const override;

   void set_state_root( const std::string& hash ) override;

   friend struct graphene::chain::database_fixture;

private:

   evm_adapter& get_adapter() { return adapter.get<vms::evm::evm_adapter>(); }

   Transaction create_eth_transaction(const eth_op& eth) const;

   EnvInfo create_environment();

   void transfer_suicide_balances(const std::vector< std::pair< Address, Address > >& suicide_transfer);

   void delete_balances( const std::unordered_map< Address, Account >& accounts );

   std::vector< uint64_t > select_attracted_contracts( const std::unordered_map< Address, Account >& accounts );

   void clear_temporary_variables();

   std::vector< uint64_t > attracted_contracts;

   std::unique_ptr< SealEngineFace > se;

   pp_state state;

};

class last_block_hashes: public dev::eth::LastBlockHashesFace {

public:

   explicit last_block_hashes(){ m_lastHashes.clear(); }

   h256s precedingHashes(h256 const& _mostRecentHash) const override
   {
      if (m_lastHashes.empty() || m_lastHashes.front() != _mostRecentHash)
         m_lastHashes.resize(256);
      return m_lastHashes;
   }

   void clear() override { m_lastHashes.clear(); }

private:

   mutable h256s m_lastHashes;

};

} }
