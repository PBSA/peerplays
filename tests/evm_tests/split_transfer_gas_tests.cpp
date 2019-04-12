#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"
#include <graphene/chain/contract_object.hpp>
#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/witness_object.hpp>
#include <evm_result.hpp>

#include <fc/filesystem.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace vms::evm;

/*
    pragma solidity >=0.4.22 <0.6.0;
    contract solidityAdd {
        uint a = 26;
        function add() public {
            a = a + 1;
        }
        function() external payable {}
        constructor () public payable {}
    }
*/
std::string solidityAddCode = "6080604052601a60005560898060166000396000f3fe6080604052600436106039576000357c0100000000000000000000000000000000000000000000000000000000900480634f2be91f14603b575b005b348015604657600080fd5b50604d604f565b005b60016000540160008190555056fea165627a7a72305820ca7373d84858554566f0336168b09e3e765186ac38e4cf18cc584bcec4bd7fe20029";

BOOST_FIXTURE_TEST_SUITE( split_transfer_gas_tests, database_fixture )

const uint64_t transfer_amount = 50000;
const uint64_t gas_limit_amount = 1000000;
const uint64_t custom_asset_transfer_amount = 100000000; // do not set less then 1000000 ( `not_enough_asset_for_fee_test` and `not_enough_asset_for_transfer_test` will fail this way )
const uint64_t core_asset_transfer_amount = 1000000000;
const uint64_t fee_pool_amount = 1000000;

inline const account_statistics_object& test_contract_deploy( database& db, asset_id_type first_asset, asset_id_type second_asset, account_id_type acc = account_id_type(5) ) {
   transaction_evaluation_state context(&db);
   asset_fund_fee_pool_operation op_fund_fee_pool;
   op_fund_fee_pool.asset_id = first_asset;
   op_fund_fee_pool.from_account = account_id_type(0);
   op_fund_fee_pool.amount = fee_pool_amount;

   db.apply_operation( context, op_fund_fee_pool );

   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = acc;
   contract_op.fee = asset(0, first_asset);
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), second_asset, transfer_amount, first_asset, 1, gas_limit_amount, solidityAddCode } );

   db._evaluating_from_block = true;
   db.apply_operation( context, contract_op );
   db._evaluating_from_block = false;

   const simple_index<account_statistics_object>& statistics_index = db.get_index_type<simple_index<account_statistics_object>>();
   auto iter = statistics_index.begin();
   for(size_t i = 0; i < 5; i++)
      iter++;
   return *iter;
}

inline share_type get_gas_used( database& db, result_contract_id_type result_id ) {
   auto raw_res = db.db_res.get_results( std::string( (object_id_type)result_id ) );
   auto res = fc::raw::unpack< vms::evm::evm_result >( *raw_res );
   return res.exec_result.gasUsed.convert_to<share_type>();
}

BOOST_AUTO_TEST_CASE( fee_test )
{
   asset_object temp_asset = create_user_issued_asset("ETB");
   issue_uia( account_id_type(5), temp_asset.amount( custom_asset_transfer_amount ) );
   transfer( account_id_type(0), account_id_type(5), asset( core_asset_transfer_amount, asset_id_type() ) );

   const auto& a = test_contract_deploy( db, asset_id_type(), asset_id_type(1) );
   auto gas_used = get_gas_used( db, result_contract_id_type() );

   BOOST_CHECK( a.pending_fees + a.pending_vested_fees == gas_used );
   BOOST_CHECK( db.get_balance(contract_id_type(), asset_id_type()).amount.value == 0 );
   BOOST_CHECK( db.get_balance(contract_id_type(), asset_id_type(1)).amount.value == transfer_amount );
   BOOST_CHECK( db.get_balance(account_id_type(5), asset_id_type()).amount.value == core_asset_transfer_amount - gas_used );
   BOOST_CHECK( db.get_balance(account_id_type(5), asset_id_type(1)).amount.value == custom_asset_transfer_amount - transfer_amount );
}

BOOST_AUTO_TEST_CASE( author_of_block_exec_tests )
{
   auto& wit_index = db.get_index_type<witness_index>().indices().get<by_id>();
   uint32_t slot_num = db.get_slot_at_time( db.head_block_time() + db.block_interval() );
   witness_id_type scheduled_witness = db.get_scheduled_witness( slot_num );
   witness_object wit_obj = *wit_index.find(scheduled_witness);
    
   transfer( account_id_type(0), wit_obj.witness_account, asset( 1000000000, asset_id_type() ) );

   generate_block();
   signed_transaction trx;
   set_expiration( db, trx );

   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = wit_obj.witness_account;
   contract_op.fee = asset(0, asset_id_type());
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 2000000, solidityAddCode });
   trx.operations.push_back( contract_op );

   PUSH_TX( db, trx, ~0 );
   trx.clear();
   generate_block();

   auto gas_used = get_gas_used( db, result_contract_id_type() );

   BOOST_CHECK( db.get_balance( wit_obj.witness_account, asset_id_type() ).amount.value + gas_used == 1000000000 );
}

BOOST_AUTO_TEST_CASE( not_CORE_fee_test )
{
   asset_object temp_asset = create_user_issued_asset("ETB");
   issue_uia( account_id_type(5), temp_asset.amount( custom_asset_transfer_amount ) );
   transfer( account_id_type(0), account_id_type(5), asset(core_asset_transfer_amount, asset_id_type() ) );

   const auto& a = test_contract_deploy( db, asset_id_type(1), asset_id_type() );
   auto gas_used = get_gas_used( db, result_contract_id_type() );

   BOOST_CHECK( a.pending_fees + a.pending_vested_fees == gas_used );
   BOOST_CHECK( db.get_balance(contract_id_type(), asset_id_type(1)).amount.value == 0 );
   BOOST_CHECK( db.get_balance(contract_id_type(), asset_id_type()).amount.value == transfer_amount );
   BOOST_CHECK( db.get_balance(account_id_type(5), asset_id_type()).amount.value == core_asset_transfer_amount - transfer_amount );
   BOOST_CHECK( db.get_balance(account_id_type(5), asset_id_type(1)).amount.value == custom_asset_transfer_amount - gas_used );
}

BOOST_AUTO_TEST_CASE( mixed_assets_test )
{
   asset_object temp_asset = create_user_issued_asset("ETB");
   issue_uia( account_id_type(5), temp_asset.amount( custom_asset_transfer_amount ) );
   transfer(account_id_type(0),account_id_type(5),asset(core_asset_transfer_amount, asset_id_type()));

   const auto& a = test_contract_deploy( db, asset_id_type(1), asset_id_type() );
   auto gas_deploy = get_gas_used( db, result_contract_id_type() );    

   BOOST_CHECK(a.pending_fees + a.pending_vested_fees == gas_deploy);

   transaction_evaluation_state context(&db);
   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type(5);
   contract_op.fee = asset(0, asset_id_type());
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(), std::set<uint64_t>(), asset_id_type(1), transfer_amount, asset_id_type(), 1, gas_limit_amount } );

   db._evaluating_from_block = true;
   db.apply_operation( context, contract_op );
   db._evaluating_from_block = false;

   auto gas_call =  get_gas_used( db, result_contract_id_type(1) );

   BOOST_CHECK( db.get_balance(contract_id_type(), asset_id_type()).amount.value == transfer_amount );
   BOOST_CHECK( db.get_balance(contract_id_type(), asset_id_type(1)).amount.value == transfer_amount );
   BOOST_CHECK( db.get_balance(account_id_type(5), asset_id_type()).amount.value == core_asset_transfer_amount - transfer_amount - gas_call );
   BOOST_CHECK( db.get_balance(account_id_type(5), asset_id_type(1)).amount.value == custom_asset_transfer_amount - transfer_amount - gas_deploy );
}

inline void not_enough_cash_call( database_fixture& df, asset_id_type transfer, asset_id_type gas )
{
   asset_object temp_asset = df.create_user_issued_asset("ETB");
   df.issue_uia( account_id_type(5), temp_asset.amount( custom_asset_transfer_amount / 100000 ) );
   df.transfer( account_id_type(0), account_id_type(5), asset( core_asset_transfer_amount, asset_id_type() ) );

   transaction_evaluation_state context(&df.db);
   asset_fund_fee_pool_operation op_fund_fee_pool;
   op_fund_fee_pool.asset_id = gas;
   op_fund_fee_pool.from_account = account_id_type(0);
   op_fund_fee_pool.amount = fee_pool_amount;

   df.db.apply_operation( context, op_fund_fee_pool );

   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type(5);
   contract_op.fee = asset(0, gas);
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), transfer, transfer_amount, gas, 1, gas_limit_amount, solidityAddCode } );

   df.db._evaluating_from_block = true;
   GRAPHENE_REQUIRE_THROW( df.db.apply_operation( context, contract_op ), fc::exception );
   df.db._evaluating_from_block = false;
}

BOOST_AUTO_TEST_CASE( not_enough_asset_for_fee_test )
{
   not_enough_cash_call( *this, asset_id_type(), asset_id_type(1) );
}

BOOST_AUTO_TEST_CASE( not_enough_asset_for_transfer_test )
{
   not_enough_cash_call( *this, asset_id_type(1), asset_id_type() );
}

BOOST_AUTO_TEST_CASE( check_gas_and_value_test )
{
   set_max_block_gas_limit();
   transaction_evaluation_state context(&db);
   asset_object temp_asset = create_user_issued_asset("ETB");
   issue_uia( account_id_type(5), temp_asset.amount( custom_asset_transfer_amount ) );
   transfer(account_id_type(0),account_id_type(5),asset(core_asset_transfer_amount, asset_id_type()));

   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type( 5 );
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type( 0 ), 0, asset_id_type( 0 ), 1, 500000, solidityAddCode } );

   db._evaluating_from_block = true;
   auto create_result = db.apply_operation( context, contract_op );

   auto create_evm_res = fc::raw::unpack<vms::evm::evm_result>( *db.db_res.get_results( std::string( create_result.get<object_id_type>() ) ) );
   auto id_contract = vms::evm::address_to_id( create_evm_res.exec_result.newAddress ).second;
   const uint64_t balance_core = static_cast<uint64_t>( db.get_balance( contract_op.registrar, asset_id_type() ).amount.value );
   const uint64_t balance_asset = static_cast<uint64_t>( db.get_balance( contract_op.registrar, temp_asset.id ).amount.value );

   auto exec = [&]( contract_operation& op ) -> vms::evm::evm_result {
      auto call_result = db._executor->execute( op );
      return fc::raw::unpack<vms::evm::evm_result>( call_result.second );
   };

   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type( id_contract ), std::set<uint64_t>(), asset_id_type( 0 ), balance_core + 1, asset_id_type( 0 ), 1, 500000, "00" } );
   BOOST_CHECK( exec( contract_op ).exec_result.excepted == dev::eth::TransactionException::NotEnoughCash );

   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type( id_contract ), std::set<uint64_t>(), asset_id_type( 0 ), 13, temp_asset.id, 1, balance_asset + 1, "00" } );
   BOOST_CHECK( exec( contract_op ).exec_result.excepted == dev::eth::TransactionException::NotEnoughCash );

   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type( id_contract ), std::set<uint64_t>(), asset_id_type( 0 ), balance_core + 1, temp_asset.id, 1, 500000, "00" } );
   BOOST_CHECK( exec( contract_op ).exec_result.excepted == dev::eth::TransactionException::NotEnoughCash );

   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type( id_contract ), std::set<uint64_t>(), asset_id_type( 0 ), 13, temp_asset.id, 1, 500000, "00" } );
   auto result = exec( contract_op );
   BOOST_CHECK( result.exec_result.excepted == dev::eth::TransactionException::None );
   db.adjust_balance( contract_op.registrar, asset( static_cast<int64_t>( result.exec_result.gasUsed ), temp_asset.id ) );
   db._evaluating_from_block = false;
}

BOOST_AUTO_TEST_SUITE_END()