#pragma once

#include <cstdint>
#include <memory>
#include <libdevcore/Common.h>
#include <libdevcore/FixedHash.h>

namespace graphene { namespace chain { class database; } }

using namespace graphene::chain;

namespace vms { namespace base {

using namespace dev;

// pair<type, id>
using contract_or_account_id = std::pair<uint64_t, uint64_t>;

class chain_adapter {

public:

   chain_adapter( database& _db ) : db( _db ) {}

   virtual ~chain_adapter() {};

   bool is_there_account( const uint64_t& account_id ) const;

   bool is_there_contract( const uint64_t& contract_id ) const;

   uint32_t create_contract_obj( const std::set<uint64_t>& allowed_assets );

   void delete_contract_obj( const uint64_t& contract_id );

   uint64_t get_next_contract_id() const;

   int64_t get_account_balance( const uint64_t& account_id, const uint64_t& asset_id ) const;

   int64_t get_contract_balance( const uint64_t& contract_id, const uint64_t& asset_id ) const;

   void change_balance( const contract_or_account_id& from_id, const uint64_t& asset_id, const int64_t& amount );

   uint32_t head_block_num() const;

   uint64_t get_id_author_block() const;

   uint32_t head_block_time() const;

   bool evaluating_from_apply_block() const;

protected:

   database& db;

};

} }
