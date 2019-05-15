#pragma once

#include <wavm_adapter.hpp>
#include <eosio/chain/controller.hpp>

namespace vms { namespace wavm {

class pp_controller : public eosio::chain::abstract_controller 
{

public:
   pp_controller() {}
   ~pp_controller() {}

   eosio::chain::transaction_trace_ptr push_transaction( const eosio::chain::transaction_metadata_ptr& trx, fc::time_point deadline, uint32_t billed_cpu_time_us = 0 ) {}

   const chainbase::database& db()const {}
   const eosio::chain::fork_database& fork_db()const {}

   const eosio::chain::account_object&                 get_account( eosio::chain::account_name n )const {}
   const eosio::chain::global_property_object&         get_global_properties()const {}
   const eosio::chain::dynamic_global_property_object& get_dynamic_global_properties()const {}
   const eosio::chain::resource_limits_manager&        get_resource_limits_manager()const {}
   eosio::chain::resource_limits_manager&              get_mutable_resource_limits_manager() {}
   const eosio::chain::authorization_manager&          get_authorization_manager()const {}
   eosio::chain::authorization_manager&                get_mutable_authorization_manager() {}

   eosio::chain::block_id_type        head_block_id()const {}
   eosio::chain::time_point              pending_block_time()const {}
   eosio::chain::block_state_ptr         pending_block_state()const {}
   fc::optional<eosio::chain::block_id_type> pending_producer_block_id()const {}
   const eosio::chain::producer_schedule_type&    active_producers()const {}

   bool sender_avoids_whitelist_blacklist_enforcement( eosio::chain::account_name sender )const {}
   void check_actor_list( const fc::flat_set<eosio::chain::account_name>& actors )const {}
   void check_contract_list( eosio::chain::account_name code )const {}
   void check_action_list( eosio::chain::account_name code, eosio::chain::action_name action )const {}
   void check_key_list( const eosio::chain::public_key_type& key )const {}

   bool is_producing_block()const {}
   bool is_ram_billing_in_notify_allowed()const {}
   bool is_resource_greylisted(const eosio::chain::account_name &name) const {}

   void validate_expiration( const eosio::chain::transaction& t )const {}
   void validate_tapos( const eosio::chain::transaction& t )const {}

   int64_t set_proposed_producers( std::vector<eosio::chain::producer_key> producers ) {}

   bool skip_auth_check()const {}
   bool skip_db_sessions()const {}
   bool skip_trx_checks()const {}
   bool contracts_console()const {}

   const eosio::chain::apply_handler* find_apply_handler( eosio::chain::account_name contract, eosio::chain::scope_name scope, eosio::chain::action_name act )const {}
   eosio::chain::wasm_interface& get_wasm_interface() {}

private:
   chainbase::database& mutable_db()const {}

};

} }