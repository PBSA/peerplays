#pragma once

#include <graphene/peerplays_sidechain/defs.hpp>
#include <graphene/peerplays_sidechain/peerplays_sidechain_plugin.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <vector>

#include <boost/program_options.hpp>

namespace graphene { namespace peerplays_sidechain {

class sidechain_net_manager {
public:
   sidechain_net_manager(peerplays_sidechain_plugin& _plugin);
   virtual ~sidechain_net_manager();

   bool create_handler(peerplays_sidechain::sidechain_type sidechain, const boost::program_options::variables_map& options);
   void recreate_primary_wallet();
private:
   peerplays_sidechain_plugin& plugin;
   graphene::chain::database& database;
   std::vector<std::unique_ptr<sidechain_net_handler>> net_handlers;

};

} } // graphene::peerplays_sidechain

