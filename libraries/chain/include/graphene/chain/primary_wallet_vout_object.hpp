#pragma once

#include <graphene/db/generic_index.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <sidechain/types.hpp>

namespace graphene { namespace chain {

class primary_wallet_vout_object : public abstract_object<primary_wallet_vout_object>
{
public:
   static const uint8_t space_id = protocol_ids;
   static const uint8_t type_id  = primary_wallet_vout_object_type;

   struct comparer {
      bool operator()(const primary_wallet_vout_object& lhs, const primary_wallet_vout_object& rhs) const;
   };
   
   primary_wallet_vout_id_type get_id() const { return id; }

   sidechain::prev_out      vout;
   fc::sha256               hash_id; // ( sha256(hash + n_vout) - 8 bytes ) + id_obj

   bool                     confirmed;
   bool                     used;
   
};

struct by_hash_id;

typedef boost::multi_index_container<
   primary_wallet_vout_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< object, object_id_type, &object::id > >,
      ordered_unique< tag< by_hash_id >, member< primary_wallet_vout_object, fc::uint256, &primary_wallet_vout_object::hash_id > >
   >
> primary_wallet_vout_multi_index_container;

typedef generic_index<primary_wallet_vout_object, primary_wallet_vout_multi_index_container> primary_wallet_vout_index;

} }

FC_REFLECT_DERIVED( graphene::chain::primary_wallet_vout_object, (graphene::chain::object), (vout)(hash_id)(confirmed)(used) )

