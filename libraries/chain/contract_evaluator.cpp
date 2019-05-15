#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/result_contract_object.hpp>

namespace graphene { namespace chain {

   void_result contract_evaluator::do_evaluate( const contract_operation& op )
   { try {
   
      return void_result();
   } FC_CAPTURE_AND_RETHROW( (op) ) }

   object_id_type contract_evaluator::do_apply( const contract_operation& o )
   { try {

      return object_id_type();
   } FC_CAPTURE_AND_RETHROW((o)) }

} } // graphene::chain
