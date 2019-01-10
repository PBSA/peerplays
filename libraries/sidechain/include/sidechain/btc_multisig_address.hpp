#pragma once

#include <vector>
#include <map>

#include <graphene/chain/protocol/types.hpp>
#include <sidechain/utils.hpp>

using namespace graphene::chain;

namespace sidechain {

const std::vector<char> op = {0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f}; // OP_1 - OP_15

class btc_multisig_address
{

public:

   btc_multisig_address() = default;

   btc_multisig_address( const size_t n_required, const std::map< account_id_type, public_key_type >& keys );

   size_t count_intersection( const std::map< account_id_type, public_key_type >& keys ) const;

   virtual std::vector< char > get_hex_address() { return address; }

   std::vector< char > get_redeem_script() { return redeem_script; }

private:

   void create_redeem_script();

   void create_address();

public:

   enum address_types { MAINNET_SCRIPT = 5, TESTNET_SCRIPT = 196 };

   enum { OP_0 = 0x00, OP_EQUAL = 0x87, OP_HASH160 = 0xa9, OP_CHECKMULTISIG = 0xae };

   std::vector< char > address;
   
   std::vector< char > redeem_script;

   size_t keys_required = 0;

   const std::map< account_id_type,  public_key_type > witnesses_keys;

};

// multisig segwit address (P2WSH)
// https://0bin.net/paste/nfnSf0HcBqBUGDto#7zJMRUhGEBkyh-eASQPEwKfNHgQ4D5KrUJRsk8MTPSa
class btc_multisig_segwit_address : public btc_multisig_address
{

public:

   btc_multisig_segwit_address() = default;

   btc_multisig_segwit_address( const size_t n_required, const std::map< account_id_type, public_key_type >& keys );

   bool operator==( const btc_multisig_segwit_address& addr ) const;

   std::vector< char > get_hex_address() override { return segwit_address; }
   
   std::string get_base58_address() { return base58_address; }

   std::vector< char > get_witness_script() { return witness_script; }

private:

   void create_witness_script();

   void create_segwit_address();

   std::vector<char> get_address_bytes( const std::vector<char>& script_hash );

public:

   std::vector< char > segwit_address;

   std::vector< char > witness_script;
   
   std::string base58_address;

};

}

FC_REFLECT( sidechain::btc_multisig_address, (address)(redeem_script)(keys_required)(witnesses_keys) );

FC_REFLECT_DERIVED( sidechain::btc_multisig_segwit_address, (sidechain::btc_multisig_address),
   (segwit_address)(witness_script)(base58_address) );
