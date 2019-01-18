#pragma once
#include <sidechain/bitcoin_address.hpp>
#include <sidechain/serialize.hpp>

namespace sidechain {

struct out_point {
   fc::sha256 hash;
   uint32_t n = 0;
   out_point() = default;
   out_point( fc::sha256 _hash, uint32_t _n ) : hash( _hash ), n( _n ) {}
};

struct tx_in {
   out_point prevout;
   bytes scriptSig;
   uint32_t nSequence = 0xffffffff;
   std::vector<bytes> scriptWitness;
};

struct tx_out {
   int64_t value = 0;
   bytes scriptPubKey;

   bool is_p2wsh() const;

   bool is_p2wpkh() const;

   bool is_p2pkh() const;

   bool is_p2sh() const;

   bool is_p2pk() const;

   bytes get_data_or_script() const;
};

struct bitcoin_transaction {
    int32_t nVersion = 1;
    std::vector<tx_in> vin;
    std::vector<tx_out> vout;
    uint32_t nLockTime = 0;

    fc::sha256 get_hash() const;
    fc::sha256 get_txid() const;
    size_t get_vsize() const;
};

class bitcoin_transaction_builder
{

public:

   bitcoin_transaction_builder() = default;

   bitcoin_transaction_builder( const bitcoin_transaction _tx ) : tx( _tx ) {}

   void set_version( int32_t version );

   void set_locktime( uint32_t lock_time );

   void add_in( payment_type type, const fc::sha256& txid, uint32_t n_out,
      const bytes& script_code, bool front = false, uint32_t sequence = 0xffffffff );
   
   void add_in( payment_type type, tx_in txin, const bytes& script_code, bool front = false );

   void add_out( payment_type type, int64_t amount, const std::string& base58_address, bool front = false );

   void add_out( payment_type type, int64_t amount, const fc::ecc::public_key_data& pubkey, bool front = false );

   void add_out( payment_type type, int64_t amount, const bytes& script_code, bool front = false );

   void add_out_all_type( const uint64_t& amount, const bitcoin_address& address, bool front = false );

   bitcoin_transaction get_transaction() const { return tx; }

private:

   inline bool is_payment_to_pubkey( payment_type type );

   bytes get_script_pubkey( payment_type type, const bytes& script_code );

   bitcoin_transaction tx;

};

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
inline void unpack( Stream& s, bitcoin_transaction& tx ) {
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

inline std::vector<char> pack( const bitcoin_transaction& v, bool with_witness = true ) {
   fc::datastream<size_t> ps;
   pack( ps, v, with_witness );
   std::vector<char> vec( ps.tellp() );

   if( !vec.empty() ) {
      fc::datastream<char*>  ds( vec.data(), size_t(vec.size()) );
      pack( ds, v, with_witness );
   }
   return vec;
}

inline bitcoin_transaction unpack( const std::vector<char>& s )
{ try  {
   bitcoin_transaction tmp;
   if( !s.empty() ) {
      fc::datastream<const char*>  ds( s.data(), size_t(s.size()) );
      unpack(ds, tmp);
   }
   return tmp;
} FC_RETHROW_EXCEPTIONS( warn, "error unpacking ${type}", ("type","transaction" ) ) }

}

FC_REFLECT( sidechain::out_point, (hash)(n) )
FC_REFLECT( sidechain::tx_in, (prevout)(scriptSig)(nSequence)(scriptWitness) )
FC_REFLECT( sidechain::tx_out, (value)(scriptPubKey) )
FC_REFLECT( sidechain::bitcoin_transaction, (nVersion)(vin)(vout)(nLockTime) )
