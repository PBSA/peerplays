#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <graphene/chain/sidechain_address_object.hpp>

#include <fc/log/logger.hpp>

namespace graphene { namespace peerplays_sidechain {

sidechain_net_handler::sidechain_net_handler(std::shared_ptr<graphene::chain::database> db, const boost::program_options::variables_map& options) :
   database(db)
{
}

sidechain_net_handler::~sidechain_net_handler() {
}

std::vector<std::string> sidechain_net_handler::get_sidechain_addresses() {
   std::vector<std::string> result;

   switch (sidechain) {
      case sidechain_type::bitcoin:
      {
         const auto& sidechain_addresses_idx = database->get_index_type<sidechain_address_index>();
         const auto& sidechain_addresses_by_sidechain_idx = sidechain_addresses_idx.indices().get<by_sidechain>();
         const auto& sidechain_addresses_by_sidechain_range = sidechain_addresses_by_sidechain_idx.equal_range(sidechain);
         std::for_each(sidechain_addresses_by_sidechain_range.first, sidechain_addresses_by_sidechain_range.second,
               [&result] (const sidechain_address_object& sao) {
            result.push_back(sao.address);
         });
         break;
      }
      default:
         assert(false);
   }

   return result;
}

void sidechain_net_handler::sidechain_event_data_received(const sidechain_event_data& sed) {
   ilog( __FUNCTION__ );
   ilog( "sidechain_event_data:" );
   ilog( "  sidechain:      ${sidechain}", ( "sidechain", sed.sidechain ) );
   ilog( "  transaction_id: ${transaction_id}", ( "transaction_id", sed.transaction_id ) );
   ilog( "  from:           ${from}", ( "from", sed.from ) );
   ilog( "  to:             ${to}", ( "to", sed.to ) );
   ilog( "  amount:         ${amount}", ( "amount", sed.amount ) );
}

} } // graphene::peerplays_sidechain

