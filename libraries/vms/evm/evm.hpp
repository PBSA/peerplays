#pragma once

#include <vm_interface.hpp>
#include <pp_state.hpp>
#include <graphene/chain/protocol/contract.hpp>

#include <aleth/libethereum/LastBlockHashesFace.h>

namespace vms { namespace evm {

using namespace vms::base;

class evm : public vm_interface
{

public:

   evm( const std::string& path, adapters adapter ) : vm_interface( adapter ), state( fs::path( path ), adapter.get<evm_adapter>() ) {}

   bytes exec( const bytes& data, const bool commit ) override;
   bytes get_execute_result(/*result_obj_id*/) override {}
   void roll_back_db(/*hash or number block*/) override {}

private:

   evm_adapter& get_adapter() { return adapter.get<vms::evm::evm_adapter>(); }

   Transaction create_eth_transaction(const eth_op& eth) const;

   EnvInfo create_environment();

   void transfer_suicide_balances(const std::vector< std::pair< Address, Address > >& suicide_transfer);

   void delete_balances( const std::unordered_map< Address, Account >& accounts );

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
