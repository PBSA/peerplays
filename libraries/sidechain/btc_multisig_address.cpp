#include <sidechain/btc_multisig_address.hpp>
#include <sstream>
#include <fc/crypto/base58.hpp>

namespace sidechain {

btc_multisig_address::btc_multisig_address( const size_t n_required, const std::map< account_id_type, public_key_type >& keys ) :
   keys_required ( n_required ), witnesses_keys( keys )
{
   create_redeem_script();
   create_address();
}

size_t btc_multisig_address::count_intersection( const std::map< account_id_type, public_key_type >& keys ) const
{
   FC_ASSERT( keys.size() > 0 );
   
   int intersections_count = 0;
   for( auto& key : keys ) {
      auto witness_key = witnesses_keys.find( key.first );
      if( witness_key == witnesses_keys.end() ) continue;
      if( key.second == witness_key->second )
         intersections_count++;
   }
   return intersections_count;
}

void btc_multisig_address::create_redeem_script()
{
   FC_ASSERT( keys_required > 0 );
   FC_ASSERT( keys_required < witnesses_keys.size() );
   redeem_script.clear();
   redeem_script.push_back( op[keys_required - 1] );
   for( auto& key : witnesses_keys ) {
      std::stringstream ss;
      ss << std::hex << key.second.key_data.size();
      auto key_size_hex = sidechain::parse_hex( ss.str() );
      redeem_script.insert( redeem_script.end(), key_size_hex.begin(), key_size_hex.end() );
      redeem_script.insert( redeem_script.end(), key.second.key_data.begin(), key.second.key_data.end() );
   }
   redeem_script.push_back( op[witnesses_keys.size() - 1] );
   redeem_script.push_back( OP_CHECKMULTISIG );
}

void btc_multisig_address::create_address()
{
   FC_ASSERT( redeem_script.size() > 0 );
   address.clear();
   fc::sha256 hash256 = fc::sha256::hash( redeem_script.data(), redeem_script.size() );
   fc::ripemd160 hash160 = fc::ripemd160::hash( hash256.data(), hash256.data_size() );
   std::vector<char> temp_addr_hash( sidechain::parse_hex( hash160.str() ) );

   address.push_back( OP_HASH160 );
   std::stringstream ss;
   ss << std::hex << temp_addr_hash.size();
   auto address_size_hex = sidechain::parse_hex( ss.str() );
   address.insert( address.end(), address_size_hex.begin(), address_size_hex.end() );
   address.insert( address.end(), temp_addr_hash.begin(), temp_addr_hash.end() );
   address.push_back( OP_EQUAL );
}

btc_multisig_segwit_address::btc_multisig_segwit_address( const size_t n_required, const std::map< account_id_type, public_key_type >& keys ) :
   btc_multisig_address( n_required, keys )
{
   create_witness_script();
   create_segwit_address();
}

bool btc_multisig_segwit_address::operator==( const btc_multisig_segwit_address& addr ) const
{
   if( address != addr.address || redeem_script != addr.redeem_script ||
       witnesses_keys != addr.witnesses_keys || witness_script != addr.witness_script ||
       segwit_address != addr.segwit_address || base58_address != addr.base58_address )
      return false;
   return true;
}

void btc_multisig_segwit_address::create_witness_script()
{
   const auto redeem_sha256 = fc::sha256::hash( redeem_script.data(), redeem_script.size() );
   witness_script.push_back( OP_0 );
   witness_script.push_back( 0x20 ); // PUSH_32
   witness_script.insert( witness_script.end(), redeem_sha256.data(), redeem_sha256.data() + redeem_sha256.data_size() );
}

void btc_multisig_segwit_address::create_segwit_address()
{
   fc::sha256 hash256 = fc::sha256::hash( witness_script.data(), witness_script.size() );
   fc::ripemd160 hash160 = fc::ripemd160::hash( hash256.data(), hash256.data_size() );
   
   segwit_address = std::vector<char>(hash160.data(), hash160.data() + hash160.data_size() );
   base58_address = fc::to_base58( get_address_bytes( segwit_address ) );
}

std::vector<char> btc_multisig_segwit_address::get_address_bytes( const std::vector<char>& script_hash )
{
   std::vector<char> address_bytes( 1, TESTNET_SCRIPT ); // 1 byte version
   address_bytes.insert( address_bytes.end(), script_hash.begin(), script_hash.end() );
   fc::sha256 hash256 = fc::sha256::hash( fc::sha256::hash( address_bytes.data(), address_bytes.size() ) );
   address_bytes.insert( address_bytes.end(), hash256.data(), hash256.data() + 4 ); // 4 byte checksum
   
   return address_bytes;
}

}
