#include <sidechain/network/zmq_listener.hpp>
#include <boost/algorithm/hex.hpp>
#include <thread>

namespace sidechain {

zmq_listener::zmq_listener( std::string _ip, uint32_t _zmq, uint32_t _rpc ):
                            ip( _ip ), zmq_port( _zmq ), rpc_port( _rpc ), ctx( 1 ), socket( ctx, ZMQ_SUB )
{
   std::thread( &zmq_listener::handle_zmq, this ).detach();
}

std::vector<zmq::message_t> zmq_listener::receive_multipart()
{
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

void zmq_listener::handle_zmq()
{
   socket.setsockopt( ZMQ_SUBSCRIBE, "hashblock", 0 );
   socket.connect( "tcp://" + ip + ":" + std::to_string( zmq_port ) );

   fc::http::connection conn;
   try {
      conn.connect_to( fc::ip::endpoint( fc::ip::address( ip ), rpc_port ) );
   } catch ( fc::exception e ) {
      elog( "No BTC node running at ${ip} or wrong rpc port: ${port}", ("ip", ip) ("port", rpc_port) );
      FC_ASSERT( false );
   }

   while ( true ) {
      auto msg = receive_multipart();
      const auto header = std::string( static_cast<char*>( msg[0].data() ), msg[0].size() );
      const auto hash = boost::algorithm::hex( std::string( static_cast<char*>( msg[1].data() ), msg[1].size() ) );

      block_received( hash );
   }
}

}