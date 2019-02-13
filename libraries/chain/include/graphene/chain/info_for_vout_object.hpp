#pragma once

#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/generic_index.hpp>
#include <sidechain/types.hpp>
#include <sidechain/bitcoin_address.hpp>

namespace graphene { namespace chain {

class info_for_vout_object : public abstract_object<info_for_vout_object>
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = info_for_vout_object_type;

      struct comparer {
         bool operator()( const info_for_vout_object& lhs, const info_for_vout_object& rhs ) const {
            if( lhs.used != rhs.used )
               return lhs.used < rhs.used;
            return lhs.id < rhs.id;
         }
      };

      bool operator==( const info_for_vout_object& ifv ) const {
         return ( this->payer == ifv.payer ) &&
                ( this->address == ifv.address ) &&
                ( this->amount == ifv.amount );
      }

      info_for_vout_id_type get_id()const { return id; }

      account_id_type            payer;
      sidechain::bitcoin_address address;
      uint64_t                   amount;

      bool used = false;
};

struct by_created;
struct by_id_and_not_used;

typedef boost::multi_index_container<
   info_for_vout_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< object, object_id_type, &object::id > >,
      ordered_non_unique< tag< by_created >, member< info_for_vout_object, bool, &info_for_vout_object::used > >,
      ordered_non_unique<tag<by_id_and_not_used>, identity< info_for_vout_object >, info_for_vout_object::comparer >
   >
> info_for_vout_multi_index_container;
typedef generic_index<info_for_vout_object, info_for_vout_multi_index_container> info_for_vout_index;

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::info_for_vout_object, (graphene::chain::object), (payer)(address)(amount)(used) )

