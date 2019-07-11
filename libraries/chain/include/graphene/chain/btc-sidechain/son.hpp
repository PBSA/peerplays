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

         account_id_type                  son_member_account;
         vote_id_type                     vote_id;
         uint64_t                         total_votes = 0;
         string                           url;

         fc::optional<fc::time_point_sec> time_of_deleting;

         struct comparer {
            bool operator()( const son_member_object& lhs, const son_member_object& rhs ) const {
               uint8_t lhs_valid = static_cast<uint8_t>( lhs.time_of_deleting.valid() );
               uint8_t rhs_valid = static_cast<uint8_t>( rhs.time_of_deleting.valid() );
               if( lhs_valid != rhs_valid )
                  return lhs_valid > rhs_valid;
               else if ( lhs_valid && lhs_valid == rhs_valid )
                  return *lhs.time_of_deleting < *lhs.time_of_deleting;
               return lhs.id < rhs.id;
            }
         };

         son_member_id_type get_id()const { return id; }

   };

   struct by_account;
   struct by_vote_id;
   struct by_deleted;
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
         >,
         ordered_non_unique< tag<by_deleted>,
            identity<son_member_object>, son_member_object::comparer>
      >
   >;
   using son_member_index = generic_index<son_member_object, son_member_multi_index_type>;
} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::son_member_object, (graphene::db::object),
                    (son_member_account)(vote_id)(total_votes)(url)(time_of_deleting) )
