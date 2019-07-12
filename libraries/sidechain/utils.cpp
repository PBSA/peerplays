#include <sidechain/utils.hpp>

namespace sidechain {

bytes parse_hex( const std::string& str )
{
    bytes vec( str.size() / 2 );
    fc::from_hex( str, vec.data(), vec.size() );
    return vec;
}

std::vector<bytes> get_pubkey_from_redeemScript( bytes script )
{
   FC_ASSERT( script.size() >= 37 );

   script.erase( script.begin() );
   script.erase( script.end() - 2, script.end() );

   std::vector<bytes> result;
   uint64_t count = script.size() / 34;
   for( size_t i = 0; i < count; i++ ) {
      result.push_back( bytes( script.begin() + (34 * i) + 1, script.begin() + (34 * (i + 1)) ) );
   }
   return result;
}

bytes public_key_data_to_bytes( const fc::ecc::public_key_data& key )
{
    bytes result;
    result.resize( key.size() );
    std::copy( key.begin(), key.end(), result.begin() );
    return result;
}

}
