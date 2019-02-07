#pragma once
#include <sidechain/bitcoin_address.hpp>

namespace sidechain {

struct out_point {
   fc::sha256 hash;
   uint32_t n = 0;
   out_point() = default;
   out_point( fc::sha256 _hash, uint32_t _n ) : hash( _hash ), n( _n ) {}
   bool operator==( const out_point& op ) const;
};

struct tx_in {

   bool operator==( const tx_in& ti ) const;

   out_point prevout;
   bytes scriptSig;
   uint32_t nSequence = 0xffffffff;
   std::vector<bytes> scriptWitness;
};

struct tx_out {
   int64_t value = 0;
   bytes scriptPubKey;

   bool operator==( const tx_out& to ) const;

   bool is_p2wsh() const;

   bool is_p2wpkh() const;

   bool is_p2pkh() const;

   bool is_p2sh() const;

   bool is_p2pk() const;

   bytes get_data_or_script() const;
};

struct bitcoin_transaction {

   bool operator!=( const bitcoin_transaction& bt ) const;

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

}

FC_REFLECT( sidechain::out_point, (hash)(n) )
FC_REFLECT( sidechain::tx_in, (prevout)(scriptSig)(nSequence)(scriptWitness) )
FC_REFLECT( sidechain::tx_out, (value)(scriptPubKey) )
FC_REFLECT( sidechain::bitcoin_transaction, (nVersion)(vin)(vout)(nLockTime) )
