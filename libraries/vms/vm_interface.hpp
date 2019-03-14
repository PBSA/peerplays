#pragma once

#include "chain_adapter.hpp"
#include <evm_adapter.hpp>
#include <fc/static_variant.hpp>

namespace vms { namespace base {

using namespace vms::evm;
using namespace dev;
using adapters = fc::static_variant<chain_adapter, evm_adapter>;

class vm_interface {

public:

   vm_interface( adapters _adapter ) : adapter(_adapter) {}
   virtual ~vm_interface() {}

   virtual std::pair<uint64_t, bytes> exec( const bytes& data, const bool commit ) = 0;
   virtual std::vector< uint64_t > get_attracted_contracts() const = 0;
   virtual void roll_back_db(/*hash or number block*/) = 0;

   virtual std::string get_state_root() const { return std::string(""); }
   virtual void set_state_root( const std::string& hash  ) {}

protected:

   adapters adapter;

};

} }
