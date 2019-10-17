#pragma once
#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {
   using namespace graphene::db;

   /**
    * @class son_statistics_object
    * @ingroup object
    * @ingroup implementation
    *
    * This object contains regularly updated statistical data about an SON. It is provided for the purpose of
    * separating the SON transaction data that changes frequently from the SON object data that is mostly static.
    */
   class son_statistics_object : public graphene::db::abstract_object<son_statistics_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_son_statistics_object_type;

         son_id_type  owner;
         // Transactions signed since the last son payouts
         uint64_t txs_signed = 0;
   };

   /**
    * @class son_object
    * @brief tracks information about a SON account.
    * @ingroup object
    */
   class son_object : public abstract_object<son_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = son_object_type;

         account_id_type son_account;
         vote_id_type vote_id;
         uint64_t total_votes = 0;
         string url;
         vesting_balance_id_type deposit;
         public_key_type signing_key;
         vesting_balance_id_type pay_vb;
         son_statistics_id_type statistics;

         void pay_son_fee(share_type pay, database& db);
   };

   struct by_account;
   struct by_vote_id;
   using son_multi_index_type = multi_index_container<
      son_object,
      indexed_by<
         ordered_unique< tag<by_id>,
            member<object, object_id_type, &object::id>
         >,
         ordered_unique< tag<by_account>,
            member<son_object, account_id_type, &son_object::son_account>
         >,
         ordered_unique< tag<by_vote_id>,
            member<son_object, vote_id_type, &son_object::vote_id>
         >
      >
   >;
   using son_index = generic_index<son_object, son_multi_index_type>;

   using son_stats_multi_index_type = multi_index_container<
      son_statistics_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >
      >
   >;

   using son_stats_index = generic_index<son_statistics_object, son_stats_multi_index_type>;
} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::son_object, (graphene::db::object),
                    (son_account)(vote_id)(total_votes)(url)(deposit)(signing_key)(pay_vb) )

FC_REFLECT_DERIVED( graphene::chain::son_statistics_object,
                    (graphene::db::object),
                    (owner)
                    (txs_signed)
                  )
