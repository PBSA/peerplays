#pragma once

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <sidechain/thread_safe_index.hpp>
#include <fc/crypto/sha256.hpp>

#include <graphene/chain/protocol/types.hpp>

using boost::multi_index_container;
using namespace boost::multi_index;

namespace graphene { namespace chain { class database; } }

namespace sidechain {

struct btc_tx_confirmations
{
   btc_tx_confirmations() = default;

   btc_tx_confirmations( fc::sha256 trx_id ) : transaction_id( trx_id ) {}

   fc::sha256 transaction_id;

   uint64_t count_blocks = 0;
   bool confirmed = false;
};

struct by_hash;

using btc_tx_confirmations_index = boost::multi_index_container<btc_tx_confirmations,
   indexed_by<
      ordered_unique<tag<by_hash>, member<btc_tx_confirmations, fc::sha256, &btc_tx_confirmations::transaction_id>>
   >
>;

}