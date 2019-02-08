#pragma once

#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/generic_index.hpp>
#include <sidechain/types.hpp>
#include <sidechain/bitcoin_address.hpp>

namespace graphene { namespace chain {

class info_for_used_vin_object : public abstract_object<info_for_used_vin_object>
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = info_for_used_vin_object_type;

      info_for_used_vin_id_type get_id()const { return id; }

      fc::sha256 identifier;

      sidechain::prev_out out;
      std::string address;
      sidechain::bytes script;
};

struct by_id;
struct by_identifier;

typedef boost::multi_index_container<
   info_for_used_vin_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< object, object_id_type, &object::id > >,
      ordered_unique<tag<by_identifier>, member<info_for_used_vin_object, fc::sha256, &info_for_used_vin_object::identifier>>
   >
> info_for_used_vin_multi_index_container;
typedef generic_index<info_for_used_vin_object, info_for_used_vin_multi_index_container> info_for_used_vin_index;

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::info_for_used_vin_object, (graphene::chain::object), (identifier)(out)(address)(script) )
