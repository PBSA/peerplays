#pragma once

#include <cstdint>

namespace graphene { namespace chain { class database; } }

using namespace graphene::chain;

namespace vms { namespace base {

class chain_adapter {

public:

   chain_adapter( database& _db ) : db( _db ) {}

   virtual ~chain_adapter() {};

   bool is_there_account( const uint64_t& account_id ) const;

   bool is_there_contract( const uint64_t& contract_id ) const;

   uint32_t create_contract_obj();

   uint64_t get_next_contract_id() const;

   int64_t get_account_balance( const uint64_t& account_id, const uint64_t& asset_id ) const;

   int64_t get_contract_balance( const uint64_t& contract_id, const uint64_t& asset_id ) const;

   void add_account_balance( const uint64_t& account_id, const uint64_t& asset_id, const int64_t& amount );

   void add_contract_balance( const uint64_t& contract_id, const uint64_t& asset_id, const int64_t& amount );

   void sub_account_balance( const uint64_t& account_id, const uint64_t& asset_id, const int64_t& amount );

   void sub_contract_balance( const uint64_t& contract_id, const uint64_t& asset_id, const int64_t& amount );

   uint32_t head_block_num() const;

   uint64_t get_id_author_block() const;

   uint32_t head_block_time() const;

   bool evaluating_from_apply_block() const;

protected:

   database& db;

};

} }
