#pragma once

#include <chain_adapter.hpp>

namespace vms { namespace wavm {

class wavm_adapter : public vms::base::chain_adapter
{

public:

   wavm_adapter( database& _db ) : vms::base::chain_adapter( _db ) {}

private:

};

} }
