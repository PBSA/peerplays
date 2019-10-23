#pragma once

#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {

enum class son_proposal_type
{
    son_deregister_proposal
};

class son_proposal_object : public abstract_object<son_proposal_object>
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = son_proposal_object_type;

      son_proposal_id_type get_id()const { return id; }

      proposal_id_type          proposal_id;
      son_id_type               son_id;
      son_proposal_type         proposal_type;
};

struct by_proposal;
using son_proposal_multi_index_container = multi_index_container<
    son_proposal_object,
    indexed_by<
       ordered_unique< tag< by_id >, member< object, object_id_type, &object::id > >,
       ordered_unique< tag< by_proposal >, member< son_proposal_object, proposal_id_type, &son_proposal_object::proposal_id > >
    >
>;
using son_proposal_index = generic_index<son_proposal_object, son_proposal_multi_index_container>;

} } // graphene::chain

FC_REFLECT_ENUM(graphene::chain::son_proposal_type, (son_deregister_proposal) )

FC_REFLECT_DERIVED( graphene::chain::son_proposal_object, (graphene::chain::object), (proposal_id)(son_id)(proposal_type) )
