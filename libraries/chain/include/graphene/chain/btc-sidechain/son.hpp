#pragma once
#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    * @class son_member_object
    * @brief tracks information about a son_member account.
    * @ingroup object
    */
   class son_member_object : public abstract_object<son_member_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = son_member_object_type;

         account_id_type  son_member_account;
         vote_id_type     vote_id;
         uint64_t         total_votes = 0;
         string           url;
   };

   struct by_account;
   struct by_vote_id;
   using son_member_multi_index_type = multi_index_container<
      son_member_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_unique< tag<by_account>,
            member<son_member_object, account_id_type, &son_member_object::son_member_account>
         >,
         ordered_unique< tag<by_vote_id>,
            member<son_member_object, vote_id_type, &son_member_object::vote_id>
         >
      >
   >;
   using son_member_index = generic_index<son_member_object, son_member_multi_index_type>;
} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::son_member_object, (graphene::db::object),
                    (son_member_account)(vote_id)(total_votes)(url) )
