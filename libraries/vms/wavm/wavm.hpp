#pragma once

#include <pp_controller.hpp>
#include <vm_interface.hpp>

namespace vms { namespace wavm {

class wavm : public vms::base::vm_interface
{

public:

   wavm( vms::base::adapters adapter );

   std::pair<uint64_t, bytes> exec( const bytes& data ) override;

   std::vector< uint64_t > get_attracted_contracts() const override;

   void roll_back_db( const uint32_t& block_number ) override;

   std::map<uint64_t, bytes> get_contracts( const std::vector<uint64_t>& ids ) const override;

   bytes get_code( const uint64_t& id ) const override;

   eosio::chain::account_name id_to_wavm_name( const uint64_t& id, const uint64_t& type );

   std::pair<uint64_t, uint64_t> wavm_name_to_id( const eosio::chain::account_name acc_name );

private:

   wavm_adapter& get_adapter() { return adapter.get<vms::wavm::wavm_adapter>(); }

   pp_controller contr;

   const std::string charmap = "12345abcdefghijklmnopqrstuvwxyz";

};

} }