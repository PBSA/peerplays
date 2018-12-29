#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/db/generic_index.hpp>
// #include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {

   class result_contract_object : public graphene::db::abstract_object<result_contract_object>
   {
      public:
      
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = result_contract_object_type;

         vector<uint64_t> contracts_id;
         
         contract_id_type get_id()const { return id; }
   };

   /**
    * @ingroup object_index
    */
   typedef multi_index_container<
      result_contract_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >
      >
   > result_contract_multi_index_type;

   /**
    * @ingroup object_index
    */
   typedef generic_index<result_contract_object, result_contract_multi_index_type> result_contract_index;

}}
FC_REFLECT_DERIVED( graphene::chain::result_contract_object, (graphene::db::object),(contracts_id) )
