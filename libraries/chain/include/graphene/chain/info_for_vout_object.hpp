#pragma once

#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/generic_index.hpp>
#include <sidechain/types.hpp>

namespace graphene { namespace chain {

class info_for_vout_object : public abstract_object<info_for_vout_object>
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = info_for_vout_object_type;

      struct comparer {
         bool operator()( const info_for_vout_object& lhs, const info_for_vout_object& rhs ) const {
            if( lhs.created != rhs.created )
               return lhs.created < rhs.created;
            return lhs.id < rhs.id;
         }
      };

      info_for_vout_id_type get_id()const { return id; }

      account_id_type         payer;
      sidechain::payment_type addr_type;
      std::string             data;
      uint64_t                amount;

      bool created = false;
};

struct by_created;
struct by_id_and_not_created;

typedef boost::multi_index_container<
   info_for_vout_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< object, object_id_type, &object::id > >,
      ordered_non_unique< tag< by_created >, member< info_for_vout_object, bool, &info_for_vout_object::created > >,
      ordered_non_unique<tag<by_id_and_not_created>, identity< info_for_vout_object >, info_for_vout_object::comparer >
   >
> info_for_vout_multi_index_container;
typedef generic_index<info_for_vout_object, info_for_vout_multi_index_container> info_for_vout_index;

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::info_for_vout_object, (graphene::chain::object), (payer)(addr_type)(data)(amount)(created) )

