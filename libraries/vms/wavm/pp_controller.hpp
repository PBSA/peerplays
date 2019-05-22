#pragma once

#include <wavm_adapter.hpp>
#include <eosio/chain/controller.hpp>

namespace vms { namespace wavm {

struct pp_controller_impl;

class pp_controller : public eosio::chain::abstract_controller 
{

public:

   struct config {
      fc::flat_set<eosio::chain::account_name>   sender_bypass_whiteblacklist;
      fc::flat_set<eosio::chain::account_name>   actor_whitelist;
      fc::flat_set<eosio::chain::account_name>   actor_blacklist;
      fc::flat_set<eosio::chain::account_name>   contract_whitelist;
      fc::flat_set<eosio::chain::account_name>   contract_blacklist;
      fc::flat_set< std::pair<eosio::chain::account_name, eosio::chain::action_name> > action_blacklist;
      fc::flat_set<eosio::chain::public_key_type> key_blacklist;
      bool                     read_only              =  false;
      bool                     contracts_console      =  false;
      fc::path                 state_dir              =  eosio::chain::config::default_state_dir_name;
      uint64_t                 state_size             =  eosio::chain::config::default_state_size;

      eosio::chain::genesis_state            genesis;
      eosio::chain::wasm_interface::vm_type  wasm_runtime = eosio::chain::config::default_wasm_runtime;
   };

   pp_controller( pp_controller::config cfg, wavm_adapter _adapter );
   ~pp_controller();

   eosio::chain::transaction_trace_ptr push_transaction( const eosio::chain::transaction_metadata_ptr& trx, fc::time_point deadline, uint32_t billed_cpu_time_us = 0 ) {}

   const chainbase::database& db()const;

   const eosio::chain::account_object&                 get_account( eosio::chain::account_name n )const;
   const eosio::chain::global_property_object&         get_global_properties()const;
   const eosio::chain::dynamic_global_property_object& get_dynamic_global_properties()const;
   const eosio::chain::resource_limits_manager&        get_resource_limits_manager()const {}  // should be deleted
   eosio::chain::resource_limits_manager&              get_mutable_resource_limits_manager() {}  // should be deleted
   const eosio::chain::authorization_manager&          get_authorization_manager()const;
   eosio::chain::authorization_manager&                get_mutable_authorization_manager();

   eosio::chain::block_id_type        head_block_id()const;
   eosio::chain::time_point              pending_block_time()const;
   eosio::chain::block_state_ptr         pending_block_state()const;
   fc::optional<eosio::chain::block_id_type> pending_producer_block_id()const;
   const eosio::chain::producer_schedule_type&    active_producers()const {}  // should be deleted

   bool sender_avoids_whitelist_blacklist_enforcement( eosio::chain::account_name sender )const;
   void check_actor_list( const fc::flat_set<eosio::chain::account_name>& actors )const;
   void check_contract_list( eosio::chain::account_name code )const;
   void check_action_list( eosio::chain::account_name code, eosio::chain::action_name action )const;
   void check_key_list( const eosio::chain::public_key_type& key )const;

   bool is_producing_block()const { return true; }   // temp
   bool is_ram_billing_in_notify_allowed()const { return true; }   // temp
   bool is_resource_greylisted(const eosio::chain::account_name &name) const {}  // should be deleted?

   void validate_expiration( const eosio::chain::transaction& t )const {}  // should be deleted
   void validate_tapos( const eosio::chain::transaction& t )const {}  // should be deleted

   int64_t set_proposed_producers( std::vector<eosio::chain::producer_key> producers ) {}  // should be deleted

   bool skip_auth_check()const { return false; }  // temp
   bool skip_db_sessions() const { return false; } // temp
   bool skip_trx_checks() const { return true; }  // temp
   bool contracts_console()const;

   const eosio::chain::apply_handler* find_apply_handler( eosio::chain::account_name contract, eosio::chain::scope_name scope, eosio::chain::action_name act )const;
   eosio::chain::wasm_interface& get_wasm_interface();

private:
   chainbase::database& mutable_db()const;

   std::unique_ptr<pp_controller_impl> my;
   wavm_adapter adapter;

};

} }