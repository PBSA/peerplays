#pragma once

#include <graphene/peerplays_sidechain/defs.hpp>
#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <vector>

#include <boost/program_options.hpp>

namespace graphene { namespace peerplays_sidechain {

class sidechain_net_manager {
public:
   sidechain_net_manager();
   virtual ~sidechain_net_manager();

   bool create_handler(peerplays_sidechain::network network, const boost::program_options::variables_map& options);
private:

   std::vector<std::unique_ptr<sidechain_net_handler>> net_handlers;

};

} } // graphene::peerplays_sidechain

