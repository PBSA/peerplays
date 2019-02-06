#pragma once

#include <map>
#include <vector>
#include <string>
#include <fc/reflect/reflect.hpp>

#include <graphene/chain/protocol/types.hpp>

namespace sidechain {

class bitcoin_transaction;

using bytes = std::vector<char>;
using accounts_keys = std::map< graphene::chain::account_id_type, graphene::chain::public_key_type >;
using info_for_vout = graphene::chain::info_for_vout_object;
using full_btc_transaction = std::pair<bitcoin_transaction, uint64_t>;

enum class payment_type
{
   NULLDATA,
   P2PK,
   P2PKH,
   P2SH,
   P2WPKH,
   P2WSH,
   P2SH_WPKH,
   P2SH_WSH
};

enum class sidechain_proposal_type
{
   ISSUE_BTC,
   SEND_BTC_TRANSACTION,
   RETURN_PBTC_BACK
};

struct prev_out
{
   std::string hash_tx;
   uint32_t n_vout;
   uint64_t amount;
};

}

FC_REFLECT_ENUM( sidechain::payment_type, (NULLDATA)(P2PK)(P2PKH)(P2SH)(P2WPKH)(P2WSH)(P2SH_WPKH)(P2SH_WSH) );
FC_REFLECT_ENUM( sidechain::sidechain_proposal_type, (ISSUE_BTC)(SEND_BTC_TRANSACTION)(RETURN_PBTC_BACK) );
FC_REFLECT( sidechain::prev_out, (hash_tx)(n_vout)(amount) );
