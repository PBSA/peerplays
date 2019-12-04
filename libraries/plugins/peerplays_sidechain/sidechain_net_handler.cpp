#include <graphene/peerplays_sidechain/sidechain_net_handler.hpp>

namespace graphene { namespace peerplays_sidechain {

sidechain_net_handler::sidechain_net_handler(const boost::program_options::variables_map& options) {
}

sidechain_net_handler::~sidechain_net_handler() {
}

std::vector<std::string> sidechain_net_handler::get_user_sidechain_address_mapping() {
   std::vector<std::string> result;

   switch (network) {
      case network::bitcoin:
         result.push_back("2N5aFW5WFaYZLuJWx9RGziHBdEMj9Zf8s3J");
         result.push_back("2MxAnE469fhhdvUqUB7daU997VSearb2mn7");
         result.push_back("2NAYptFvTU8vJ1pC7CxvVA9R7D3NdBJHpwL");
         result.push_back("2N9zPaLDfaJazUmVfr3wgn8BK75tid2kkzR");
         result.push_back("2NDN7cDH3E57E1B8TwTYvBgF7CndL4FTBPL");
         //result.push_back("2MzEmSiwrRzozxE6gfZ14LAyDHZ4DYP1zVG");
         //result.push_back("2NDCdm1WVJVCMWJzRaSSy9NDvpNKiqkbrMg");
         //result.push_back("2Mu2iz3Jfqjyv3hBQGQZSGmZGZxhJp91TNX");
         //result.push_back("2N1sFbwcn4QVbvjp7yRsN4mg4mBFbvC8gKM");
         //result.push_back("2NDmxr6ufBE7Zgdgq9hShF2grx2YPEiTyNy");

      default:
         assert(false);
   }

   return result;
}

} } // graphene::peerplays_sidechain

