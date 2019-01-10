#pragma once

#include <string>
#include <vector>
#include <fc/crypto/hex.hpp>

namespace sidechain {

std::vector<char> parse_hex( const std::string& str );

}
