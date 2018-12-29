#pragma once

#include "chain_adapter.hpp"
#include <evm_adapter.hpp>
#include <fc/static_variant.hpp>

namespace vms { namespace base {

using namespace vms::evm;

using bytes = std::vector<char>;
using adapters = fc::static_variant<chain_adapter, evm_adapter>;

class vm_interface {

public:

   vm_interface( adapters _adapter ) : adapter(_adapter) {}
   virtual ~vm_interface() {}

   virtual bytes exec( const bytes& data, const bool commit ) = 0;
   virtual std::vector< uint64_t > get_attracted_contracts() const = 0;
   virtual bytes get_execute_result(/*result_obj_id*/) = 0;
   virtual void roll_back_db(/*hash or number block*/) = 0;

protected:

   adapters adapter;

};

} }
