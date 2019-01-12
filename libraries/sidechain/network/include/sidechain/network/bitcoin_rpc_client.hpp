#pragma once

#include <string>
#include <fc/network/http/connection.hpp>

namespace sidechain {

class bitcoin_rpc_client
{

public:

   bitcoin_rpc_client( std::string _ip, uint32_t _rpc, std::string _user, std::string _password) ;

   std::string receive_full_block( const std::string& block_hash );

   int32_t receive_confirmations_tx( const std::string& tx_hash );

   bool receive_mempool_entry_tx( const std::string& tx_hash );

   uint64_t receive_estimated_fee();

   std::string send_btc_tx( const std::string& tx_hex );

   bool connection_is_not_defined() const;

private:

   fc::http::reply send_post_request( std::string body );

   std::string ip;
   uint32_t rpc_port;
   std::string user;
   std::string password;

   fc::http::header authorization;

};

}
