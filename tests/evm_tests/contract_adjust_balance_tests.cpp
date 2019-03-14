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

BOOST_AUTO_TEST_CASE( contract_adjust_banalce ){
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    contract_operation contract_op;
    contract_op.version_vm = 1;
    contract_op.registrar = account_id_type(5);
    auto contract_code = string("6060604052346000575b600d6000819055505b5b6052806100206000396000f360606040526000357c0100000000000000000000000000000000000000000000000000000000900480634f2be91f146036575b6000565b603c603e565b005b600d6000600082825401925050819055505b56");
    
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(), 0, 1, 100000, contract_code } );

    transaction_evaluation_state context(&db);

    db._evaluating_from_apply_block = true;
    auto res = db.apply_operation( context, contract_op );
    db._evaluating_from_apply_block = false;

    result_contract_id_type cid = res.get<object_id_type>();

    db.adjust_balance( contract_id_type( cid(db).contracts_id.front() ), asset(1000, asset_id_type()));
    db.adjust_balance(account_id_type(5), -asset(1000, asset_id_type()));
    
    BOOST_CHECK(db.get_balance( contract_id_type( cid(db).contracts_id.front() ), asset_id_type() ) == asset(1000, asset_id_type()));
}

BOOST_AUTO_TEST_CASE( contract_adjust_banalce_neg ){
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    contract_operation contract_op;
    contract_op.version_vm = 1;
    contract_op.registrar = account_id_type(5);
    auto contract_code = string("6060604052346000575b600d6000819055505b5b6052806100206000396000f360606040526000357c0100000000000000000000000000000000000000000000000000000000900480634f2be91f146036575b6000565b603c603e565b005b600d6000600082825401925050819055505b56");
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(), 0, 1, 100000, contract_code } );
    transaction_evaluation_state context(&db);

    db._evaluating_from_apply_block = true;
    auto res = db.apply_operation( context, contract_op );
    db._evaluating_from_apply_block = false;

    result_contract_id_type cid = res.get<object_id_type>();

    db.adjust_balance( contract_id_type( cid(db).contracts_id.front() ), asset(1000, asset_id_type()));
    db.adjust_balance( contract_id_type( cid(db).contracts_id.front() ), asset(-500, asset_id_type()));
    db.adjust_balance(account_id_type(5), -asset(500, asset_id_type()));
    
    BOOST_CHECK(db.get_balance(contract_id_type( cid(db).contracts_id.front() ), asset_id_type()) == asset(500, asset_id_type()));
}

BOOST_AUTO_TEST_SUITE_END()