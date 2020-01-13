#pragma once
#include <graphene/chain/protocol/types.hpp>
#include <graphene/peerplays_sidechain/defs.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    * @class son_wallet_object
    * @brief tracks information about a SON wallet.
    * @ingroup object
    */
   class son_wallet_object : public abstract_object<son_wallet_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = son_wallet_object_type;

         flat_map<peerplays_sidechain::sidechain_type, string> addresses;
   };

   struct by_sidechain_type;
   struct by_address;
   using son_wallet_multi_index_type = multi_index_container<
      son_wallet_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >
      >
   >;
   using son_wallet_index = generic_index<son_wallet_object, son_wallet_multi_index_type>;
} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::son_wallet_object, (graphene::db::object),
                    (addresses) )
