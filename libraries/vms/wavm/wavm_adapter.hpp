#pragma once

#include <chain_adapter.hpp>

namespace vms { namespace wavm {

class wavm_adapter : public vms::base::chain_adapter
{

public:

   wavm_adapter( database& _db ) : vms::base::chain_adapter( _db ) {}

   wavm_adapter( const wavm_adapter& adapter ): chain_adapter( adapter ) {}

   const std::string& head_block_id() const;

   uint32_t pending_block_time() const;

   uint32_t pending_block_num() const;

   std::string pending_block_id() const;

private:

};

} }
