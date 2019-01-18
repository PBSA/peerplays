#pragma once

#include <graphene/db/generic_index.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <sidechain/types.hpp>

namespace graphene { namespace chain {

class sidechain_proposal_object : public abstract_object<sidechain_proposal_object>
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = sidechain_proposal_object_type;

      sidechain_proposal_id_type get_id()const { return id; }

      proposal_id_type          proposal_id;
      sidechain::sidechain_proposal_type   proposal_type;
};

struct by_proposal;
struct by_type;
typedef boost::multi_index_container<
   sidechain_proposal_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< object, object_id_type, &object::id > >,
      ordered_unique< tag< by_proposal >, member< sidechain_proposal_object, proposal_id_type, &sidechain_proposal_object::proposal_id > >,
      ordered_non_unique< tag< by_type >, member< sidechain_proposal_object, sidechain::sidechain_proposal_type, &sidechain_proposal_object::proposal_type > >
   >
> sidechain_multi_index_container;
typedef generic_index<sidechain_proposal_object, sidechain_multi_index_container> sidechain_proposal_index;

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::sidechain_proposal_object, (graphene::chain::object), (proposal_id)(proposal_type) )
