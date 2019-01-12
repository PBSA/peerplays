#pragma once

#include <map>
#include <vector>
#include <string>
#include <fc/reflect/reflect.hpp>

#include <graphene/chain/account_object.hpp>

namespace sidechain {

using bytes = std::vector<char>;
using accounts_keys = std::map< graphene::chain::account_id_type, graphene::chain::public_key_type >;

enum class payment_type {
   NULLDATA,
   P2PK,
   P2PKH,
   P2SH,
   P2WPKH,
   P2WSH,
   P2SH_WPKH,
   P2SH_WSH
};

}

FC_REFLECT_ENUM( sidechain::payment_type, (NULLDATA)(P2PK)(P2PKH)(P2SH)(P2WPKH)(P2WSH)(P2SH_WPKH)(P2SH_WSH) );
