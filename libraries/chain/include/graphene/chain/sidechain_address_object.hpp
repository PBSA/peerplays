#pragma once
#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>

#include <graphene/peerplays_sidechain/defs.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    * @class sidechain_address_object
    * @brief tracks information about a sidechain addresses for user accounts.
    * @ingroup object
    */
   class sidechain_address_object : public abstract_object<sidechain_address_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = sidechain_address_object_type;

         account_id_type sidechain_address_account;
         graphene::peerplays_sidechain::sidechain_type sidechain;
         string address;
         string private_key;
         string public_key;

         sidechain_address_object() :
            sidechain(graphene::peerplays_sidechain::sidechain_type::bitcoin),
            address(""),
            private_key(""),
            public_key("") {}
   };

   struct by_account;
   struct by_sidechain;
   struct by_account_and_sidechain;
   struct by_sidechain_and_address;
   using sidechain_address_multi_index_type = multi_index_container<
      sidechain_address_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_non_unique< tag<by_account>,
            member<sidechain_address_object, account_id_type, &sidechain_address_object::sidechain_address_account>
         >,
         ordered_non_unique< tag<by_sidechain>,
            member<sidechain_address_object, peerplays_sidechain::sidechain_type, &sidechain_address_object::sidechain>
         >,
         ordered_unique< tag<by_account_and_sidechain>,
            composite_key<sidechain_address_object,
               member<sidechain_address_object, account_id_type, &sidechain_address_object::sidechain_address_account>,
               member<sidechain_address_object, peerplays_sidechain::sidechain_type, &sidechain_address_object::sidechain>
            >
         >,
         ordered_unique< tag<by_sidechain_and_address>,
            composite_key<sidechain_address_object,
               member<sidechain_address_object, peerplays_sidechain::sidechain_type, &sidechain_address_object::sidechain>,
               member<sidechain_address_object, std::string, &sidechain_address_object::address>
            >
         >
      >
   >;
   using sidechain_address_index = generic_index<sidechain_address_object, sidechain_address_multi_index_type>;

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::sidechain_address_object, (graphene::db::object),
                    (sidechain_address_account)(sidechain)(address)(private_key)(public_key) )
