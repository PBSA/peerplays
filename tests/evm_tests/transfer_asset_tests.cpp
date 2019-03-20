#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"
#include <graphene/chain/contract_object.hpp>
#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/result_contract_object.hpp>

#include <fc/filesystem.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace vms::evm;

namespace graphene { namespace app { namespace detail {
genesis_state_type create_example_genesis();
} } }

BOOST_FIXTURE_TEST_SUITE( transfer_asset_tests, database_fixture )

/*
    pragma solidity >=0.4.22 <0.6.0;
    contract solidity {
      function tr(address payable addr, uint sum, uint idAsset) public {
         addr.transferasset(sum, idAsset);
      }
      function() external payable {}
    }
*/
std::string solidityCode = "608060405234801561001057600080fd5b50610114806100206000396000f3fe6080604052600436106039576000357c010000000000000000000000000000000000000000000000000000000090048063955c806d14603b575b005b348015604657600080fd5b50609a60048036036060811015605b57600080fd5b81019080803573ffffffffffffffffffffffffffffffffffffffff1690602001909291908035906020019092919080359060200190929190505050609c565b005b808373ffffffffffffffffffffffffffffffffffffffff166108fc849081150290604051600060405180830381858888bc935050505015801560e2573d6000803e3d6000fd5b5050505056fea165627a7a72305820939955652b350f1b50c871244e72de33daeddcf27c6ed08f3d35301f4aef800e0029";
 
 /*
    pragma solidity >=0.4.22 <0.6.0;
    contract solidityTransfer {
      function tr() public {
         address payable addr = 0x0000000000000000000000000000000000000003;
         for(uint i = 0; i < 4; i++){
            addr.transferasset(10000, i);  
         }
      }
      function() external payable {}
    }
*/
std::string solidityTransferCode = "608060405234801561001057600080fd5b5060e88061001f6000396000f3fe6080604052600436106039576000357c010000000000000000000000000000000000000000000000000000000090048063c5917cbd14603b575b005b348015604657600080fd5b50604d604f565b005b60006003905060008090505b600481101560b857808273ffffffffffffffffffffffffffffffffffffffff166108fc6127109081150290604051600060405180830381858888bc935050505015801560ab573d6000803e3d6000fd5b508080600101915050605b565b505056fea165627a7a723058205dabaa49a3f334b1c0acec15611a6c519dc3816b9ac50f27b876fde1b2c5f73c0029";
 
 /*
    pragma solidity >=0.4.22 <0.6.0;
    contract soliditySend {
      function se() public {
         address payable addr = 0x0100000000000000000000000000000000000000;
         (bool success,) = addr.call(abi.encodeWithSignature("tr()"));
         require(success);
      }
      function() external payable {}
    }
*/
std::string soliditySendCode = "608060405234801561001057600080fd5b506101e1806100206000396000f3fe60806040526004361061003b576000357c010000000000000000000000000000000000000000000000000000000090048063178a18fe1461003d575b005b34801561004957600080fd5b50610052610054565b005b6000730100000000000000000000000000000000000000905060008173ffffffffffffffffffffffffffffffffffffffff166040516024016040516020818303038152906040527fc5917cbd000000000000000000000000000000000000000000000000000000007bffffffffffffffffffffffffffffffffffffffffffffffffffffffff19166020820180517bffffffffffffffffffffffffffffffffffffffffffffffffffffffff83818316178352505050506040518082805190602001908083835b6020831061013c5780518252602082019150602081019050602083039250610119565b6001836020036101000a0380198251168184511680821785525050505050509050019150506000604051808303816000865af19150503d806000811461019e576040519150601f19603f3d011682016040523d82523d6000602084013e6101a3565b606091505b50509050806101b157600080fd5b505056fea165627a7a723058207791068ae6840c88efa2f9c937c768f25554e06009bc52cce563a02ca6ab86200029";
 
BOOST_AUTO_TEST_CASE( many_asset_transferasset_test ){
    std::vector<asset_object> assets = { create_user_issued_asset("ETB"), create_user_issued_asset("EEB"), create_user_issued_asset("EEE") };
    operation_result result;
    transaction_evaluation_state context(&db);
    transfer(account_id_type(0),account_id_type(5),asset(100000000, asset_id_type()));
    for(asset_object& a : assets) {
        issue_uia( account_id_type(5), a.amount( 10000000 ) );
        asset_fund_fee_pool_operation op_fund_fee_pool;
        op_fund_fee_pool.asset_id = a.id;
        op_fund_fee_pool.from_account = account_id_type(0);
        op_fund_fee_pool.amount = 10000000;
        db.apply_operation( context, op_fund_fee_pool );
    }

    contract_operation contract_op;
    contract_op.version_vm = 1;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type(0));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(0), 0, 1, 2000000, solidityCode } );
    db._evaluating_from_apply_block = true;
    db.apply_operation( context, contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(), 50000, 1, 2000000 } );

    std::vector<asset> balancesTo;
    for(size_t i = 0; i < 4; i++) {
        contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(i), 50000, 1, 2000000 } );
        contract_op.fee = asset(0, asset_id_type(i));
        db.apply_operation( context, contract_op );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        balancesTo.push_back(db.get_balance(account_id_type(3), asset_id_type(i)));
    }

    contract_op.fee = asset(0, asset_id_type(0));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(0), 0, 1, 2000000 } );

    for(size_t i = 0; i < 4; i++) {
        std::string sol_code = "955c806d00000000000000000000000000000000000000000000000000000000000000030000000000000000000000000000000000000000000000000000000000002710000000000000000000000000000000000000000000000000000000000000000" + std::to_string(i);
        contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(0), 0, 1, 2000000, sol_code } );
        db.apply_operation( context, contract_op );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(40000, asset_id_type(i)));
        BOOST_CHECK(db.get_balance(account_id_type(3), asset_id_type(i)).amount == balancesTo[i].amount + 10000);
    }
    BOOST_CHECK(get_evm_state().addresses().size() == 1);
}

BOOST_AUTO_TEST_CASE( many_asset_transferasset_from_contract_test ){
    std::vector<asset_object> assets = { create_user_issued_asset("ETB"), create_user_issued_asset("EEB"), create_user_issued_asset("EEE") };

    operation_result result;
    transaction_evaluation_state context(&db);
    transfer(account_id_type(0),account_id_type(5),asset(10000000, asset_id_type()));
    for(asset_object& a : assets) {
        issue_uia( account_id_type(5), a.amount( 10000000 ) );
        transfer(account_id_type(0),account_id_type(5),asset(10000000, asset_id_type()));
    
        asset_fund_fee_pool_operation op_fund_fee_pool;
        op_fund_fee_pool.asset_id = a.id;
        op_fund_fee_pool.from_account = account_id_type(0);
        op_fund_fee_pool.amount = 10000000;
        db.apply_operation( context, op_fund_fee_pool );
    }

    contract_operation contract_op;
    contract_op.version_vm = 1;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type(0));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(0), 0, 1, 2000000, solidityTransferCode } );
    db._evaluating_from_apply_block = true;
    db.apply_operation( context, contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(0), 50000, 1, 2000000 } );

    std::vector<asset> balancesTo;
    for(size_t i = 0; i < 4; i++) {
        contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(i), 50000, 1, 2000000 } );
        contract_op.fee = asset(0, asset_id_type(i));
        db.apply_operation( context, contract_op );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        balancesTo.push_back(db.get_balance(account_id_type(3), asset_id_type(i)));
    }

    contract_op.fee = asset(0, asset_id_type(0));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(0), 0, 1, 2000000, "c5917cbd" } );

    db.apply_operation( context, contract_op );

    for(size_t i = 0; i < 4; i++) {
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(40000, asset_id_type(i)));
        BOOST_CHECK(db.get_balance(account_id_type(3), asset_id_type(i)).amount == balancesTo[i].amount + 10000);
    }
    BOOST_CHECK(get_evm_state().addresses().size() == 1);
}

BOOST_AUTO_TEST_CASE( many_asset_transferasset_depth_test ){
    std::vector<asset_object> assets = { create_user_issued_asset("ETB"), create_user_issued_asset("EEB"), create_user_issued_asset("EEE") };

    operation_result result;
    transaction_evaluation_state context(&db);
    transfer(account_id_type(0),account_id_type(5),asset(10000000, asset_id_type()));
    for(asset_object& a : assets) {
        issue_uia( account_id_type(5), a.amount( 10000000 ) );
        transfer(account_id_type(0),account_id_type(5),asset(10000000, asset_id_type()));
    
        asset_fund_fee_pool_operation op_fund_fee_pool;
        op_fund_fee_pool.asset_id = a.id;
        op_fund_fee_pool.from_account = account_id_type(0);
        op_fund_fee_pool.amount = 10000000;
        db.apply_operation( context, op_fund_fee_pool );
    }

    contract_operation contract_op;
    contract_op.version_vm = 1;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type(0));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(0), 0, 1, 2000000, solidityTransferCode } );
    db._evaluating_from_apply_block = true;
    db.apply_operation( context, contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(0), 0, 1, 2000000, soliditySendCode } );
    db.apply_operation( context, contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(0), 50000, 1, 2000000 } );

    std::vector<asset> balancesTo;
    for(size_t i = 0; i < 4; i++) {
        contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(i), 50000, 1, 2000000 } );
        contract_op.fee = asset(0, asset_id_type(i));
        db.apply_operation( context, contract_op );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        balancesTo.push_back(db.get_balance(account_id_type(3), asset_id_type(i)));
    }

    contract_op.fee = asset(0, asset_id_type(0));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(1), asset_id_type(0), 0, 1, 2000000, "178a18fe" } );
    db.apply_operation( context, contract_op );

    for(size_t i = 0; i < 4; i++) {
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(40000, asset_id_type(i)));
        BOOST_CHECK(db.get_balance(account_id_type(3), asset_id_type(i)).amount == balancesTo[i].amount + 10000);
    }
    BOOST_CHECK(get_evm_state().addresses().size() == 2);
}

BOOST_AUTO_TEST_CASE( many_asset_transferasset_CORE_test ){
    std::vector<asset_object> assets = { create_user_issued_asset("ETB"), create_user_issued_asset("EEB"), create_user_issued_asset("EEE") };
    operation_result result;
    transaction_evaluation_state context(&db);
    transfer(account_id_type(0),account_id_type(5),asset(100000000, asset_id_type()));
    for(asset_object& a : assets) {
        issue_uia( account_id_type(5), a.amount( 10000000 ) );
        asset_fund_fee_pool_operation op_fund_fee_pool;
        op_fund_fee_pool.asset_id = a.id;
        op_fund_fee_pool.from_account = account_id_type(0);
        op_fund_fee_pool.amount = 10000000;
        db.apply_operation( context, op_fund_fee_pool );
    }

    contract_operation contract_op;
    contract_op.version_vm = 1;
    contract_op.registrar = account_id_type(5);
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(0), 0, 1, 2000000, solidityCode } );
    db._evaluating_from_apply_block = true;
    db.apply_operation( context, contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(0), 50000, 1, 2000000 } );

    std::vector<asset> balancesTo;
    for(size_t i = 0; i < 4; i++) {
        contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(i), 50000, 1, 2000000 } );
        contract_op.fee = asset(0, asset_id_type(i));
        db.apply_operation( context, contract_op );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        balancesTo.push_back(db.get_balance(account_id_type(3), asset_id_type(i)));
    }

    contract_op.fee = asset(0, asset_id_type(1));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(1), 0, 1, 2000000 } );

    for(size_t i = 0; i < 4; i++) {
        std::string sol_code = "955c806d00000000000000000000000000000000000000000000000000000000000000030000000000000000000000000000000000000000000000000000000000002710000000000000000000000000000000000000000000000000000000000000000" + std::to_string(i);
        contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(1), 0, 1, 2000000, sol_code } );
        db.apply_operation( context, contract_op );

        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(40000, asset_id_type(i)));
        BOOST_CHECK(db.get_balance(account_id_type(3), asset_id_type(i)).amount == balancesTo[i].amount + 10000);
    }
    BOOST_CHECK(get_evm_state().addresses().size() == 1);
}

BOOST_AUTO_TEST_CASE( many_asset_transferasset_not_CORE_test ){
    std::vector<asset_object> assets = { create_user_issued_asset("ETB"), create_user_issued_asset("EEB"), create_user_issued_asset("EEE") };
    operation_result result;
    transaction_evaluation_state context(&db);
    transfer(account_id_type(0),account_id_type(5),asset(100000000, asset_id_type()));
    for(asset_object& a : assets) {
        issue_uia( account_id_type(5), a.amount( 10000000 ) );
        asset_fund_fee_pool_operation op_fund_fee_pool;
        op_fund_fee_pool.asset_id = a.id;
        op_fund_fee_pool.from_account = account_id_type(0);
        op_fund_fee_pool.amount = 10000000;
        db.apply_operation( context, op_fund_fee_pool );
    }

    contract_operation contract_op;
    contract_op.version_vm = 1;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type(1));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(1), 0, 1, 2000000, solidityCode } );
    db._evaluating_from_apply_block = true;
    db.apply_operation( context, contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(1), 50000, 1, 2000000 } );

    std::vector<asset> balancesTo;
    for(size_t i = 1; i < 4; i++) {
        contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(i), 50000, 1, 2000000 } );
        contract_op.fee = asset(0, asset_id_type(i));
        db.apply_operation( context, contract_op );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        balancesTo.push_back(db.get_balance(account_id_type(3), asset_id_type(i)));
    }

    contract_op.fee = asset(0, asset_id_type(1));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(1), 0, 1, 2000000 } );

    for(size_t i = 1; i < 4; i++) {
        std::string sol_code = "955c806d00000000000000000000000000000000000000000000000000000000000000030000000000000000000000000000000000000000000000000000000000002710000000000000000000000000000000000000000000000000000000000000000" + std::to_string(i);
        contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(1), 0, 1, 2000000, sol_code } );
        db.apply_operation( context, contract_op );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(40000, asset_id_type(i)));
        BOOST_CHECK(db.get_balance(account_id_type(3), asset_id_type(i)).amount == balancesTo[i - 1].amount + 10000);
    }
    BOOST_CHECK(get_evm_state().addresses().size() == 1);
}

BOOST_AUTO_TEST_SUITE_END()
