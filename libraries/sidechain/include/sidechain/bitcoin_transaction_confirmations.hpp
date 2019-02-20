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

   bitcoin_transaction_confirmations( const fc::sha256& trx_id, const std::set<fc::sha256>& vins ) :
      id( count_id_tx_conf++ ), transaction_id( trx_id ), valid_vins( vins ) {}

   struct comparer {
      bool operator()( const bitcoin_transaction_confirmations& lhs, const bitcoin_transaction_confirmations& rhs ) const {
         if( lhs.is_confirmed_and_not_used() != rhs.is_confirmed_and_not_used() )
            return lhs.is_confirmed_and_not_used() < rhs.is_confirmed_and_not_used();
         return lhs.id < rhs.id;
      }
   };

   static uint64_t count_id_tx_conf;
   uint64_t id;

   bool is_confirmed_and_not_used() const { return !used && confirmed; }
   bool is_missing_and_not_used() const { return !used && missing; }

   fc::sha256 transaction_id;
   std::set<fc::sha256> valid_vins;

   uint64_t count_block = 0;
   bool confirmed = false;
   bool missing = false;
   bool used = false;
};

struct by_hash;
struct by_confirmed_and_not_used;
struct by_missing_and_not_used;

using btc_tx_confirmations_index = boost::multi_index_container<bitcoin_transaction_confirmations,
   indexed_by<
      ordered_unique<tag<by_hash>, member<bitcoin_transaction_confirmations, fc::sha256, &bitcoin_transaction_confirmations::transaction_id>>,
      ordered_non_unique<tag<by_confirmed_and_not_used>, identity< bitcoin_transaction_confirmations >, bitcoin_transaction_confirmations::comparer >,
      ordered_non_unique<tag<by_missing_and_not_used>, identity< bitcoin_transaction_confirmations >, bitcoin_transaction_confirmations::comparer >
   >
>;

}