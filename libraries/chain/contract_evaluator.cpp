#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/result_contract_object.hpp>

#include <fee_gas.hpp>

namespace graphene { namespace chain {

   void_result contract_evaluator::do_evaluate( const contract_operation& op )
   { try {
   
      if( op.vm_type == vm_types::EVM ) {
         eth_op eth_data;
         try {
            eth_data = fc::raw::unpack<eth_op>( op.data );
         } catch ( ... ) {
            FC_ASSERT( "Data does not match the virtual machine." );
         }
         FC_ASSERT( op.registrar == eth_data.registrar );

         vms::base::fee_gas check(db(), *trx_state);
         FC_ASSERT( op.fee.asset_id == eth_data.asset_id_gas );
         FC_ASSERT( !eth_data.allowed_assets.empty() && eth_data.value != 0 ?
            eth_data.allowed_assets.count( static_cast<uint64_t>( eth_data.asset_id_transfer.instance ) ) : true );

         auto check_sum = op.fee + asset( eth_data.gasPrice * eth_data.gas, eth_data.asset_id_gas );
         if( eth_data.asset_id_gas == eth_data.asset_id_transfer ) {
            check.prepare_fee( check_sum.amount + share_type( eth_data.value ), op );
            FC_ASSERT( db().get_balance( op.registrar, eth_data.asset_id_gas) >= ( check_sum + asset( eth_data.value, eth_data.asset_id_gas ) ) );
         } else {
            FC_ASSERT( db().get_balance( op.registrar, eth_data.asset_id_gas ) >= check_sum );
            FC_ASSERT( db().get_balance( op.registrar, eth_data.asset_id_transfer ) >= asset( eth_data.value, eth_data.asset_id_transfer ) );
         }

         if( db()._evaluating_from_block && eth_data.receiver.valid() ) {
            const auto& contract_idx = db().get_index_type<contract_index>().indices().get<by_id>();
            FC_ASSERT( contract_idx.count( *eth_data.receiver ) && !( *eth_data.receiver )( db() ).suicided );
         }
      }

      return void_result();
   } FC_CAPTURE_AND_RETHROW( (op) ) }

   object_id_type contract_evaluator::do_apply( const contract_operation& o )
   { try {
      if( db()._evaluating_from_block ) {
         auto out = db()._executor->execute( o );

         result_contract_object result = db().create<result_contract_object>( [&]( result_contract_object& obj ) {
            obj.vm_type = o.vm_type;
            obj.contracts_ids = db()._executor->get_attracted_contracts( o.vm_type );
         });

         db().db_res.add_result( std::string( result.id ), out.second );
         db().db_res.commit_cache();
         db().db_res.commit();

         vms::base::fee_gas fee( db(), *trx_state);
         fee.process_fee( out.first, o);
         return result.id;
      }

      return object_id_type();
   } FC_CAPTURE_AND_RETHROW((o)) }

} } // graphene::chain
