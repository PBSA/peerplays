#include <sidechain/network/sidechain_net_manager.hpp>
#include <thread>

namespace sidechain {

sidechain_net_manager::sidechain_net_manager( graphene::chain::database* _db, std::string _ip, 
                                             uint32_t _zmq, uint32_t _rpc, std::string _user, std::string _password ):
   listener( new zmq_listener( _ip, _zmq, _rpc ) ), bitcoin_client( new bitcoin_rpc_client( _ip, _rpc, _user, _password ) ), db( _db )
{
   listener->block_received.connect( [this]( const std::string& block_hash ) {
      std::thread( &sidechain_net_manager::handle_block, this, block_hash ).detach();
   });
}

void sidechain_net_manager::initialize_manager( graphene::chain::database* _db, std::string _ip, 
                                             uint32_t _zmq, uint32_t _rpc, std::string _user, std::string _password )
{
   db = std::unique_ptr<graphene::chain::database>( _db );
   listener = std::unique_ptr<zmq_listener>( new zmq_listener( _ip, _zmq, _rpc ) );
   bitcoin_client = std::unique_ptr<bitcoin_rpc_client>( new bitcoin_rpc_client( _ip, _rpc, _user, _password ) );

   listener->block_received.connect([this]( const std::string& block_hash ) {
      std::thread( &sidechain_net_manager::handle_block, this, block_hash).detach();
   } );
}


void sidechain_net_manager::update_tx_infos( const std::string& block_hash )
{
   std::string block = bitcoin_client->receive_full_block( block_hash );
   if( block != "" ) {
      
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

}