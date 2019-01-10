#include <sidechain/network/bitcoin_rpc_client.hpp>

#include <sstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace sidechain {

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

int32_t bitcoin_rpc_client::receive_confirmations_tx( const std::string& tx_hash )
{
   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"curltest\", \"method\": \"getrawtransaction\", \"params\": [") +
                      std::string("\"") + tx_hash + std::string("\"") + ", " + "true" + std::string("] }");

   const auto reply = send_post_request( body );

   if ( reply.status != 200 )
      return -1;

   const auto result = std::string( reply.body.begin(), reply.body.end() );
   
   std::stringstream ss( result );
   boost::property_tree::ptree tx;
   boost::property_tree::read_json( ss, tx );
   if( tx.count( "result" ) ) {
      if( tx.get_child( "result" ).count( "confirmations" ) ) {
         return tx.get_child( "result" ).get_child( "confirmations" ).get_value<int64_t>();
      }
   }
   return 0;

}

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

std::string bitcoin_rpc_client::send_btc_tx( const std::string& tx_hex )
{
   const auto body = std::string("{\"jsonrpc\": \"1.0\", \"id\":\"send_tx\", \"method\": \"sendrawtransaction\", \"params\": [") +
                     std::string("\"") + tx_hex + std::string("\"") + std::string("] }");

   const auto reply = send_post_request( body );

   if( reply.body.empty() )
      return "";

   std::string reply_str( reply.body.begin(), reply.body.end() );

   return reply_str;
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

}