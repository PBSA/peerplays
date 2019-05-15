#pragma once
#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/generic_index.hpp>
#include <graphene/chain/protocol/asset.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {

   class contract_balance_object : public graphene::db::abstract_object<contract_balance_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = contract_balance_object_type;

         /// Contract id owner
         contract_id_type   owner;
         /// Asset id of balance
         asset_id_type      asset_type;
         /// Amount of balance
         share_type         balance;

         asset get_balance()const { return asset(balance, asset_type); }
         void  adjust_balance(const asset& delta);
   };

   class contract_object : public graphene::db::abstract_object<contract_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = contract_object_type;

         /// See `contract_statistics_object`
         contract_statistics_id_type statistics;
         /// Allowed assets for this contract
         std::set<uint64_t> allowed_assets;
         /// Suicided or not
         bool suicided = false;

         // eosio::chain::name name;

         contract_id_type get_id()const { return id; }
   };
   
   struct by_contract_asset;
   struct by_asset_balance;
   /**
    * @ingroup object_index
    */
   typedef multi_index_container<
      contract_balance_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_unique< tag<by_contract_asset>,
            composite_key<
               contract_balance_object,
               member<contract_balance_object, contract_id_type, &contract_balance_object::owner>,
               member<contract_balance_object, asset_id_type, &contract_balance_object::asset_type>
            >
         >,
         ordered_unique< tag<by_asset_balance>,
            composite_key<
               contract_balance_object,
               member<contract_balance_object, asset_id_type, &contract_balance_object::asset_type>,
               member<contract_balance_object, share_type, &contract_balance_object::balance>,
               member<contract_balance_object, contract_id_type, &contract_balance_object::owner>
            >,
            composite_key_compare<
               std::less< asset_id_type >,
               std::greater< share_type >,
               std::less< contract_id_type >
            >
         >
      >
   > contract_balance_object_multi_index_type;

   /**
    * @ingroup object_index
    */
   typedef generic_index<contract_balance_object, contract_balance_object_multi_index_type> contract_balance_index;

  //  struct by_name{};
  struct by_name;

   /**
    * @ingroup object_index
    */
   typedef multi_index_container<
      contract_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >
         // ordered_unique< tag<by_name>, member< contract_object, eosio::chain::name, &contract_object::name > >
      >
   > contract_multi_index_type;

   /**
    * @ingroup object_index
    */
   typedef generic_index<contract_object, contract_multi_index_type> contract_index;
   
   /**
    * @class contract_statistics_object
    * @ingroup object
    * @ingroup implementation
    *
    * This object contains regularly updated statistical data about an contract. It is provided for the purpose of
    * separating the account data that changes frequently from the account data that is mostly static, which will
    * minimize the amount of data that must be backed up as part of the undo history everytime a transfer is made.
    */
   class contract_statistics_object : public graphene::db::abstract_object<contract_statistics_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_contract_statistics_object_type;

         contract_id_type  owner;
         /**
          * Keep the most recent operation as a root pointer to a linked list of the transaction history.
          */
         contract_history_id_type most_recent_op;
         /** Total operations related to this account. */
         uint32_t                            total_ops = 0;
   };


}}

FC_REFLECT_DERIVED( graphene::chain::contract_object,
                    (graphene::db::object),
                    (statistics)
                    (allowed_assets)
                    (suicided)
                  )

FC_REFLECT_DERIVED( graphene::chain::contract_balance_object,
                    (graphene::db::object),
                    (owner)(asset_type)(balance)
                  )


FC_REFLECT_DERIVED( graphene::chain::contract_statistics_object,
                    (graphene::db::object),
                    (owner)(most_recent_op)(total_ops)
                  )
