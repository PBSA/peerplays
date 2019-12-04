#include <graphene/peerplays_sidechain/sidechain_net_manager.hpp>

#include <fc/log/logger.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_handler_bitcoin.hpp>

namespace graphene { namespace peerplays_sidechain {

sidechain_net_manager::sidechain_net_manager() {
   ilog(__FUNCTION__);
}

sidechain_net_manager::~sidechain_net_manager() {
   ilog(__FUNCTION__);
}

bool sidechain_net_manager::create_handler(peerplays_sidechain::network network, const boost::program_options::variables_map& options) {
   ilog(__FUNCTION__);

   bool ret_val = false;

   switch (network) {
      case network::bitcoin: {
          std::unique_ptr<sidechain_net_handler> h = std::unique_ptr<sidechain_net_handler>(new sidechain_net_handler_bitcoin(options));
          net_handlers.push_back(std::move(h));
          ret_val = true;
      }
      default:
         assert(false);
   }

   return ret_val;
}

} } // graphene::peerplays_sidechain

