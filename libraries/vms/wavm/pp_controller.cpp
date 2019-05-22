#include <pp_controller.hpp>
#include <eosio/chain/transaction_context.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/authorization_manager.hpp>
#include <eosio/chain/eosio_contract.hpp>
#include <chainbase/chainbase.hpp>

#include <eosio/chain/account_object.hpp>
#include <eosio/chain/block_summary_object.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/contract_table_objects.hpp>
#include <eosio/chain/generated_transaction_object.hpp>
#include <eosio/chain/transaction_object.hpp>
#include <eosio/chain/reversible_block_object.hpp>

namespace vms { namespace wavm {

using controller_index_set = eosio::chain::index_set<
   eosio::chain::account_index,
   eosio::chain::account_sequence_index,
   eosio::chain::global_property_multi_index,
   eosio::chain::dynamic_global_property_multi_index,
   eosio::chain::block_summary_multi_index,
   eosio::chain::transaction_multi_index,
   eosio::chain::generated_transaction_multi_index,
   eosio::chain::table_id_multi_index
>;

using contract_database_index_set = eosio::chain::index_set<
   eosio::chain::key_value_index,
   eosio::chain::index64_index,
   eosio::chain::index128_index,
   eosio::chain::index256_index,
   eosio::chain::index_double_index,
   eosio::chain::index_long_double_index
>;


struct pp_controller_impl {
   pp_controller&                               self;
   chainbase::database                          db;
   pp_controller::config                        conf;
   eosio::chain::wasm_interface                 wasmif;
   eosio::chain::authorization_manager          authorization;

   typedef std::pair< eosio::chain::scope_name, eosio::chain::action_name >  handler_key;
   std::map< eosio::chain::account_name, std::map<handler_key, eosio::chain::apply_handler> >   apply_handlers;

   void set_apply_handler( eosio::chain::account_name receiver, eosio::chain::account_name contract, eosio::chain::action_name action, eosio::chain::apply_handler v ) {
      apply_handlers[receiver][make_pair(contract,action)] = v;
   }

   pp_controller_impl( const pp_controller::config& cfg, pp_controller& s )
   :self(s),
   db( cfg.state_dir,
      cfg.read_only ? chainbase::database::read_only : chainbase::database::read_write,
      cfg.state_size ),
   conf( cfg ),
   wasmif( cfg.wasm_runtime ),
   authorization( s, db )
   {
#define SET_APP_HANDLER( receiver, contract, action) \
   set_apply_handler( #receiver, #contract, #action, &BOOST_PP_CAT(eosio::chain::apply_, BOOST_PP_CAT(contract, BOOST_PP_CAT(_,action) ) ) )

   SET_APP_HANDLER( eosio, eosio, newaccount );
   SET_APP_HANDLER( eosio, eosio, setcode );
   SET_APP_HANDLER( eosio, eosio, setabi );
   SET_APP_HANDLER( eosio, eosio, updateauth );
   SET_APP_HANDLER( eosio, eosio, deleteauth );
   SET_APP_HANDLER( eosio, eosio, linkauth );
   SET_APP_HANDLER( eosio, eosio, unlinkauth );
   SET_APP_HANDLER( eosio, eosio, canceldelay );

   init_database();
   }

   void create_native_account( eosio::chain::account_name name, const eosio::chain::authority& owner, const eosio::chain::authority& active, bool is_privileged = false ) {
      db.create<eosio::chain::account_object>([&](auto& a) {
         a.name = name;
         a.creation_date = conf.genesis.initial_timestamp;
         a.privileged = is_privileged;

         if( name == eosio::chain::config::system_account_name ) {
            a.set_abi(eosio::chain::eosio_contract_abi(eosio::chain::abi_def()));
         }
      });
      db.create<eosio::chain::account_sequence_object>([&](auto & a) {
        a.name = name;
      });

      const auto& owner_permission  = authorization.create_permission(name, eosio::chain::config::owner_name, 0, owner, conf.genesis.initial_timestamp );
      authorization.create_permission(name, eosio::chain::config::active_name, owner_permission.id, active, conf.genesis.initial_timestamp );

   }

   void init_database()
   {
      db.set_revision( 1 );    // temp
      controller_index_set::add_indices(db);
      contract_database_index_set::add_indices(db);

      authorization.add_indices();

      conf.genesis.initial_configuration.validate();
      db.create<eosio::chain::global_property_object>([&](auto& gpo ){
         gpo.configuration = conf.genesis.initial_configuration;
      });
      db.create<eosio::chain::dynamic_global_property_object>([](auto&){});

      authorization.initialize_database();

      eosio::chain::authority system_auth(conf.genesis.initial_key);
      create_native_account( eosio::chain::config::system_account_name, system_auth, system_auth, true );
   }

   bool sender_avoids_whitelist_blacklist_enforcement( eosio::chain::account_name sender )const {
      if( conf.sender_bypass_whiteblacklist.size() > 0 &&
         ( conf.sender_bypass_whiteblacklist.find( sender ) != conf.sender_bypass_whiteblacklist.end() ) )
      {
         return true;
      }

      return false;
   }

   void check_actor_list( const fc::flat_set<eosio::chain::account_name>& actors )const {
      if( actors.size() == 0 ) return;

      if( conf.actor_whitelist.size() > 0 ) {
         // throw if actors is not a subset of whitelist
         const auto& whitelist = conf.actor_whitelist;
         bool is_subset = true;

         // quick extents check, then brute force the check actors
         if (*actors.cbegin() >= *whitelist.cbegin() && *actors.crbegin() <= *whitelist.crbegin() ) {
            auto lower_bound = whitelist.cbegin();
            for (const auto& actor: actors) {
               lower_bound = std::lower_bound(lower_bound, whitelist.cend(), actor);

               // if the actor is not found, this is not a subset
               if (lower_bound == whitelist.cend() || *lower_bound != actor ) {
                  is_subset = false;
                  break;
               }

               // if the actor was found, we are guaranteed that other actors are either not present in the whitelist
               // or will be present in the range defined as [next actor,end)
               lower_bound = std::next(lower_bound);
            }
         } else {
            is_subset = false;
         }

         // helper lambda to lazily calculate the actors for error messaging
         static auto generate_missing_actors = [](const fc::flat_set<eosio::chain::account_name>& actors, const fc::flat_set<eosio::chain::account_name>& whitelist) -> std::vector<eosio::chain::account_name> {
            std::vector<eosio::chain::account_name> excluded;
            excluded.reserve( actors.size() );
            set_difference( actors.begin(), actors.end(),
                            whitelist.begin(), whitelist.end(),
                            std::back_inserter(excluded) );
            return excluded;
         };

         EOS_ASSERT( is_subset,  eosio::chain::actor_whitelist_exception,
                     "authorizing actor(s) in transaction are not on the actor whitelist: ${actors}",
                     ("actors", generate_missing_actors(actors, whitelist))
                   );
      } else if( conf.actor_blacklist.size() > 0 ) {
         // throw if actors intersects blacklist
         const auto& blacklist = conf.actor_blacklist;
         bool intersects = false;

         // quick extents check then brute force check actors
         if( *actors.cbegin() <= *blacklist.crbegin() && *actors.crbegin() >= *blacklist.cbegin() ) {
            auto lower_bound = blacklist.cbegin();
            for (const auto& actor: actors) {
               lower_bound = std::lower_bound(lower_bound, blacklist.cend(), actor);

               // if the lower bound in the blacklist is at the end, all other actors are guaranteed to
               // not exist in the blacklist
               if (lower_bound == blacklist.cend()) {
                  break;
               }

               // if the lower bound of an actor IS the actor, then we have an intersection
               if (*lower_bound == actor) {
                  intersects = true;
                  break;
               }
            }
         }

         // helper lambda to lazily calculate the actors for error messaging
         static auto generate_blacklisted_actors = [](const fc::flat_set<eosio::chain::account_name>& actors, const fc::flat_set<eosio::chain::account_name>& blacklist) -> std::vector<eosio::chain::account_name> {
            std::vector<eosio::chain::account_name> blacklisted;
            blacklisted.reserve( actors.size() );
            set_intersection( actors.begin(), actors.end(),
                              blacklist.begin(), blacklist.end(),
                              std::back_inserter(blacklisted)
                            );
            return blacklisted;
         };

         EOS_ASSERT( !intersects, eosio::chain::actor_blacklist_exception,
                     "authorizing actor(s) in transaction are on the actor blacklist: ${actors}",
                     ("actors", generate_blacklisted_actors(actors, blacklist))
                   );
      }
   }

   void check_contract_list( eosio::chain::account_name code )const {
      if( conf.contract_whitelist.size() > 0 ) {
         EOS_ASSERT( conf.contract_whitelist.find( code ) != conf.contract_whitelist.end(),
                     eosio::chain::contract_whitelist_exception,
                     "account '${code}' is not on the contract whitelist", ("code", code)
                   );
      } else if( conf.contract_blacklist.size() > 0 ) {
         EOS_ASSERT( conf.contract_blacklist.find( code ) == conf.contract_blacklist.end(),
                     eosio::chain::contract_blacklist_exception,
                     "account '${code}' is on the contract blacklist", ("code", code)
                   );
      }
   }

   void check_action_list( eosio::chain::account_name code, eosio::chain::action_name action )const {
      if( conf.action_blacklist.size() > 0 ) {
         EOS_ASSERT( conf.action_blacklist.find( std::make_pair(code, action) ) == conf.action_blacklist.end(),
                     eosio::chain::action_blacklist_exception,
                     "action '${code}::${action}' is on the action blacklist",
                     ("code", code)("action", action)
                   );
      }
   }

   void check_key_list( const eosio::chain::public_key_type& key )const {
      if( conf.key_blacklist.size() > 0 ) {
         EOS_ASSERT( conf.key_blacklist.find( key ) == conf.key_blacklist.end(),
                     eosio::chain::key_blacklist_exception,
                     "public key '${key}' is on the key blacklist",
                     ("key", key)
                   );
      }
   }
};

pp_controller::pp_controller( pp_controller::config cfg, wavm_adapter _adapter ) : my( new pp_controller_impl( cfg, *this ) ), adapter(_adapter) {}

pp_controller::~pp_controller() {}

const chainbase::database& pp_controller::db() const { return my->db; }

chainbase::database& pp_controller::mutable_db() const { return my->db; }

const eosio::chain::account_object& pp_controller::get_account( eosio::chain::account_name name ) const
{ try {
   return my->db.get<eosio::chain::account_object, eosio::chain::by_name>(name);
} FC_CAPTURE_AND_RETHROW( (name) ) }

const eosio::chain::dynamic_global_property_object& pp_controller::get_dynamic_global_properties() const
{
  return my->db.get<eosio::chain::dynamic_global_property_object>();
}

const eosio::chain::global_property_object& pp_controller::get_global_properties() const
{
  return my->db.get<eosio::chain::global_property_object>();
}

const eosio::chain::authorization_manager& pp_controller::get_authorization_manager() const
{
   return my->authorization;
}

eosio::chain::authorization_manager& pp_controller::get_mutable_authorization_manager()
{
   return my->authorization;
}

eosio::chain::block_id_type pp_controller::head_block_id() const
{
    return eosio::chain::block_id_type( adapter.head_block_id() );
}

eosio::chain::time_point pp_controller::pending_block_time() const
{
    return eosio::chain::time_point( fc::seconds( adapter.pending_block_time() ) );
}

eosio::chain::block_state_ptr pp_controller::pending_block_state() const
{
   eosio::chain::block_state state;
   state.block_num = adapter.pending_block_num();
   return std::make_shared<eosio::chain::block_state>(state);
}

fc::optional<eosio::chain::block_id_type> pp_controller::pending_producer_block_id() const
{
   return fc::optional<eosio::chain::block_id_type>( eosio::chain::block_id_type( adapter.pending_block_id() ) );
}

bool pp_controller::sender_avoids_whitelist_blacklist_enforcement( eosio::chain::account_name sender ) const {
   return my->sender_avoids_whitelist_blacklist_enforcement( sender );
}

void pp_controller::check_actor_list( const fc::flat_set<eosio::chain::account_name>& actors ) const
{
   my->check_actor_list( actors );
}

void pp_controller::check_contract_list( eosio::chain::account_name code )const {
   my->check_contract_list( code );
}

void pp_controller::check_action_list( eosio::chain::account_name code, eosio::chain::action_name action )const {
   my->check_action_list( code, action );
}

void pp_controller::check_key_list( const eosio::chain::public_key_type& key )const {
   my->check_key_list( key );
}

bool pp_controller::contracts_console() const
{
   return my->conf.contracts_console;
}

eosio::chain::wasm_interface& pp_controller::get_wasm_interface()
{
   return my->wasmif;
}

const eosio::chain::apply_handler* pp_controller::find_apply_handler( eosio::chain::account_name contract, eosio::chain::scope_name scope, eosio::chain::action_name act ) const
{
   auto native_handler_scope = my->apply_handlers.find( contract );
   if( native_handler_scope != my->apply_handlers.end() ) {
      auto handler = native_handler_scope->second.find( make_pair( scope, act ) );
      if( handler != native_handler_scope->second.end() )
         return &handler->second;
   }
   return nullptr;
}

} }