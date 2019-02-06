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

struct bitcoin_transaction_confirmations
{
   bitcoin_transaction_confirmations() = default;

   bitcoin_transaction_confirmations( fc::sha256 trx_id ) : transaction_id( trx_id ) {}

   bool is_confirmed_and_not_used() const { return !used && confirmed; }

   fc::sha256 transaction_id;

   uint64_t count_block = 0;
   bool confirmed = false;
   bool missing = false;
   bool used = false;
};

struct by_hash;
struct by_confirmed_and_not_used;

using btc_tx_confirmations_index = boost::multi_index_container<bitcoin_transaction_confirmations,
   indexed_by<
      ordered_unique<tag<by_hash>, member<bitcoin_transaction_confirmations, fc::sha256, &bitcoin_transaction_confirmations::transaction_id>>, 
      ordered_non_unique<tag<by_confirmed_and_not_used>, const_mem_fun< bitcoin_transaction_confirmations, bool, &bitcoin_transaction_confirmations::is_confirmed_and_not_used >>
   >
>;

}