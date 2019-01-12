#include <sidechain/network/sidechain_net_manager.hpp>

#include <fc/network/http/connection.hpp>
#include <fc/network/ip.hpp>

#include <thread>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace sidechain {

sidechain_net_manager::sidechain_net_manager( graphene::chain::database* _db, std::string _ip, 
                                             uint32_t _zmq, uint32_t _rpc, std::string _user, std::string _password )
{
   initialize_manager(_db, _ip, _zmq, _rpc, _user, _password );
}

void sidechain_net_manager::initialize_manager( graphene::chain::database* _db, std::string _ip, 
                                             uint32_t _zmq, uint32_t _rpc, std::string _user, std::string _password )
{
   listener = std::unique_ptr<zmq_listener>( new zmq_listener( _ip, _zmq ) );
   bitcoin_client = std::unique_ptr<bitcoin_rpc_client>( new bitcoin_rpc_client( _ip, _rpc, _user, _password ) );
   db = std::unique_ptr<graphene::chain::database>( _db );

   fc::http::connection conn;
   try {
      conn.connect_to( fc::ip::endpoint( fc::ip::address( _ip ), _rpc ) );
   } catch ( fc::exception e ) {
      elog( "No BTC node running at ${ip} or wrong rpc port: ${port}", ("ip", _ip) ("port", _rpc) );
      FC_ASSERT( false );
   }

   listener->block_received.connect([this]( const std::string& block_hash ) {
      std::thread( &sidechain_net_manager::handle_block, this, block_hash).detach();
   } );
}

std::vector<info_for_vin> sidechain_net_manager::extract_info_from_block( const std::string& _block )
{

//    std::map<std::string, bool> test = { {"2MsmG481n4W4AC1QcPgBUJQrVRTrLNM2GpB", false},
//                                         {"2N2LkZG2Zp9eXGjSJzP9taR1gYdZBPzH7S7", false},
//                                         {"2N4MCW3XggAxs9C8Dh2vNcx7uH8zUqoLMjA", false},
//                                         {"2NEkNoDyQ9tyREFDAPehi9Jr2m6EFiTDxAS", false},
//                                         {"2N1rQcKr4F14L8dfnNiUVpc4LzcZTWN9Kpd", false} };

   std::stringstream ss( _block );
   boost::property_tree::ptree block;
   boost::property_tree::read_json( ss, block );

   std::vector<info_for_vin> result;

   for (const auto& tx_child : block.get_child("tx")) {
      const auto& tx = tx_child.second;

      for ( const auto& o : tx.get_child("vout") ) {
         const auto script = o.second.get_child("scriptPubKey");

         if( !script.count("addresses") ) continue;

         for (const auto& addr : script.get_child("addresses")) { // in which cases there can be more addresses?
            const auto address_base58 = addr.second.get_value<std::string>();

            // if( !test.count( address_base58 ) ) continue; // there is such an address in graphene

            info_for_vin vin;
            vin.out.hash_tx = tx.get_child("txid").get_value<std::string>();
            vin.out.amount = parse_amount( o.second.get_child( "value" ).get_value<std::string>() );
            vin.out.n_vout = o.second.get_child( "n" ).get_value<uint32_t>();
            vin.address = address_base58;
            result.push_back( vin );
         }
      }
   }

   return result;
}

void sidechain_net_manager::update_tx_infos( const std::string& block_hash )
{
   std::string block = bitcoin_client->receive_full_block( block_hash );
   if( block != "" ) {
      const auto& vins = extract_info_from_block( block );
      for( const auto& v : vins ) {
         db->i_w_info.insert_info_for_vin( prev_out{ v.out.hash_tx, v.out.n_vout, v.out.amount }, v.address );
      }
   }
}

void sidechain_net_manager::update_tx_approvals()
{
   
}

void sidechain_net_manager::update_estimated_fee()
{
   auto estimated_fee = bitcoin_client->receive_estimated_fee();
}

void sidechain_net_manager::send_btc_tx()
{
   FC_ASSERT( !bitcoin_client->connection_is_not_defined() );
}

bool sidechain_net_manager::connection_is_not_defined() const
{
   return listener->connection_is_not_defined() && bitcoin_client->connection_is_not_defined();
}

void sidechain_net_manager::handle_block( const std::string& block_hash )
{
   update_tx_approvals();
   update_estimated_fee();
   update_tx_infos( block_hash );
}

// Removes dot from amount output: "50.00000000"
inline uint64_t sidechain_net_manager::parse_amount(std::string raw) {
    raw.erase(std::remove(raw.begin(), raw.end(), '.'), raw.end());
    return std::stoll(raw);
}

}