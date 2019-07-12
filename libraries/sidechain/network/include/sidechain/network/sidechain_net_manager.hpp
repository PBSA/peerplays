#pragma once

#include <sidechain/network/zmq_listener.hpp>
#include <sidechain/network/bitcoin_rpc_client.hpp>
#include <graphene/chain/database.hpp>

namespace sidechain {

class sidechain_net_manager 
{

public:

   sidechain_net_manager() {};

   sidechain_net_manager( graphene::chain::database* _db, std::string _ip, 
                         uint32_t _zmq, uint32_t _rpc, std::string _user, std::string _password );

   ~sidechain_net_manager() { db = nullptr; }

   void initialize_manager( graphene::chain::database* _db, std::string _ip, 
                           uint32_t _zmq, uint32_t _rpc, std::string _user, std::string _password );

   void update_tx_infos( const std::string& block_hash );

   void update_tx_approvals();

   void update_estimated_fee();

   void send_btc_tx( const sidechain::bitcoin_transaction& trx );

   bool connection_is_not_defined() const;

private:

   void handle_block( const std::string& block_hash );

   std::vector<info_for_vin> extract_info_from_block( const std::string& _block );

   void update_transaction_status( std::vector<fc::sha256> trx_for_check );

   std::set<fc::sha256> get_valid_vins( const std::string tx_hash );

   inline uint64_t parse_amount(std::string raw);

   std::unique_ptr<zmq_listener> listener;
   std::unique_ptr<bitcoin_rpc_client> bitcoin_client;
   graphene::chain::database* db;

};

}
