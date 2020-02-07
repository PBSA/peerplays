#include <graphene/peerplays_sidechain/peerplays_sidechain_plugin.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <fc/log/logger.hpp>
#include <fc/smart_ref_impl.hpp>

#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_manager.hpp>
#include <graphene/utilities/key_conversion.hpp>

namespace bpo = boost::program_options;

namespace graphene { namespace peerplays_sidechain {

namespace detail
{

class peerplays_sidechain_plugin_impl
{
   public:
      peerplays_sidechain_plugin_impl(peerplays_sidechain_plugin& _plugin);
      virtual ~peerplays_sidechain_plugin_impl();

      void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg);
      void plugin_initialize(const boost::program_options::variables_map& options);
      void plugin_startup();

      son_id_type get_son_id();
      son_object get_son_object();
      bool is_active_son();
      std::map<chain::public_key_type, fc::ecc::private_key>& get_private_keys();

      void schedule_heartbeat_loop();
      void heartbeat_loop();
      void create_son_down_proposals();
      void recreate_primary_wallet();
      void process_deposits();
      //void process_withdrawals();
      void on_block_applied( const signed_block& b );
      void on_objects_new(const vector<object_id_type>& new_object_ids);

   private:
      peerplays_sidechain_plugin& plugin;

      bool config_ready_son;
      bool config_ready_bitcoin;

      std::unique_ptr<peerplays_sidechain::sidechain_net_manager> net_manager;
      std::map<chain::public_key_type, fc::ecc::private_key> _private_keys;
      std::set<chain::son_id_type> _sons;
      fc::future<void> _heartbeat_task;

};

peerplays_sidechain_plugin_impl::peerplays_sidechain_plugin_impl(peerplays_sidechain_plugin& _plugin) :
      plugin( _plugin ),
      config_ready_son(false),
      config_ready_bitcoin(false),
      net_manager(nullptr)
{
}

peerplays_sidechain_plugin_impl::~peerplays_sidechain_plugin_impl()
{
   try {
      if( _heartbeat_task.valid() )
         _heartbeat_task.cancel_and_wait(__FUNCTION__);
   } catch(fc::canceled_exception&) {
      //Expected exception. Move along.
   } catch(fc::exception& e) {
      edump((e.to_detail_string()));
   }
}

void peerplays_sidechain_plugin_impl::plugin_set_program_options(
   boost::program_options::options_description& cli,
   boost::program_options::options_description& cfg)
{
   auto default_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(std::string("nathan")));
   string son_id_example = fc::json::to_string(chain::son_id_type(5));

   cli.add_options()
         ("son-id", bpo::value<vector<string>>(), ("ID of SON controlled by this node (e.g. " + son_id_example + ", quotes are required)").c_str())
         ("peerplays-private-key", bpo::value<vector<string>>()->composing()->multitoken()->
               DEFAULT_VALUE_VECTOR(std::make_pair(chain::public_key_type(default_priv_key.get_public_key()), graphene::utilities::key_to_wif(default_priv_key))),
               "Tuple of [PublicKey, WIF private key]")

         ("bitcoin-node-ip", bpo::value<string>()->default_value("99.79.189.95"), "IP address of Bitcoin node")
         ("bitcoin-node-zmq-port", bpo::value<uint32_t>()->default_value(11111), "ZMQ port of Bitcoin node")
         ("bitcoin-node-rpc-port", bpo::value<uint32_t>()->default_value(22222), "RPC port of Bitcoin node")
         ("bitcoin-node-rpc-user", bpo::value<string>()->default_value("1"), "Bitcoin RPC user")
         ("bitcoin-node-rpc-password", bpo::value<string>()->default_value("1"), "Bitcoin RPC password")
         ("bitcoin-address", bpo::value<string>()->default_value("2N911a7smwDzUGARg8s7Q1ViizFCw6gWcbR"), "Bitcoin address")
         ("bitcoin-public-key", bpo::value<string>()->default_value("02d0f137e717fb3aab7aff99904001d49a0a636c5e1342f8927a4ba2eaee8e9772"), "Bitcoin public key")
         ("bitcoin-private-key", bpo::value<string>()->default_value("cVN31uC9sTEr392DLVUEjrtMgLA8Yb3fpYmTRj7bomTm6nn2ANPr"), "Bitcoin private key")
         ;
   cfg.add(cli);
}

void peerplays_sidechain_plugin_impl::plugin_initialize(const boost::program_options::variables_map& options)
{
   config_ready_son = options.count( "son-id" ) && options.count( "peerplays-private-key" );
   if (config_ready_son) {
      LOAD_VALUE_SET(options, "son-id", _sons, chain::son_id_type)
      if( options.count("peerplays-private-key") )
      {
         const std::vector<std::string> key_id_to_wif_pair_strings = options["peerplays-private-key"].as<std::vector<std::string>>();
         for (const std::string& key_id_to_wif_pair_string : key_id_to_wif_pair_strings)
         {
            auto key_id_to_wif_pair = graphene::app::dejsonify<std::pair<chain::public_key_type, std::string> >(key_id_to_wif_pair_string, 5);
            ilog("Public Key: ${public}", ("public", key_id_to_wif_pair.first));
            fc::optional<fc::ecc::private_key> private_key = graphene::utilities::wif_to_key(key_id_to_wif_pair.second);
            if (!private_key)
            {
               // the key isn't in WIF format; see if they are still passing the old native private key format.  This is
               // just here to ease the transition, can be removed soon
               try
               {
                  private_key = fc::variant(key_id_to_wif_pair.second, 2).as<fc::ecc::private_key>(1);
               }
               catch (const fc::exception&)
               {
                  FC_THROW("Invalid WIF-format private key ${key_string}", ("key_string", key_id_to_wif_pair.second));
               }
            }
            _private_keys[key_id_to_wif_pair.first] = *private_key;
         }
      }
   } else {
      wlog("Haven't set up SON parameters");
      throw;
   }

   plugin.database().applied_block.connect( [&] (const signed_block& b) { on_block_applied(b); } );
   plugin.database().new_objects.connect( [&] (const vector<object_id_type>& ids, const flat_set<account_id_type>& impacted_accounts) { on_objects_new(ids); } );

   net_manager = std::unique_ptr<sidechain_net_manager>(new sidechain_net_manager(plugin));

   config_ready_bitcoin = options.count( "bitcoin-node-ip" ) &&
           options.count( "bitcoin-node-zmq-port" ) && options.count( "bitcoin-node-rpc-port" ) &&
           options.count( "bitcoin-node-rpc-user" ) && options.count( "bitcoin-node-rpc-password" ) &&
           options.count( "bitcoin-address" ) && options.count( "bitcoin-public-key" ) && options.count( "bitcoin-private-key" );
   if (config_ready_bitcoin) {
      net_manager->create_handler(sidechain_type::bitcoin, options);
      ilog("Bitcoin sidechain handler created");
   } else {
      wlog("Haven't set up Bitcoin sidechain parameters");
   }

   //config_ready_ethereum = options.count( "ethereum-node-ip" ) &&
   //        options.count( "ethereum-address" ) && options.count( "ethereum-public-key" ) && options.count( "ethereum-private-key" );
   //if (config_ready_ethereum) {
   //   net_manager->create_handler(sidechain_type::ethereum, options);
   //   ilog("Ethereum sidechain handler created");
   //} else {
   //   wlog("Haven't set up Ethereum sidechain parameters");
   //}

   if (!(config_ready_bitcoin /*&& config_ready_ethereum*/)) {
      wlog("Haven't set up any sidechain parameters");
      throw;
   }
}

void peerplays_sidechain_plugin_impl::plugin_startup()
{
   if (config_ready_son) {
      ilog("SON running");

      ilog("Starting heartbeats for ${n} sons.", ("n", _sons.size()));
      schedule_heartbeat_loop();
   } else {
      elog("No sons configured! Please add SON IDs and private keys to configuration.");
   }

   if (config_ready_bitcoin) {
      ilog("Bitcoin sidechain handler running");
   }

   //if (config_ready_ethereum) {
   //   ilog("Ethereum sidechain handler running");
   //}
}

son_id_type peerplays_sidechain_plugin_impl::get_son_id()
{
   return *(_sons.begin());
}

son_object peerplays_sidechain_plugin_impl::get_son_object()
{
   const auto& idx = plugin.database().get_index_type<chain::son_index>().indices().get<by_id>();
   auto son_obj = idx.find( get_son_id() );
   if (son_obj == idx.end())
      return {};
   return *son_obj;
}

bool peerplays_sidechain_plugin_impl::is_active_son()
{
   const auto& idx = plugin.database().get_index_type<chain::son_index>().indices().get<by_id>();
   auto son_obj = idx.find( get_son_id() );
   if (son_obj == idx.end())
      return false;

   const chain::global_property_object& gpo = plugin.database().get_global_properties();
   vector<son_id_type> active_son_ids;
   active_son_ids.reserve(gpo.active_sons.size());
   std::transform(gpo.active_sons.begin(), gpo.active_sons.end(),
                  std::inserter(active_son_ids, active_son_ids.end()),
                  [](const son_info& swi) {
      return swi.son_id;
   });

   auto it = std::find(active_son_ids.begin(), active_son_ids.end(), get_son_id());

   return (it != active_son_ids.end());
}

std::map<chain::public_key_type, fc::ecc::private_key>& peerplays_sidechain_plugin_impl::get_private_keys()
{
   return _private_keys;
}

void peerplays_sidechain_plugin_impl::schedule_heartbeat_loop()
{
   fc::time_point now = fc::time_point::now();
   int64_t time_to_next_heartbeat = 180000000;

   fc::time_point next_wakeup( now + fc::microseconds( time_to_next_heartbeat ) );

   _heartbeat_task = fc::schedule([this]{heartbeat_loop();},
                                         next_wakeup, "SON Heartbeat Production");
}

void peerplays_sidechain_plugin_impl::heartbeat_loop()
{
   schedule_heartbeat_loop();
   chain::database& d = plugin.database();
   chain::son_id_type son_id = *(_sons.begin());
   const auto& idx = d.get_index_type<chain::son_index>().indices().get<by_id>();
   auto son_obj = idx.find( son_id );
   if(son_obj == idx.end())
      return;
   const chain::global_property_object& gpo = d.get_global_properties();
   vector<son_id_type> active_son_ids;
   active_son_ids.reserve(gpo.active_sons.size());
   std::transform(gpo.active_sons.begin(), gpo.active_sons.end(),
                  std::inserter(active_son_ids, active_son_ids.end()),
                  [](const son_info& swi) {
      return swi.son_id;
   });

   auto it = std::find(active_son_ids.begin(), active_son_ids.end(), son_id);
   if(it != active_son_ids.end() || son_obj->status == chain::son_status::in_maintenance) {
      ilog("peerplays_sidechain_plugin:  sending heartbeat");
      chain::son_heartbeat_operation op;
      op.owner_account = son_obj->son_account;
      op.son_id = son_id;
      op.ts = fc::time_point::now() + fc::seconds(0);
      chain::signed_transaction trx = d.create_signed_transaction(_private_keys.begin()->second, op);
      fc::future<bool> fut = fc::async( [&](){
         try {
            d.push_transaction(trx);
            if(plugin.app().p2p_node())
               plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            return true;
         } catch(fc::exception e){
            ilog("peerplays_sidechain_plugin_impl:  sending heartbeat failed with exception ${e}",("e", e.what()));
            return false;
         }
      });
      fut.wait(fc::seconds(10));
   }
}

void peerplays_sidechain_plugin_impl::create_son_down_proposals()
{
   auto create_son_down_proposal = [&](chain::son_id_type son_id, fc::time_point_sec last_active_ts) {
      chain::database& d = plugin.database();
      chain::son_id_type my_son_id = *(_sons.begin());
      const chain::global_property_object& gpo = d.get_global_properties();
      const auto& idx = d.get_index_type<chain::son_index>().indices().get<by_id>();
      auto son_obj = idx.find( my_son_id );

      chain::son_report_down_operation son_down_op;
      son_down_op.payer = gpo.parameters.get_son_btc_account_id();
      son_down_op.son_id = son_id;
      son_down_op.down_ts = last_active_ts;

      proposal_create_operation proposal_op;
      proposal_op.fee_paying_account = son_obj->son_account;
      proposal_op.proposed_ops.push_back( op_wrapper( son_down_op ) );
      uint32_t lifetime = ( gpo.parameters.block_interval * gpo.active_witnesses.size() ) * 3;
      proposal_op.expiration_time = time_point_sec( d.head_block_time().sec_since_epoch() + lifetime );
      return proposal_op;
   };

   chain::database& d = plugin.database();
   const chain::global_property_object& gpo = d.get_global_properties();
   const chain::dynamic_global_property_object& dgpo = d.get_dynamic_global_properties();
   const auto& idx = d.get_index_type<chain::son_index>().indices().get<by_id>();
   std::set<son_id_type> sons_being_reported_down = d.get_sons_being_reported_down();
   chain::son_id_type my_son_id = *(_sons.begin());
   for(auto son_inf: gpo.active_sons) {
      if(my_son_id == son_inf.son_id || (sons_being_reported_down.find(son_inf.son_id) != sons_being_reported_down.end())){
         continue;
      }
      auto son_obj = idx.find( son_inf.son_id );
      auto stats = son_obj->statistics(d);
      fc::time_point_sec last_maintenance_time = dgpo.next_maintenance_time - gpo.parameters.maintenance_interval;
      fc::time_point_sec last_active_ts = ((stats.last_active_timestamp > last_maintenance_time) ? stats.last_active_timestamp : last_maintenance_time);
      int64_t down_threshold = 2*180000000;
      if(((son_obj->status == chain::son_status::active) || (son_obj->status == chain::son_status::request_maintenance)) &&
          ((fc::time_point::now() - last_active_ts) > fc::microseconds(down_threshold)))  {
         ilog("peerplays_sidechain_plugin:  sending son down proposal for ${t} from ${s}",("t",std::string(object_id_type(son_obj->id)))("s",std::string(object_id_type(my_son_id))));
         chain::proposal_create_operation op = create_son_down_proposal(son_inf.son_id, last_active_ts);
         chain::signed_transaction trx = d.create_signed_transaction(_private_keys.begin()->second, op);
         fc::future<bool> fut = fc::async( [&](){
            try {
               d.push_transaction(trx);
               if(plugin.app().p2p_node())
                  plugin.app().p2p_node()->broadcast(net::trx_message(trx));
               return true;
            } catch(fc::exception e){
               ilog("peerplays_sidechain_plugin_impl:  sending son down proposal failed with exception ${e}",("e", e.what()));
               return false;
            }
         });
         fut.wait(fc::seconds(10));
      }
   }
}

void peerplays_sidechain_plugin_impl::recreate_primary_wallet()
{
   net_manager->recreate_primary_wallet();
}

void peerplays_sidechain_plugin_impl::process_deposits() {
}

//void peerplays_sidechain_plugin_impl::process_withdrawals() {
//}

void peerplays_sidechain_plugin_impl::on_block_applied( const signed_block& b )
{
   chain::database& d = plugin.database();
   chain::son_id_type my_son_id = *(_sons.begin());
   const chain::global_property_object& gpo = d.get_global_properties();
   bool latest_block = ((fc::time_point::now() - b.timestamp) < fc::microseconds(gpo.parameters.block_interval * 1000000));
   // Return if there are no active SONs
   if(gpo.active_sons.size() <= 0 || !latest_block) {
      return;
   }

   chain::son_id_type next_son_id = d.get_scheduled_son(1);
   if(next_son_id == my_son_id) {

      create_son_down_proposals();

      recreate_primary_wallet();

      process_deposits();

      //process_withdrawals();

   }
}

void peerplays_sidechain_plugin_impl::on_objects_new(const vector<object_id_type>& new_object_ids)
{
   chain::database& d = plugin.database();
   chain::son_id_type my_son_id = *(_sons.begin());
   const chain::global_property_object& gpo = d.get_global_properties();
   const auto& idx = d.get_index_type<chain::son_index>().indices().get<by_id>();
   auto son_obj = idx.find( my_son_id );
   vector<son_id_type> active_son_ids;
   active_son_ids.reserve(gpo.active_sons.size());
   std::transform(gpo.active_sons.begin(), gpo.active_sons.end(),
                  std::inserter(active_son_ids, active_son_ids.end()),
                  [](const son_info& swi) {
      return swi.son_id;
   });

   auto it = std::find(active_son_ids.begin(), active_son_ids.end(), my_son_id);
   if(it == active_son_ids.end()) {
      return;
   }

   auto approve_proposal = [ & ]( const chain::proposal_id_type& id )
   {
      ilog("peerplays_sidechain_plugin:  sending approval for ${t} from ${s}",("t",std::string(object_id_type(id)))("s",std::string(object_id_type(my_son_id))));
      chain::proposal_update_operation puo;
      puo.fee_paying_account = son_obj->son_account;
      puo.proposal = id;
      puo.active_approvals_to_add = { son_obj->son_account };
      chain::signed_transaction trx = d.create_signed_transaction(_private_keys.begin()->second, puo);
      fc::future<bool> fut = fc::async( [&](){
         try {
            d.push_transaction(trx);
            if(plugin.app().p2p_node())
               plugin.app().p2p_node()->broadcast(net::trx_message(trx));
            return true;
         } catch(fc::exception e){
            ilog("peerplays_sidechain_plugin_impl:  sending approval failed with exception ${e}",("e", e.what()));
            return false;
         }
      });
      fut.wait(fc::seconds(10));
   };

   for(auto object_id: new_object_ids) {
      if( object_id.is<chain::proposal_object>() ) {
         const object* obj = d.find_object(object_id);
         const chain::proposal_object* proposal = dynamic_cast<const chain::proposal_object*>(obj);
         if(proposal == nullptr || (proposal->available_active_approvals.find(son_obj->son_account) != proposal->available_active_approvals.end())) {
            return;
         }

         if(proposal->proposed_transaction.operations.size() == 1
         && proposal->proposed_transaction.operations[0].which() == chain::operation::tag<chain::son_report_down_operation>::value) {
            approve_proposal( proposal->id );
         }

         if(proposal->proposed_transaction.operations.size() == 1
         && proposal->proposed_transaction.operations[0].which() == chain::operation::tag<chain::son_wallet_update_operation>::value) {
            approve_proposal( proposal->id );
         }
      }
   }
}

} // end namespace detail

peerplays_sidechain_plugin::peerplays_sidechain_plugin() :
   my( new detail::peerplays_sidechain_plugin_impl(*this) )
{
}

peerplays_sidechain_plugin::~peerplays_sidechain_plugin()
{
   return;
}

std::string peerplays_sidechain_plugin::plugin_name()const
{
   return "peerplays_sidechain";
}

void peerplays_sidechain_plugin::plugin_set_program_options(
   boost::program_options::options_description& cli,
   boost::program_options::options_description& cfg)
{
   ilog("peerplays sidechain plugin:  plugin_set_program_options()");
    my->plugin_set_program_options(cli, cfg);
}

void peerplays_sidechain_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
   ilog("peerplays sidechain plugin:  plugin_initialize()");
   my->plugin_initialize(options);
}

void peerplays_sidechain_plugin::plugin_startup()
{
   ilog("peerplays sidechain plugin:  plugin_startup()");
   my->plugin_startup();
}

son_id_type peerplays_sidechain_plugin::get_son_id()
{
   return my->get_son_id();
}

son_object peerplays_sidechain_plugin::get_son_object()
{
   return my->get_son_object();
}

bool peerplays_sidechain_plugin::is_active_son()
{
   return my->is_active_son();
}

std::map<chain::public_key_type, fc::ecc::private_key>& peerplays_sidechain_plugin::get_private_keys()
{
   return my->get_private_keys();
}

} } // graphene::peerplays_sidechain

