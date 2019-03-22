#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"
#include <graphene/chain/result_contract_object.hpp>
#include <graphene/chain/contract_evaluator.hpp>

#include <fc/filesystem.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

namespace graphene { namespace app { namespace detail {
genesis_state_type create_example_genesis();
} } }

BOOST_FIXTURE_TEST_SUITE( contract_adjust_balance_tests, database_fixture )

/*
    pragma solidity >=0.4.22 <0.6.0;
    contract soliditySend {
      function() external payable {}
    }
*/
auto contract_code = "6080604052348015600f57600080fd5b50603280601d6000396000f3fe608060405200fea165627a7a723058207b2b25058622910280a2634280772cbe0f3a64d254ea81a99b45145cd6439cdf0029";

BOOST_AUTO_TEST_CASE( contract_adjust_banalce ){

    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    contract_operation contract_op;
    contract_op.vm_type = vms::base::vm_types::EVM;
    contract_op.registrar = account_id_type(5);

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(), 0, asset_id_type(), 1, 100000, contract_code } );

    transaction_evaluation_state context(&db);

    db._evaluating_from_apply_block = true;
    auto res = db.apply_operation( context, contract_op );
    db._evaluating_from_apply_block = false;

    result_contract_id_type cid = res.get<object_id_type>();

    db.adjust_balance( contract_id_type( cid(db).contracts_ids.front() ), asset(1000, asset_id_type()));
    db.adjust_balance(account_id_type(5), -asset(1000, asset_id_type()));
    
    BOOST_CHECK(db.get_balance( contract_id_type( cid(db).contracts_ids.front() ), asset_id_type() ) == asset(1000, asset_id_type()));
}

BOOST_AUTO_TEST_CASE( contract_adjust_banalce_neg ){
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    contract_operation contract_op;
    contract_op.vm_type = vms::base::vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(), 0, asset_id_type(), 1, 100000, contract_code } );
    transaction_evaluation_state context(&db);

    db._evaluating_from_apply_block = true;
    auto res = db.apply_operation( context, contract_op );
    db._evaluating_from_apply_block = false;

    result_contract_id_type cid = res.get<object_id_type>();

    db.adjust_balance( contract_id_type( cid(db).contracts_ids.front() ), asset(1000, asset_id_type()));
    db.adjust_balance( contract_id_type( cid(db).contracts_ids.front() ), asset(-500, asset_id_type()));
    db.adjust_balance(account_id_type(5), -asset(500, asset_id_type()));
    
    BOOST_CHECK(db.get_balance(contract_id_type( cid(db).contracts_ids.front() ), asset_id_type()) == asset(500, asset_id_type()));
}

BOOST_AUTO_TEST_SUITE_END()