#pragma once

#include <sidechain/types.hpp>
#include <sidechain/bitcoin_transaction.hpp>

namespace sidechain {

inline void write_compact_size( bytes& vec, size_t size )
{
   bytes sb;
   sb.reserve( 2 );
   if ( size < 253 ) {
      sb.insert( sb.end(), static_cast<uint8_t>( size ) );
   } else if ( size <= std::numeric_limits<unsigned short>::max() ) {
      uint16_t tmp = htole16( static_cast<uint16_t>( size ) );
      sb.insert( sb.end(), static_cast<uint8_t>( 253 ) );
      sb.insert( sb.end(), reinterpret_cast<const char*>( tmp ), reinterpret_cast<const char*>( tmp ) + sizeof( tmp ) );
   } else if ( size <= std::numeric_limits<unsigned int>::max() ) {
      uint32_t tmp = htole32( static_cast<uint32_t>( size ) );
      sb.insert( sb.end(), static_cast<uint8_t>( 254 ) );
      sb.insert( sb.end(), reinterpret_cast<const char*>( tmp ), reinterpret_cast<const char*>( tmp ) + sizeof( tmp ) );
   } else {
      uint64_t tmp = htole64( static_cast<uint64_t>( size ) );
      sb.insert( sb.end(), static_cast<uint8_t>( 255 ) );
      sb.insert( sb.end(), reinterpret_cast<const char*>( tmp ), reinterpret_cast<const char*>( tmp ) + sizeof( tmp ) );
   }
   vec.insert( vec.end(), sb.begin(), sb.end() );
}

template<typename Stream>
inline void pack_compact_size(Stream& s, size_t size)
{
   if ( size < 253 ) {
      fc::raw::pack( s, static_cast<uint8_t>( size ) );
   } else if ( size <= std::numeric_limits<unsigned short>::max() ) {
      fc::raw::pack( s, static_cast<uint8_t>( 253 ) );
      fc::raw::pack( s, htole16( static_cast<uint16_t>( size ) ) );
   } else if ( size <= std::numeric_limits<unsigned int>::max() ) {
      fc::raw::pack( s, static_cast<uint8_t>( 254 ) );
      fc::raw::pack( s, htole32(static_cast<uint32_t>( size ) ) );
   } else {
      fc::raw::pack( s, static_cast<uint8_t>( 255 ) );
      fc::raw::pack( s, htole64( static_cast<uint64_t>( size ) ) );
   }
}

template<typename Stream>
inline uint64_t unpack_compact_size( Stream& s )
{
   uint8_t size;
   uint64_t size_ret;

   fc::raw::unpack( s, size );

   if (size < 253) {
      size_ret = size;
   } else if ( size == 253 ) {
      uint16_t tmp;
      fc::raw::unpack( s, tmp );
      size_ret = le16toh( tmp );
      if ( size_ret < 253 )
         FC_THROW_EXCEPTION( fc::parse_error_exception, "non-canonical unpack_compact_size()" );
   } else if ( size == 254 ) {
      uint32_t tmp;
      fc::raw::unpack( s, tmp );
      size_ret = le32toh( tmp );
      if ( size_ret < 0x10000u )
         FC_THROW_EXCEPTION( fc::parse_error_exception, "non-canonical unpack_compact_size()" );
   } else {
      uint32_t tmp;
      fc::raw::unpack( s, tmp );
      size_ret = le64toh( tmp );
      if ( size_ret < 0x100000000ULL )
         FC_THROW_EXCEPTION( fc::parse_error_exception, "non-canonical unpack_compact_size()" );
   }

   if ( size_ret > 0x08000000 )
      FC_THROW_EXCEPTION( fc::parse_error_exception, "unpack_compact_size(): size too large" );

   return size_ret;
}

template<typename Stream>
inline void pack( Stream& s, const std::vector<char>& v )
{
   pack_compact_size( s, v.size() );
   if ( !v.empty() )
      s.write( v.data(), v.size() );
}

template<typename Stream>
inline void unpack( Stream& s, std::vector<char>& v )
{
   const auto size = unpack_compact_size( s );
   v.resize( size );
   if ( size )
      s.read( v.data(), size );
}

template<typename Stream, typename T>
inline void pack( Stream& s, const T& val )
{
   fc::raw::pack( s, val );
}

template<typename Stream, typename T>
inline void unpack( Stream& s, T& val )
{
   fc::raw::unpack( s, val );
}

template<typename Stream>
inline void pack( Stream& s, const out_point& op )
{
   fc::sha256 reversed( op.hash );
   std::reverse( reversed.data(), reversed.data() + reversed.data_size() );
   s.write( reversed.data(), reversed.data_size() );
   pack( s, op.n );
}

template<typename Stream>
inline void unpack( Stream& s, out_point& op )
{
   uint64_t hash_size = op.hash.data_size();
   std::unique_ptr<char[]> hash_data( new char[hash_size] );
   s.read( hash_data.get(), hash_size );
   std::reverse( hash_data.get(), hash_data.get() + hash_size );

   op.hash = fc::sha256( hash_data.get(), hash_size );
   unpack( s, op.n );
}

template<typename Stream>
inline void pack( Stream& s, const tx_in& in )
{
   pack( s, in.prevout );
   pack( s, in.scriptSig );
   pack( s, in.nSequence );
}

template<typename Stream>
inline void unpack( Stream& s, tx_in& in )
{
   unpack( s, in.prevout );
   unpack( s, in.scriptSig );
   unpack( s, in.nSequence );
}

template<typename Stream>
inline void pack( Stream& s, const tx_out& out )
{
   pack( s, out.value );
   pack( s, out.scriptPubKey );
}

template<typename Stream>
inline void unpack( Stream& s, tx_out& out )
{
   unpack( s, out.value );
   unpack( s, out.scriptPubKey );
}

template<typename Stream>
inline void pack( Stream& s, const bitcoin_transaction& tx, bool with_witness = true )
{
   uint8_t flags = 0;

   if ( with_witness ) {
      for ( const auto& in : tx.vin ) {
         if ( !in.scriptWitness.empty() ) {
            flags |= 1;
            break;
         }
      }
   }

   pack( s, tx.nVersion );

   if ( flags ) {
      pack_compact_size( s, 0 );
      pack( s, flags );
   }

   pack_compact_size( s, tx.vin.size() );
   for ( const auto& in : tx.vin )
      pack( s, in );

   pack_compact_size( s, tx.vout.size() );
   for ( const auto& out : tx.vout )
      pack( s, out );

   if ( flags & 1 ) {
      for ( const auto in : tx.vin ) {
         pack_compact_size( s, in.scriptWitness.size() );
         for ( const auto& sc : in.scriptWitness )
            pack( s, sc );
      }
   }

   pack( s, tx.nLockTime );
}

template<typename Stream>
inline void unpack( Stream& s, bitcoin_transaction& tx )
{
   uint8_t flags = 0;

   unpack( s, tx.nVersion );

   auto vin_size = unpack_compact_size( s );
   if ( vin_size == 0 ) {
      unpack( s, flags );
      vin_size = unpack_compact_size( s );
   }

   tx.vin.reserve( vin_size );
   for ( size_t i = 0; i < vin_size; i++ ) {
      tx_in in;
      unpack( s, in );
      tx.vin.push_back( in );
   }

   const auto vout_size = unpack_compact_size( s );
   tx.vout.reserve( vout_size );
   for ( size_t i = 0; i < vout_size; i++ ) {
      tx_out out;
      unpack( s, out );
      tx.vout.push_back( out );
   }

   if ( flags & 1 ) {
      for ( auto& in : tx.vin ) {
         uint64_t stack_size = unpack_compact_size( s );
         in.scriptWitness.reserve( stack_size );
         for ( uint64_t i = 0; i < stack_size; i++ ) {
            std::vector<char> script;
            unpack( s, script );
            in.scriptWitness.push_back( script );
         }
      }
   }

   unpack( s, tx.nLockTime );
}

inline std::vector<char> pack( const bitcoin_transaction& v, bool with_witness = true )
{
   fc::datastream<size_t> ps;
   pack( ps, v, with_witness );
   std::vector<char> vec( ps.tellp() );

   if( !vec.empty() ) {
      fc::datastream<char*>  ds( vec.data(), size_t( vec.size() ) );
      pack( ds, v, with_witness );
   }
   return vec;
}

inline bitcoin_transaction unpack( const std::vector<char>& s )
{ try  {
   bitcoin_transaction tmp;
   if( !s.empty() ) {
      fc::datastream<const char*>  ds( s.data(), size_t( s.size() ) );
      unpack(ds, tmp);
   }
   return tmp;
} FC_RETHROW_EXCEPTIONS( warn, "error unpacking ${type}", ("type","transaction" ) ) }

template<typename Stream>
inline void pack_tx_signature( Stream& s, const std::vector<char>& scriptPubKey, const bitcoin_transaction& tx, unsigned int in_index, int hash_type )
{
   pack( s, tx.nVersion );

   pack_compact_size( s, tx.vin.size() );
   for ( size_t i = 0; i < tx.vin.size(); i++ ) {
      const auto& in = tx.vin[i];
      pack( s, in.prevout );
      if ( i == in_index )
         pack( s, scriptPubKey );
      else
         pack_compact_size( s, 0 ); // Blank signature
      pack( s, in.nSequence );
   }

   pack_compact_size( s, tx.vout.size() );
   for ( const auto& out : tx.vout )
      pack( s, out );

   pack( s, tx.nLockTime );
   pack( s, hash_type );
}

template<typename Stream>
inline void pack_tx_witness_signature( Stream& s, const std::vector<char>& scriptCode, const bitcoin_transaction& tx, unsigned int in_index, int64_t amount, int hash_type )
{

   fc::sha256 hash_prevouts;
   fc::sha256 hash_sequence;
   fc::sha256 hash_output;

   {
      fc::datastream<size_t> ps;
      for ( const auto in : tx.vin )
         pack( ps, in.prevout );

      std::vector<char> vec( ps.tellp() );
      if ( vec.size() ) {
         fc::datastream<char*> ds( vec.data(), size_t( vec.size() ) );
         for ( const auto in : tx.vin )
            pack( ds, in.prevout );
      }

      hash_prevouts = fc::sha256::hash( fc::sha256::hash( vec.data(), vec.size() ) );
   }

   {
      fc::datastream<size_t> ps;
      for ( const auto in : tx.vin )
         pack( ps, in.nSequence );

      std::vector<char> vec( ps.tellp() );
      if ( vec.size() ) {
         fc::datastream<char*> ds( vec.data(), size_t( vec.size() ) );
         for ( const auto in : tx.vin )
            pack( ds, in.nSequence );
      }

      hash_sequence = fc::sha256::hash( fc::sha256::hash( vec.data(), vec.size() ) );
   };

   {
      fc::datastream<size_t> ps;
      for ( const auto out : tx.vout )
         pack( ps, out );

      std::vector<char> vec( ps.tellp() );
      if ( vec.size() ) {
         fc::datastream<char*> ds( vec.data(), size_t( vec.size() ) );
         for ( const auto out : tx.vout )
            pack( ds, out );
      }

      hash_output = fc::sha256::hash( fc::sha256::hash( vec.data(), vec.size() ) );
   }

   pack( s, tx.nVersion );
   pack( s, hash_prevouts );
   pack( s, hash_sequence );

   pack( s, tx.vin[in_index].prevout );
   pack( s, scriptCode );
   pack( s, amount );
   pack( s, tx.vin[in_index].nSequence );

   pack( s, hash_output );
   pack( s, tx.nLockTime );
   pack( s, hash_type );
}

}
