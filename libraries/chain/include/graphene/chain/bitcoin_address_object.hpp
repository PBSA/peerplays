#pragma once

#include <graphene/db/generic_index.hpp>
#include <graphene/chain/witness_object.hpp>

#include <sidechain/bitcoin_address.hpp>

namespace graphene { namespace chain {

class bitcoin_address_object : public abstract_object<bitcoin_address_object>
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = bitcoin_address_object_type;

      bitcoin_address_id_type get_id()const { return id; }
      // multisig m-of-n (m = 5). Address is valid before count of changed witnesses < 5
      bool valid() { return count_invalid_pub_key < 5; } // TODO: move to global_properties 

      std::string get_address() const { return address.get_address(); }

      void update_count_invalid_pub_key( const sidechain::accounts_keys& incoming_wit_keys ) {
         count_invalid_pub_key = incoming_wit_keys.size() - address.count_intersection( incoming_wit_keys );
      }

      account_id_type      owner;
      sidechain::btc_multisig_segwit_address address;
      uint8_t count_invalid_pub_key;
};

struct by_address;
struct by_owner;

typedef boost::multi_index_container<
   bitcoin_address_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_address>, const_mem_fun< bitcoin_address_object, std::string, &bitcoin_address_object::get_address > >,
      ordered_non_unique< tag<by_owner>, member< bitcoin_address_object, account_id_type, &bitcoin_address_object::owner > >
   >
> bitcoin_address_multi_index_container;
typedef generic_index<bitcoin_address_object, bitcoin_address_multi_index_container> bitcoin_address_index;

} }

FC_REFLECT_DERIVED( graphene::chain::bitcoin_address_object, (graphene::chain::object), (owner)(address)(count_invalid_pub_key) )

