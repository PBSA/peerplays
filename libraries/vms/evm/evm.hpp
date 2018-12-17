#pragma once

#include <vms/vm_interface.hpp>

namespace vms { namespace evm {

using namespace vms::base;

class evm : public vm_interface {

public:

   bytes exec( const bytes& data, const bool commit ) override {}
   bytes get_execute_result(/*result_obj_id*/) override {}
   void roll_back_db(/*hash or number block*/) override {}

private:

};

} }
