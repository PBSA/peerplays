#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/db/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {

   class contracts_results_in_block_object : public graphene::db::abstract_object<contracts_results_in_block_object>
   {
      public:
      
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = contracts_results_in_block_object_type;

         vector<result_contract_id_type> results_id;
         
         contracts_results_in_block_id_type get_id()const { return id; }
   };

   /**
    * @ingroup object_index
    */
   typedef multi_index_container<
      contracts_results_in_block_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >
      >
   > block_result_multi_index_type;

   /**
    * @ingroup object_index
    */
   typedef generic_index<contracts_results_in_block_object, block_result_multi_index_type> contracts_results_in_block_index;

}}

FC_REFLECT_DERIVED( graphene::chain::contracts_results_in_block_object, (graphene::db::object),(results_id) )
