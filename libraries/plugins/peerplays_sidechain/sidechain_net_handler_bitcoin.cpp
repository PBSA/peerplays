#include <graphene/peerplays_sidechain/sidechain_net_handler_bitcoin.hpp>

#include <algorithm>
#include <thread>

#include <boost/algorithm/hex.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <fc/crypto/base64.hpp>
#include <fc/log/logger.hpp>
#include <fc/network/ip.hpp>

#include <graphene/chain/sidechain_address_object.hpp>

namespace graphene { namespace peerplays_sidechain {

// =============================================================================

bitcoin_rpc_client::bitcoin_rpc_client( std::string _ip, uint32_t _rpc, std::string _user, std::string _password ):
                      ip( _ip ), rpc_port( _rpc ), user( _user ), password( _password )
{
   authorization.key = "Authorization";
   authorization.val = "Basic " + fc::base64_encode( user + ":" + password );
}

std::string bitcoin_rpc_client::receive_full_block( const std::string& block_hash )
{
   fc::http::connection conn;
   conn.connect_to( fc::ip::endpoint( fc::ip::address( ip ), rpc_port ) );

   const auto url = "http://" + ip + ":" + std::to_string( rpc_port ) + "/rest/block/" + block_hash + ".json";

   const auto reply = conn.request( "GET", url );
   if ( reply.status != 200 )
      return "";

   ilog( "Receive Bitcoin block: ${hash}", ( "hash", block_hash ) );
   return std::string( reply.body.begin(), reply.body.end() );
}

//int32_t bitcoin_rpc_client::receive_confirmations_tx( const std::string& tx_hash )
//{
//   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", \"method\": \"getrawtransaction\", \"params\": [") +
//                      std::string("\"") + tx_hash + std::string("\"") + ", " + "true" + std::string("] }");
//
//   const auto reply = send_post_request( body );
//
//   if ( reply.status != 200 )
//      return 0;
//
//   const auto result = std::string( reply.body.begin(), reply.body.end() );
//
//   std::stringstream ss( result );
//   boost::property_tree::ptree tx;
//   boost::property_tree::read_json( ss, tx );
//
//   if( tx.count( "result" ) ) {
//      if( tx.get_child( "result" ).count( "confirmations" ) ) {
//         return tx.get_child( "result" ).get_child( "confirmations" ).get_value<int64_t>();
//      }
//   }
//   return 0;
//}

bool bitcoin_rpc_client::receive_mempool_entry_tx( const std::string& tx_hash )
{
   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", \"method\": \"getmempoolentry\", \"params\": [") +
                      std::string("\"") + tx_hash + std::string("\"") + std::string("] }");

   const auto reply = send_post_request( body );

   if ( reply.status != 200 )
        return false;

    return true;
}

uint64_t bitcoin_rpc_client::receive_estimated_fee()
{
   static const auto confirmation_target_blocks = 6;

   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"estimated_feerate\", \"method\": \"estimatesmartfee\", \"params\": [") +
                     std::to_string(confirmation_target_blocks) + std::string("] }");

   const auto reply = send_post_request( body );

   if( reply.status != 200 )
      return 0;

   std::stringstream ss( std::string( reply.body.begin(), reply.body.end() ) );
   boost::property_tree::ptree json;
   boost::property_tree::read_json( ss, json );

   if( json.count( "result" ) )
      if ( json.get_child( "result" ).count( "feerate" ) ) {
         auto feerate_str = json.get_child( "result" ).get_child( "feerate" ).get_value<std::string>();
         feerate_str.erase( std::remove( feerate_str.begin(), feerate_str.end(), '.' ), feerate_str.end() );
         return std::stoll( feerate_str );
      }
   return 0;
}

void bitcoin_rpc_client::send_btc_tx( const std::string& tx_hex )
{
   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"send_tx\", \"method\": \"sendrawtransaction\", \"params\": [") +
                     std::string("\"") + tx_hex + std::string("\"") + std::string("] }");

   const auto reply = send_post_request( body );

   if( reply.body.empty() )
      return;

   std::string reply_str( reply.body.begin(), reply.body.end() );

   std::stringstream ss(reply_str);
   boost::property_tree::ptree json;
   boost::property_tree::read_json( ss, json );

   if( reply.status == 200 ) {
      idump(( tx_hex ));
      return;
   } else if( json.count( "error" ) && !json.get_child( "error" ).empty() ) {
      const auto error_code = json.get_child( "error" ).get_child( "code" ).get_value<int>();
      if( error_code == -27 ) // transaction already in block chain
         return;

      wlog( "BTC tx is not sent! Reply: ${msg}", ("msg", reply_str) );
   }
}

bool bitcoin_rpc_client::connection_is_not_defined() const
{
   return ip.empty() || rpc_port == 0 || user.empty() || password.empty();
}

fc::http::reply bitcoin_rpc_client::send_post_request( std::string body )
{
   fc::http::connection conn;
   conn.connect_to( fc::ip::endpoint( fc::ip::address( ip ), rpc_port ) );

   const auto url = "http://" + ip + ":" + std::to_string( rpc_port );

   return conn.request( "POST", url, body, fc::http::headers{authorization} );
}

// =============================================================================

zmq_listener::zmq_listener( std::string _ip, uint32_t _zmq ): ip( _ip ), zmq_port( _zmq ), ctx( 1 ), socket( ctx, ZMQ_SUB ) {
   std::thread( &zmq_listener::handle_zmq, this ).detach();
}

std::vector<zmq::message_t> zmq_listener::receive_multipart() {
   std::vector<zmq::message_t> msgs;

   int32_t more;
   size_t more_size = sizeof( more );
   while ( true ) {
      zmq::message_t msg;
      socket.recv( &msg, 0 );
      socket.getsockopt( ZMQ_RCVMORE, &more, &more_size );

      if ( !more )
         break;
      msgs.push_back( std::move(msg) );
   }

   return msgs;
}

void zmq_listener::handle_zmq() {
   socket.setsockopt( ZMQ_SUBSCRIBE, "hashblock", 9 );
   //socket.setsockopt( ZMQ_SUBSCRIBE, "hashtx", 6 );
   //socket.setsockopt( ZMQ_SUBSCRIBE, "rawblock", 8 );
   //socket.setsockopt( ZMQ_SUBSCRIBE, "rawtx", 5 );
   socket.connect( "tcp://" + ip + ":" + std::to_string( zmq_port ) );

   while ( true ) {
      auto msg = receive_multipart();
      const auto header = std::string( static_cast<char*>( msg[0].data() ), msg[0].size() );
      const auto block_hash = boost::algorithm::hex( std::string( static_cast<char*>( msg[1].data() ), msg[1].size() ) );

      event_received( block_hash );
   }
}

// =============================================================================

sidechain_net_handler_bitcoin::sidechain_net_handler_bitcoin(std::shared_ptr<graphene::chain::database> db, const boost::program_options::variables_map& options) :
      sidechain_net_handler(db, options) {
   sidechain = sidechain_type::bitcoin;

   ip = options.at("bitcoin-node-ip").as<std::string>();
   zmq_port = options.at("bitcoin-node-zmq-port").as<uint32_t>();
   rpc_port = options.at("bitcoin-node-rpc-port").as<uint32_t>();
   rpc_user = options.at("bitcoin-node-rpc-user").as<std::string>();
   rpc_password = options.at("bitcoin-node-rpc-password").as<std::string>();

   fc::http::connection conn;
   try {
      conn.connect_to( fc::ip::endpoint( fc::ip::address( ip ), rpc_port ) );
   } catch ( fc::exception e ) {
      elog( "No BTC node running at ${ip} or wrong rpc port: ${port}", ("ip", ip) ("port", rpc_port) );
      FC_ASSERT( false );
   }

   listener = std::unique_ptr<zmq_listener>( new zmq_listener( ip, zmq_port ) );
   bitcoin_client = std::unique_ptr<bitcoin_rpc_client>( new bitcoin_rpc_client( ip, rpc_port, rpc_user, rpc_password ) );

   listener->event_received.connect([this]( const std::string& event_data ) {
      std::thread( &sidechain_net_handler_bitcoin::handle_event, this, event_data ).detach();
   } );
}

sidechain_net_handler_bitcoin::~sidechain_net_handler_bitcoin() {
}

bool sidechain_net_handler_bitcoin::connection_is_not_defined() const
{
   return listener->connection_is_not_defined() && bitcoin_client->connection_is_not_defined();
}

std::string sidechain_net_handler_bitcoin::create_multisignature_wallet( const std::vector<std::string> public_keys )
{
   return "";
}

std::string sidechain_net_handler_bitcoin::transfer( const std::string& from, const std::string& to, const uint64_t amount )
{
   return "";
}

std::string sidechain_net_handler_bitcoin::sign_transaction( const std::string& transaction )
{
   return "";
}

std::string sidechain_net_handler_bitcoin::send_transaction( const std::string& transaction )
{
   return "";
}

void sidechain_net_handler_bitcoin::handle_event( const std::string& event_data ) {
   ilog("peerplays sidechain plugin:  sidechain_net_handler_bitcoin::handle_event");
   ilog("                             event_data: ${event_data}", ("event_data", event_data));

   std::string block = bitcoin_client->receive_full_block( event_data );
   if( block != "" ) {
      const auto& vins = extract_info_from_block( block );

      const auto& sidechain_addresses_idx = database->get_index_type<sidechain_address_index>().indices().get<by_sidechain_and_address>();

      for( const auto& v : vins ) {
         const auto& addr_itr = sidechain_addresses_idx.find(std::make_tuple(sidechain_type::bitcoin, v.address));
         if ( addr_itr == sidechain_addresses_idx.end() )
            continue;

         sidechain_event_data sed;
         sed.sidechain = addr_itr->sidechain;
         sed.transaction_id = v.out.hash_tx;
         sed.from = "";
         sed.to = v.address;
         sed.amount = v.out.amount;
         sidechain_event_data_received(sed);
      }
   }
}

std::vector<info_for_vin> sidechain_net_handler_bitcoin::extract_info_from_block( const std::string& _block )
{
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
            info_for_vin vin;
            vin.out.hash_tx = tx.get_child("txid").get_value<std::string>();
            string amount = o.second.get_child( "value" ).get_value<std::string>();
            amount.erase(std::remove(amount.begin(), amount.end(), '.'), amount.end());
            vin.out.amount = std::stoll(amount);
            vin.out.n_vout = o.second.get_child( "n" ).get_value<uint32_t>();
            vin.address = address_base58;
            result.push_back( vin );
         }
      }
   }

   return result;
}

// =============================================================================

} } // graphene::peerplays_sidechain

