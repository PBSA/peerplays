#pragma once

#include <chain_adapter.hpp>
#include <wavm_adapter.hpp>
#include <fc_pp/static_variant.hpp>
#include <graphene/chain/protocol/types.hpp>

namespace vms { namespace base {

using adapters = fc_pp::static_variant<chain_adapter, vms::wavm::wavm_adapter>;

/**
 * @class vm_interface
 * @brief Interface for VMs
 * 
 * This class represents interface for all VM used in PeerPlays
 */
class vm_interface {

public:

   vm_interface( adapters _adapter ) : adapter(_adapter) {}
   virtual ~vm_interface() {}

   /// Execution VM operation
   virtual std::pair<uint64_t, bytes> exec( const bytes& ) = 0;
   /// Get contracts, used in last VM run
   virtual std::vector< uint64_t > get_attracted_contracts() const = 0;
   /// Rolling back VM database
   virtual void roll_back_db( const uint32_t& ) = 0;
   /// Get contracts from VM database
   virtual std::map<uint64_t, bytes> get_contracts( const std::vector<uint64_t>& ) const = 0;
   /// Get code fo contract from VM database
   virtual bytes get_code( const uint64_t& ) const = 0;

protected:

   adapters adapter;

};

} }
