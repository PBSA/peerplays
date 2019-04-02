#pragma once

#include <vm_interface.hpp>
#include <pp_state.hpp>

#include <aleth/libethereum/LastBlockHashesFace.h>

namespace graphene { namespace chain { struct database_fixture; } }

namespace vms { namespace evm {

using namespace vms::base;

class last_block_hashes: public dev::eth::LastBlockHashesFace {

public:

   explicit last_block_hashes( const vector<block_id_type>& _hashes ): hashes(_hashes) {}

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

class evm : public vm_interface
{

public:

   evm( const std::string& path, adapters adapter );

   std::pair<uint64_t, bytes> exec( const bytes& data ) override;

   std::vector< uint64_t > get_attracted_contracts( ) const override { return attracted_contracts; };

   void roll_back_db( const uint32_t& block_number ) override;

   std::map<uint64_t, bytes> get_contracts( const std::vector<uint64_t>& ids ) const override;

   bytes get_code( const uint64_t& id ) const override;

   std::string get_state_root() const;

   void set_state_root( const std::string& hash );

   friend struct graphene::chain::database_fixture;

private:

   evm_adapter& get_adapter() { return adapter.get<vms::evm::evm_adapter>(); }

   Transaction create_eth_transaction(const eth_op& eth) const;

   EnvInfo create_environment( last_block_hashes const& last_hashes );

   void transfer_suicide_balances(const std::vector< std::pair< Address, Address > >& suicide_transfer);

   void delete_balances( const std::unordered_map< Address, Account >& accounts );

   std::vector< uint64_t > select_attracted_contracts( const std::unordered_map< Address, Account >& accounts );

   void clear_temporary_variables();

   std::vector< uint64_t > attracted_contracts;

   std::unique_ptr< SealEngineFace > se;

   pp_state state;

};

} }