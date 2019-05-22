#pragma once

#include <pp_controller.hpp>
#include <wavm_adapter.hpp>
#include <vm_interface.hpp>

namespace vms { namespace wavm {

class wavm : public vms::base::vm_interface
{

public:

   wavm( pp_controller::config cfg, vms::wavm::wavm_adapter _adapter );

   std::pair<uint64_t, bytes> exec( const bytes& data ) override;

   std::vector< uint64_t > get_attracted_contracts() const override;

   void roll_back_db( const uint32_t& block_number ) override;

   std::map<uint64_t, bytes> get_contracts( const std::vector<uint64_t>& ids ) const override;

   bytes get_code( const uint64_t& id ) const override;

private:

   wavm_adapter& get_adapter() { return adapter; }

   eosio::chain::account_name id_to_wavm_name( const uint64_t& id, const uint64_t& type );

   std::pair<uint64_t, uint64_t> wavm_name_to_id( const eosio::chain::account_name acc_name );

   pp_controller contr;

   vms::wavm::wavm_adapter adapter;

   const std::string charmap = "12345abcdefghijklmnopqrstuvwxyz";

};

} }