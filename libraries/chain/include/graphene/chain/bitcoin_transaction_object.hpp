#pragma once

#include <graphene/db/generic_index.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <sidechain/bitcoin_transaction.hpp>

namespace graphene { namespace chain {

class bitcoin_transaction_object : public abstract_object<bitcoin_transaction_object>
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = bitcoin_transaction_object_type;

      bitcoin_transaction_id_type get_id()const { return id; }

      fc::sha256                                  pw_vin;

      std::vector< fc::sha256 >                   vins;
      std::vector< info_for_vout_id_type >        vouts;

      sidechain::bitcoin_transaction              transaction;
      fc::sha256                                  transaction_id;

      uint64_t                                    fee_for_size;
};

struct by_transaction_id;

typedef boost::multi_index_container<
   bitcoin_transaction_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< object, object_id_type, &object::id > >,
      ordered_unique< tag< by_transaction_id >, member< bitcoin_transaction_object, fc::sha256, &bitcoin_transaction_object::transaction_id > >
   >
> bitcoin_transaction_multi_index_container;
typedef generic_index<bitcoin_transaction_object, bitcoin_transaction_multi_index_container> bitcoin_transaction_index;

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::bitcoin_transaction_object, (graphene::chain::object), (pw_vin)(vins)(vouts)(transaction)(transaction_id)(fee_for_size) )
