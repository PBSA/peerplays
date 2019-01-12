#include <sidechain/bitcoin_address.hpp>
#include <sstream>
#include <fc/crypto/base58.hpp>
#include <sidechain/segwit_addr.hpp>

namespace sidechain {

payment_type bitcoin_address::determine_type()
{
   if( is_p2pk() ) {
      return payment_type::P2PK;
   } else if( is_p2wpkh() ) {
      return payment_type::P2WPKH;
   } else if( is_p2wsh() ) {
      return payment_type::P2WSH;
   } else if( is_p2pkh() ) {
      return payment_type::P2PKH;
   } else if( is_p2sh() ) {
      return payment_type::P2SH;
   } else {
      return payment_type::NULLDATA;
   }
}

bytes bitcoin_address::determine_raw_address( const payment_type& type )
{
   bytes result;
   switch( type ) {
      case payment_type::P2PK : {
         result = parse_hex( address );
         break;
      }
      case payment_type::P2WPKH :
      case payment_type::P2WSH : {
         std::string prefix( address.compare(0,4,"bcrt") == 0 ? std::string( address.begin(), address.begin() + 4 ) :
                                                                std::string( address.begin(), address.begin() + 2 ) );
         const auto& decode_bech32 = segwit_addr::decode( prefix, address );
         result = bytes( decode_bech32.second.begin(), decode_bech32.second.end() );
         break;
      }
      case payment_type::P2SH_WPKH :
      case payment_type::P2SH_WSH :
      case payment_type::P2PKH :
      case payment_type::P2SH : {
         bytes hex_addr = fc::from_base58( address );
         result = bytes( hex_addr.begin() + 1, hex_addr.begin() + 21 );
         break;
      }
      case payment_type::NULLDATA : return result;
   }
   return result;
}

bool bitcoin_address::check_segwit_address( const size_segwit_address& size ) const {
   if( !address.compare(0,4,"bcrt") || !address.compare(0,2,"bc") || !address.compare(0,2,"tb") ) {
      std::string prefix( !address.compare(0,4,"bcrt") ? std::string(address.begin(), address.begin() + 4) :
                                                         std::string(address.begin(), address.begin() + 2) );

      const auto& decode_bech32 = segwit_addr::decode( prefix, address );
      
      if( decode_bech32.first == -1 || decode_bech32.second.size() != size ) {
         return false;
      }

      return true;
   }
   return false;
}

bool bitcoin_address::is_p2pk() const
{
   try {
      bool prefix = !address.compare(0,2,"02") || !address.compare(0,2,"03");
      if( address.size() == 66 && prefix ) {
         parse_hex( address );
         return true;
      }
   } catch( fc::exception e ) {
      return false;
   }
   return false;
}

bool bitcoin_address::is_p2wpkh() const
{
   return check_segwit_address( size_segwit_address::P2WPKH );
}

bool bitcoin_address::is_p2wsh() const
{
   return check_segwit_address( size_segwit_address::P2WSH );
}

bool bitcoin_address::is_p2pkh() const
{
   try {
      bytes hex_addr = fc::from_base58( address );
      if( hex_addr.size() == 25 && ( static_cast<unsigned char>( hex_addr[0] ) == 0x00 ||
                                     static_cast<unsigned char>( hex_addr[0] ) == 0x6f ) ) {
         return true;
      }
      return false;
   } catch( fc::exception e ) {
      return false;
   }
}

bool bitcoin_address::is_p2sh() const
{
   try {
      bytes hex_addr = fc::from_base58( address );
      if( hex_addr.size() == 25 && ( static_cast<unsigned char>( hex_addr[0] ) == 0x05 ||
                                     static_cast<unsigned char>( hex_addr[0] ) == 0xc4 ) ) {
         return true;
      }
      return false;
   } catch( fc::exception e ) {
      return false;
   }
}

btc_multisig_address::btc_multisig_address( const size_t n_required, const accounts_keys& keys ) :
   keys_required ( n_required ), witnesses_keys( keys )
{
   create_redeem_script();
   create_address();
   type = payment_type::P2SH;
}

size_t btc_multisig_address::count_intersection( const accounts_keys& keys ) const
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
   for( const auto& key : witnesses_keys ) {
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
   raw_address.clear();
   fc::sha256 hash256 = fc::sha256::hash( redeem_script.data(), redeem_script.size() );
   fc::ripemd160 hash160 = fc::ripemd160::hash( hash256.data(), hash256.data_size() );
   bytes temp_addr_hash( sidechain::parse_hex( hash160.str() ) );

   raw_address.push_back( OP_HASH160 );
   std::stringstream ss;
   ss << std::hex << temp_addr_hash.size();
   auto address_size_hex = sidechain::parse_hex( ss.str() );
   raw_address.insert( raw_address.end(), address_size_hex.begin(), address_size_hex.end() );
   raw_address.insert( raw_address.end(), temp_addr_hash.begin(), temp_addr_hash.end() );
   raw_address.push_back( OP_EQUAL );
}

btc_multisig_segwit_address::btc_multisig_segwit_address( const size_t n_required, const accounts_keys& keys ) :
   btc_multisig_address( n_required, keys )
{
   create_witness_script();
   create_segwit_address();
   type = payment_type::P2SH;
}

bool btc_multisig_segwit_address::operator==( const btc_multisig_segwit_address& addr ) const
{
   if( address != addr.address || redeem_script != addr.redeem_script ||
       witnesses_keys != addr.witnesses_keys || witness_script != addr.witness_script ||
       raw_address != addr.raw_address )
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
   
   raw_address = bytes(hash160.data(), hash160.data() + hash160.data_size() );
   address = fc::to_base58( get_address_bytes( raw_address ) );
}

bytes btc_multisig_segwit_address::get_address_bytes( const bytes& script_hash )
{
   bytes address_bytes( 1, TESTNET_SCRIPT ); // 1 byte version
   address_bytes.insert( address_bytes.end(), script_hash.begin(), script_hash.end() );
   fc::sha256 hash256 = fc::sha256::hash( fc::sha256::hash( address_bytes.data(), address_bytes.size() ) );
   address_bytes.insert( address_bytes.end(), hash256.data(), hash256.data() + 4 ); // 4 byte checksum
   
   return address_bytes;
}

}
