#pragma once

#include <sidechain/types.hpp>
#include <fc/crypto/hex.hpp>

namespace sidechain {

bytes parse_hex( const std::string& str );

std::vector<bytes> get_pubkey_from_redeemScript( bytes script );

}
