#pragma once

#include <sidechain/types.hpp>
#include <sidechain/input_withdrawal_info.hpp>
#include <secp256k1.h>

namespace sidechain {

class bitcoin_transaction;

fc::sha256 get_signature_hash( const bitcoin_transaction& tx, const bytes& scriptPubKey, int64_t amount,
                               size_t in_index, int hash_type, bool is_witness );

std::vector<char> privkey_sign( const bytes& privkey, const fc::sha256 &hash, const secp256k1_context_t* context_sign = nullptr );

std::vector<bytes> sign_witness_transaction_part( const bitcoin_transaction& tx, const std::vector<bytes>& redeem_scripts,
                                                  const std::vector<uint64_t>& amounts, const bytes& privkey,
                                                  const secp256k1_context_t* context_sign = nullptr, int hash_type = 1 );

void sign_witness_transaction_finalize( bitcoin_transaction& tx, const std::vector<bytes>& redeem_scripts );

bool verify_sig( const bytes& sig, const bytes& pubkey, const bytes& msg, const secp256k1_context_t* context );

std::vector<std::vector<bytes>> sort_sigs( const bitcoin_transaction& tx, const std::vector<bytes>& redeem_scripts,
                                           const std::vector<uint64_t>& amounts, const secp256k1_context_t* context );

}
