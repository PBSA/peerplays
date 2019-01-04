#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/result_contract_object.hpp>

#include <fee_gas.hpp>

namespace graphene { namespace chain {

   void_result contract_evaluator::do_evaluate( const contract_operation& op )
   { try {
      
      try {
         if( op.version_vm == 1 ) {
            eth_op eth_data = fc::raw::unpack<eth_op>( op.data );
            FC_ASSERT( op.registrar == eth_data.registrar );

            vms::base::fee_gas check(db(), *trx_state);
            check.prepare_fee(op.fee.amount + share_type( eth_data.gas * eth_data.gasPrice + eth_data.value), op );
            FC_ASSERT( op.fee.asset_id == eth_data.asset_id );
            if( eth_data.receiver )
               FC_ASSERT( !( *eth_data.receiver )( db() ).suicided );
            }
      } catch ( ... ) {
         FC_ASSERT( "Data does not match the virtual machine." );
      }

      return void_result();
   } FC_CAPTURE_AND_RETHROW( (op) ) }

   object_id_type contract_evaluator::do_apply( const contract_operation& o )
   { try {

      auto out = db()._executor->execute( o, true );

      result_contract_object result = db().create<result_contract_object>( [&]( result_contract_object& obj ){
         obj.contracts_id = db()._executor->get_attracted_contracts( o.version_vm );
      });

      vms::base::fee_gas fee( db(), *trx_state);
      fee.process_fee( out.first, o);
        
      return result.id;
   } FC_CAPTURE_AND_RETHROW((o)) }

} } // graphene::chain
