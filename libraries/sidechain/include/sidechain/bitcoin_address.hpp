#pragma once

#include <graphene/chain/protocol/types.hpp>
#include <sidechain/types.hpp>
#include <sidechain/utils.hpp>

using namespace graphene::chain;

namespace sidechain {

const bytes op = {0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f}; // OP_1 - OP_15

class bitcoin_address
{

public:

   bitcoin_address() = default;

   bitcoin_address( const std::string& addr ) : address( addr ), type( determine_type() ),
      raw_address( determine_raw_address( type ) ) {}

   payment_type get_type() const { return type; }

   std::string get_address() const { return address; }

   bytes get_raw_address() const { return raw_address; }

private:

   enum size_segwit_address { P2WSH = 32, P2WPKH = 20 };

   payment_type determine_type();

   bytes determine_raw_address( const payment_type& type );

   bool check_segwit_address( const size_segwit_address& size ) const;

   bool is_p2pk() const;

   bool is_p2wpkh() const;

   bool is_p2wsh() const;

   bool is_p2pkh() const;

   bool is_p2sh() const;

public:

   std::string address;

   payment_type type;

   bytes raw_address;

};

class btc_multisig_address : public bitcoin_address
{

public:

   btc_multisig_address() = default;

   btc_multisig_address( const size_t n_required, const accounts_keys& keys );

   size_t count_intersection( const accounts_keys& keys ) const;

   bytes get_redeem_script() const { return redeem_script; }

private:

   void create_redeem_script();

   void create_address();

public:

   enum address_types { MAINNET_SCRIPT = 5, TESTNET_SCRIPT = 196 };

   enum { OP_0 = 0x00, OP_EQUAL = 0x87, OP_HASH160 = 0xa9, OP_CHECKMULTISIG = 0xae };
   
   bytes redeem_script;

   size_t keys_required = 0;

   accounts_keys witnesses_keys;

};

// multisig segwit address (P2WSH)
// https://0bin.net/paste/nfnSf0HcBqBUGDto#7zJMRUhGEBkyh-eASQPEwKfNHgQ4D5KrUJRsk8MTPSa
class btc_multisig_segwit_address : public btc_multisig_address
{

public:

   btc_multisig_segwit_address() = default;

   btc_multisig_segwit_address( const size_t n_required, const accounts_keys& keys );

   bool operator==( const btc_multisig_segwit_address& addr ) const;

   bytes get_witness_script() const { return witness_script; }

private:

   void create_witness_script();

   void create_segwit_address();

   bytes get_address_bytes( const bytes& script_hash );

public:

   bytes witness_script;

};

}

FC_REFLECT( sidechain::bitcoin_address, (address)(type)(raw_address) );

FC_REFLECT_DERIVED( sidechain::btc_multisig_address, (sidechain::bitcoin_address),
   (redeem_script)(keys_required)(witnesses_keys) );

FC_REFLECT_DERIVED( sidechain::btc_multisig_segwit_address, (sidechain::btc_multisig_address), (witness_script) );
