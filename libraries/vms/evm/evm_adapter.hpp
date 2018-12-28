#pragma once

#include <utility>
#include <chain_adapter.hpp>
#include <string>
#include <vector>

namespace vms { namespace evm {

using namespace vms::base;

// pair<type, id>
using contract_or_account_id = std::pair<uint64_t, uint64_t>;

class evm_adapter : public chain_adapter
{

public:

   evm_adapter( database& _db ) : chain_adapter( _db ) {}

   void publish_contract_transfer( const contract_or_account_id& from_id, const contract_or_account_id& to_id,
                                   const uint64_t& asset_id, const int64_t& value );

   std::string get_next_result_id() const;

   // pair< amount, asset_id >
   std::vector<std::pair<int64_t, uint64_t>> get_contract_balances( const uint64_t& from ) const;

   void delete_contract_balances( const uint64_t& id );

   void contract_suicide( const uint64_t& id );

private:

};

} }
