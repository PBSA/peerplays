#include <graphene/peerplays_sidechain/sidechain_net_manager.hpp>

#include <fc/log/logger.hpp>
#include <graphene/chain/son_wallet_object.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_handler_bitcoin.hpp>

namespace graphene { namespace peerplays_sidechain {

sidechain_net_manager::sidechain_net_manager(peerplays_sidechain_plugin& _plugin) :
   plugin(_plugin),
   database(_plugin.database())
{
}

sidechain_net_manager::~sidechain_net_manager() {
}

bool sidechain_net_manager::create_handler(peerplays_sidechain::sidechain_type sidechain, const boost::program_options::variables_map& options) {

   bool ret_val = false;

   switch (sidechain) {
      case sidechain_type::bitcoin: {
         std::unique_ptr<sidechain_net_handler> h = std::unique_ptr<sidechain_net_handler>(new sidechain_net_handler_bitcoin(plugin, options));
         net_handlers.push_back(std::move(h));
         ret_val = true;
         break;
      }
      default:
         assert(false);
   }

   return ret_val;
}

void sidechain_net_manager::recreate_primary_wallet() {
   for ( size_t i = 0; i < net_handlers.size(); i++ ) {
      net_handlers.at(i)->recreate_primary_wallet();
   }
}

} } // graphene::peerplays_sidechain

