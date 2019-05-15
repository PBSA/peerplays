#include <graphene/chain/contract_object.hpp>

namespace graphene { namespace chain {

void contract_balance_object::adjust_balance(const asset& delta)
{
   assert(delta.asset_id == asset_type);
   balance += delta.amount;
}

} }
