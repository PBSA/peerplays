#pragma once

#include <map>
#include <vms/vm_interface.hpp>

namespace vms { namespace base {

class vm_executor
{

public:

private:

   std::map<uint8_t, vm_interface> registered_vms;

};

} }
