#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"
#include <graphene/chain/contract_object.hpp>
#include <graphene/chain/contract_evaluator.hpp>

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

inline const account_statistics_object& test_contract_deploy( database& db, asset_id_type first_asset, asset_id_type second_asset, share_type fee_pool_amount = 1000000 ) {
    transaction_evaluation_state context(&db);
    asset_fund_fee_pool_operation op_fund_fee_pool;
    op_fund_fee_pool.asset_id = first_asset;
    op_fund_fee_pool.from_account = account_id_type(0);
    op_fund_fee_pool.amount = fee_pool_amount;

    db.apply_operation( context, op_fund_fee_pool );

    contract_operation contract_op;
    contract_op.vm_type = vms::base::vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, first_asset);
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), second_asset, 50000, first_asset, 1, 1000000, solidityAddCode } );

    db._evaluating_from_apply_block = true;
    db.apply_operation( context, contract_op );
    db._evaluating_from_apply_block = false;

    const simple_index<account_statistics_object>& statistics_index = db.get_index_type<simple_index<account_statistics_object>>();
    auto iter = statistics_index.begin();
    for(size_t i = 0; i < 5; i++)
        iter++;
    return *iter;
}

BOOST_AUTO_TEST_CASE( fee_test ){
    asset_object temp_asset = create_user_issued_asset("ETB");
    issue_uia( account_id_type(5), temp_asset.amount( 100000000 ) );
    transfer( account_id_type(0), account_id_type(5), asset( 1000000000, asset_id_type() ) );

    const auto& a = test_contract_deploy( db, asset_id_type(), asset_id_type(1) );

    BOOST_CHECK( a.pending_fees + a.pending_vested_fees == 108843 );
    BOOST_CHECK( db.get_balance(contract_id_type(), asset_id_type()).amount.value == 0 );
    BOOST_CHECK( db.get_balance(contract_id_type(), asset_id_type(1)).amount.value == 50000 );
    BOOST_CHECK( db.get_balance(account_id_type(5), asset_id_type()).amount.value == 1000000000 - 108843 );
    BOOST_CHECK( db.get_balance(account_id_type(5), asset_id_type(1)).amount.value == 100000000 - 50000 );
}

BOOST_AUTO_TEST_CASE( not_CORE_fee_test ){
    asset_object temp_asset = create_user_issued_asset("ETB");
    issue_uia( account_id_type(5), temp_asset.amount( 100000000 ) );
    transfer( account_id_type(0), account_id_type(5), asset(1000000000, asset_id_type() ) );

    const auto& a = test_contract_deploy( db, asset_id_type(1), asset_id_type() );

    BOOST_CHECK( a.pending_fees + a.pending_vested_fees == 108843 );
    BOOST_CHECK( db.get_balance(contract_id_type(), asset_id_type(1)).amount.value == 0 );
    BOOST_CHECK( db.get_balance(contract_id_type(), asset_id_type()).amount.value == 50000 );
    BOOST_CHECK( db.get_balance(account_id_type(5), asset_id_type()).amount.value == 1000000000 - 50000 );
    BOOST_CHECK( db.get_balance(account_id_type(5), asset_id_type(1)).amount.value == 100000000 - 108843 );
}

BOOST_AUTO_TEST_CASE( mixed_assets_test ){
    asset_object temp_asset = create_user_issued_asset("ETB");
    issue_uia( account_id_type(5), temp_asset.amount( 100000000 ) );
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));

    const auto& a = test_contract_deploy( db, asset_id_type(1), asset_id_type() );

    BOOST_CHECK(a.pending_fees + a.pending_vested_fees == 108843);

    transaction_evaluation_state context(&db);
    contract_operation contract_op;
    contract_op.vm_type = vms::base::vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type());
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(), asset_id_type(1), 50000, asset_id_type(), 1, 1000000 } );

    db._evaluating_from_apply_block = true;
    db.apply_operation( context, contract_op );
    db._evaluating_from_apply_block = false;

    BOOST_CHECK( db.get_balance(contract_id_type(), asset_id_type()).amount.value == 50000 );
    BOOST_CHECK( db.get_balance(contract_id_type(), asset_id_type(1)).amount.value == 50000 );
    BOOST_CHECK( db.get_balance(account_id_type(5), asset_id_type()).amount.value == 1000000000 - 50000 - 21040 );
    BOOST_CHECK( db.get_balance(account_id_type(5), asset_id_type(1)).amount.value == 100000000 - 108843 - 50000 );
}

BOOST_AUTO_TEST_CASE( not_enough_asset_for_fee_test ){
    asset_object temp_asset = create_user_issued_asset("ETB");
    issue_uia( account_id_type(5), temp_asset.amount( 1000 ) );
    transfer( account_id_type(0), account_id_type(5), asset( 1000000000, asset_id_type() ) );

    transaction_evaluation_state context(&db);
    asset_fund_fee_pool_operation op_fund_fee_pool;
    op_fund_fee_pool.asset_id = asset_id_type(1);
    op_fund_fee_pool.from_account = account_id_type(0);
    op_fund_fee_pool.amount = 100000;

    db.apply_operation( context, op_fund_fee_pool );

    contract_operation contract_op;
    contract_op.vm_type = vms::base::vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type(1));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(), 50000, asset_id_type(1), 1, 1000000, solidityAddCode } );

    db._evaluating_from_apply_block = true;
    GRAPHENE_REQUIRE_THROW( db.apply_operation( context, contract_op ), fc::exception );
    db._evaluating_from_apply_block = false;
}

BOOST_AUTO_TEST_CASE( not_enough_asset_for_transfer_test ){
    asset_object temp_asset = create_user_issued_asset("ETB");
    issue_uia( account_id_type(5), temp_asset.amount( 1000 ) );
    transfer( account_id_type(0), account_id_type(5), asset( 1000000000, asset_id_type() ) );

    transaction_evaluation_state context(&db);
    asset_fund_fee_pool_operation op_fund_fee_pool;
    op_fund_fee_pool.asset_id = asset_id_type();
    op_fund_fee_pool.from_account = account_id_type(0);
    op_fund_fee_pool.amount = 100000;

    db.apply_operation( context, op_fund_fee_pool );

    contract_operation contract_op;
    contract_op.vm_type = vms::base::vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type());
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(1), 50000, asset_id_type(), 1, 300000, solidityAddCode } );

    db._evaluating_from_apply_block = true;
    GRAPHENE_REQUIRE_THROW( db.apply_operation( context, contract_op ), fc::exception );
    db._evaluating_from_apply_block = false;
}

BOOST_AUTO_TEST_SUITE_END()