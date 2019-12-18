#include <graphene/peerplays_sidechain/peerplays_sidechain_plugin.hpp>

#include <fc/log/logger.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
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

private:
      peerplays_sidechain_plugin& plugin;

      bool config_ready_son;
      bool config_ready_bitcoin;

      std::unique_ptr<peerplays_sidechain::sidechain_net_manager> net_manager;
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
   } else {
      wlog("Haven't set up SON parameters");
      throw;
   }

   net_manager = std::unique_ptr<sidechain_net_manager>(new sidechain_net_manager(plugin.app().chain_database()));

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
   }

   if (config_ready_bitcoin) {
      ilog("Bitcoin sidechain handler running");
   }

   //if (config_ready_ethereum) {
   //   ilog("Ethereum sidechain handler running");
   //}
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

} } // graphene::peerplays_sidechain

