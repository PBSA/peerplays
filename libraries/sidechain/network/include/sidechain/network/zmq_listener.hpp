#pragma once

#include <string>
#include <vector>

#include <fc/signals.hpp>

#include <zmq.hpp>

namespace sidechain {

class zmq_listener
{

public:

   zmq_listener( std::string _ip, uint32_t _zmq );

   bool connection_is_not_defined() const { return zmq_port == 0; }

   fc::signal<void( const std::string& )> block_received;

private:

   void handle_zmq();
   std::vector<zmq::message_t> receive_multipart();

   std::string ip;
   uint32_t zmq_port;

   zmq::context_t ctx;
   zmq::socket_t socket;
};

}
