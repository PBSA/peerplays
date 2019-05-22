#include <wavm_adapter.hpp>
#include <graphene/chain/database.hpp>

namespace vms { namespace wavm {
    
const std::string& wavm_adapter::head_block_id() const
{
    return db.head_block_id().str();
}

uint32_t wavm_adapter::pending_block_time() const
{
    return db.get_current_block().timestamp.sec_since_epoch();
}

uint32_t wavm_adapter::pending_block_num() const
{
    return db.get_current_block().block_num();
}

std::string wavm_adapter::pending_block_id() const
{
    return db.get_current_block().id().str();
}

} }