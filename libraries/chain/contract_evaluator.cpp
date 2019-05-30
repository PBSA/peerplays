#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/result_contract_object.hpp>

namespace graphene { namespace chain {

   void_result contract_evaluator::do_evaluate( const contract_operation& op )
   { try {
      if( op.vm_type == vm_types::WAVM ) {
         wavm_op wavm_data;
         try {
            wavm_data = fc_pp::raw::unpack<wavm_op>( op.data );
         } catch ( ... ) {
            FC_ASSERT( "Data does not match the virtual machine." );
         }
      }
   
      return void_result();
   } FC_CAPTURE_AND_RETHROW( (op) ) }

   object_id_type contract_evaluator::do_apply( const contract_operation& o )
   { try {
         auto out = db()._executor->execute( o );
      return object_id_type();
   } FC_CAPTURE_AND_RETHROW((o)) }

} } // graphene::chain
