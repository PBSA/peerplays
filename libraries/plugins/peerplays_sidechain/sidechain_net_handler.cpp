#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

#include <graphene/chain/sidechain_address_object.hpp>

#include <fc/log/logger.hpp>

namespace graphene { namespace peerplays_sidechain {

sidechain_net_handler::sidechain_net_handler(peerplays_sidechain_plugin& _plugin, const boost::program_options::variables_map& options) :
   plugin(_plugin),
   database(_plugin.database())
{
}

sidechain_net_handler::~sidechain_net_handler() {
}

graphene::peerplays_sidechain::sidechain_type sidechain_net_handler::get_sidechain() {
   return sidechain;
}

std::vector<std::string> sidechain_net_handler::get_sidechain_addresses() {
   std::vector<std::string> result;

   switch (sidechain) {
      case sidechain_type::bitcoin:
      {
         const auto& sidechain_addresses_idx = database.get_index_type<sidechain_address_index>();
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
   ilog( "  timestamp:                ${timestamp}", ( "timestamp", sed.timestamp ) );
   ilog( "  sidechain:                ${sidechain}", ( "sidechain", sed.sidechain ) );
   ilog( "  sidechain_uid:            ${uid}", ( "uid", sed.sidechain_uid ) );
   ilog( "  sidechain_transaction_id: ${transaction_id}", ( "transaction_id", sed.sidechain_transaction_id ) );
   ilog( "  sidechain_from:           ${from}", ( "from", sed.sidechain_from ) );
   ilog( "  sidechain_to:             ${to}", ( "to", sed.sidechain_to ) );
   ilog( "  sidechain_amount:         ${amount}", ( "amount", sed.sidechain_amount ) );
   ilog( "  peerplays_from:           ${peerplays_from}", ( "peerplays_from", sed.peerplays_from ) );
   ilog( "  peerplays_to:             ${peerplays_to}", ( "peerplays_to", sed.peerplays_to ) );

   if (!plugin.is_active_son()) {
      ilog( "  !!!                       SON is not active and not processing sidechain events...");
      return;
   }

   const chain::global_property_object& gpo = database.get_global_properties();

   son_wallet_transfer_create_operation op;
   op.payer = gpo.parameters.get_son_btc_account_id();
   op.timestamp = sed.timestamp;
   op.sidechain = sed.sidechain;
   op.sidechain_uid = sed.sidechain_uid;
   op.sidechain_transaction_id = sed.sidechain_transaction_id;
   op.sidechain_from = sed.sidechain_from;
   op.sidechain_to = sed.sidechain_to;
   op.sidechain_amount = sed.sidechain_amount;
   op.peerplays_from = sed.peerplays_from;
   op.peerplays_to = sed.peerplays_to;

   proposal_create_operation proposal_op;
   proposal_op.fee_paying_account = plugin.get_son_object().son_account;
   proposal_op.proposed_ops.push_back( op_wrapper( op ) );
   uint32_t lifetime = ( gpo.parameters.block_interval * gpo.active_witnesses.size() ) * 3;
   proposal_op.expiration_time = time_point_sec( database.head_block_time().sec_since_epoch() + lifetime );

   signed_transaction trx = plugin.database().create_signed_transaction(plugin.get_private_keys().begin()->second, proposal_op);
   try {
      database.push_transaction(trx);
   } catch(fc::exception e){
      ilog("sidechain_net_handler:  sending proposal for son wallet transfer create operation failed with exception ${e}",("e", e.what()));
   }
}

} } // graphene::peerplays_sidechain

