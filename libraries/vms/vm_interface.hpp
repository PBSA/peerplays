#pragma once

#include <fc/static_variant.hpp>

namespace vms { namespace base {

using bytes = std::vector<char>;

class vm_interface {

public:

   virtual bytes exec( const bytes& data, const bool commit ) = 0;
   virtual bytes get_execute_result(/*result_obj_id*/) = 0;
   virtual void roll_back_db(/*hash or number block*/) = 0;

protected:

   fc::static_variant<int> chain_adapter;

};

} }
