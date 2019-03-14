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

BOOST_FIXTURE_TEST_SUITE( asset_balance_tests, database_fixture )

 /*
    pragma solidity >=0.4.22 <0.6.0;
    contract soliditySend {
      function se() public {
        address addr = 0x0100000000000000000000000000000000000000;
        (bool success,) = addr.call(abi.encodeWithSignature("saveBalance()"));
        require(success);
      }
    function() external payable {}
}
 */
std::string soliditySendCode = "608060405234801561001057600080fd5b506101c4806100206000396000f3fe60806040526004361061001e5760003560e01c8063178a18fe14610020575b005b34801561002c57600080fd5b50610035610037565b005b6000730100000000000000000000000000000000000000905060008173ffffffffffffffffffffffffffffffffffffffff166040516024016040516020818303038152906040527f9c793c98000000000000000000000000000000000000000000000000000000007bffffffffffffffffffffffffffffffffffffffffffffffffffffffff19166020820180517bffffffffffffffffffffffffffffffffffffffffffffffffffffffff83818316178352505050506040518082805190602001908083835b6020831061011f57805182526020820191506020810190506020830392506100fc565b6001836020036101000a0380198251168184511680821785525050505050509050019150506000604051808303816000865af19150503d8060008114610181576040519150601f19603f3d011682016040523d82523d6000602084013e610186565b606091505b505090508061019457600080fd5b505056fea165627a7a72305820e73845b15756cd9cf899fbbe1d61b750810266b128fff0aba0cbb1414d04d5c10029";
/*
    pragma solidity >=0.4.22 <0.6.0;
     contract solidity {
       uint public balanceAddr1CORE;
       uint public balanceAddr1ETB;
       uint public balanceAddr1EEB;
       uint public balanceAddr1EEE;

       uint public balanceAddr2CORE;
       uint public balanceAddr2ETB;
       uint public balanceAddr2EEB;
       uint public balanceAddr2EEE;

       uint public balanceThis2CORE;
       uint public balanceThis2ETB;
       uint public balanceThis2EEB;
       uint public balanceThis2EEE;

       address public addr1 = 0x0000000000000000000000000000000000000005;
       address public addr2 = 0x0100000000000000000000000000000000000000;

       function saveBalance() public {
          balanceAddr1CORE = addr1.assetbalance(0);
          balanceAddr1ETB = addr1.assetbalance(1);
          balanceAddr1EEB = addr1.assetbalance(2);
          balanceAddr1EEE = addr1.assetbalance(3);

          balanceAddr2CORE = addr2.assetbalance(0);
          balanceAddr2ETB = addr2.assetbalance(1);
          balanceAddr2EEB = addr2.assetbalance(2);
          balanceAddr2EEE = addr2.assetbalance(3);

          balanceThis2CORE = address(this).assetbalance(0);
          balanceThis2ETB = address(this).assetbalance(1);
          balanceThis2EEB = address(this).assetbalance(2);
          balanceThis2EEE = address(this).assetbalance(3);
       }

       function() external payable { }
     }
*/
std::string solidityCode = "60806040526005600c60006101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff160217905550730100000000000000000000000000000000000000600d60006101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff1602179055503480156100a757600080fd5b50610705806100b76000396000f3fe6080604052600436106100e85760003560e01c80638de38f981161008a578063c81d020d11610059578063c81d020d146102db578063e953470114610306578063eabe09ad14610331578063f4cadb5114610388576100e8565b80638de38f98146102175780639c793c981461026e578063afa25c8414610285578063c753244c146102b0576100e8565b8063589ec336116100c6578063589ec3361461016b57806361999fc4146101965780636b66c3f3146101c15780638c5a4c8a146101ec576100e8565b80632a79ae83146100ea57806346f63af21461011557806347b7f48314610140575b005b3480156100f657600080fd5b506100ff6103b3565b6040518082815260200191505060405180910390f35b34801561012157600080fd5b5061012a6103b9565b6040518082815260200191505060405180910390f35b34801561014c57600080fd5b506101556103bf565b6040518082815260200191505060405180910390f35b34801561017757600080fd5b506101806103c5565b6040518082815260200191505060405180910390f35b3480156101a257600080fd5b506101ab6103cb565b6040518082815260200191505060405180910390f35b3480156101cd57600080fd5b506101d66103d1565b6040518082815260200191505060405180910390f35b3480156101f857600080fd5b506102016103d7565b6040518082815260200191505060405180910390f35b34801561022357600080fd5b5061022c6103dd565b604051808273ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200191505060405180910390f35b34801561027a57600080fd5b50610283610403565b005b34801561029157600080fd5b5061029a610695565b6040518082815260200191505060405180910390f35b3480156102bc57600080fd5b506102c561069b565b6040518082815260200191505060405180910390f35b3480156102e757600080fd5b506102f06106a1565b6040518082815260200191505060405180910390f35b34801561031257600080fd5b5061031b6106a7565b6040518082815260200191505060405180910390f35b34801561033d57600080fd5b506103466106ad565b604051808273ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200191505060405180910390f35b34801561039457600080fd5b5061039d6106d3565b6040518082815260200191505060405180910390f35b60045481565b60055481565b60005481565b60065481565b600b5481565b60035481565b60015481565b600d60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1681565b600c60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166000bb600081905550600c60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166001bb600181905550600c60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166002bb600281905550600c60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166003bb600381905550600d60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166000bb600481905550600d60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166001bb600581905550600d60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166002bb600681905550600d60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166003bb6007819055503073ffffffffffffffffffffffffffffffffffffffff166000bb6008819055503073ffffffffffffffffffffffffffffffffffffffff166001bb6009819055503073ffffffffffffffffffffffffffffffffffffffff166002bb600a819055503073ffffffffffffffffffffffffffffffffffffffff166003bb600b81905550565b60085481565b60075481565b60095481565b60025481565b600c60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1681565b600a548156fea165627a7a72305820269e0af06f53b28cb5eee6217b5db924b2503d983c8ab944d1b2aa3168f7deec0029";
 
void checkBalancesToStorage(database_fixture &df, contract_id_type contract_id, std::vector<asset>& balancesAccount, std::vector<asset>& balancesContract) {
    auto storage = df.get_evm_state().storage(id_to_address(contract_id.instance.value, 1));
    BOOST_CHECK_EQUAL( storage.size(), 14 );
    for (auto e : storage) {
        switch(uint64_t(e.second.first)) {
            case 0: BOOST_CHECK(uint64_t(e.second.second) == balancesAccount[0].amount); break;
            case 1: BOOST_CHECK(uint64_t(e.second.second) == balancesAccount[1].amount); break;
            case 2: BOOST_CHECK(uint64_t(e.second.second) == balancesAccount[2].amount); break;
            case 3: BOOST_CHECK(uint64_t(e.second.second) == balancesAccount[3].amount); break;
            case 4: BOOST_CHECK(uint64_t(e.second.second) == balancesContract[0].amount); break;
            case 5: BOOST_CHECK(uint64_t(e.second.second) == balancesContract[1].amount); break;
            case 6: BOOST_CHECK(uint64_t(e.second.second) == balancesContract[2].amount); break;
            case 7: BOOST_CHECK(uint64_t(e.second.second) == balancesContract[3].amount); break;
            case 8: BOOST_CHECK(uint64_t(e.second.second) == balancesContract[0].amount); break;
            case 9: BOOST_CHECK(uint64_t(e.second.second) == balancesContract[1].amount); break;
            case 10: BOOST_CHECK(uint64_t(e.second.second) == balancesContract[2].amount); break;
            case 11: BOOST_CHECK(uint64_t(e.second.second) == balancesContract[3].amount); break;
        }
    }
}

BOOST_AUTO_TEST_CASE( many_asset_assetbalance_test ){
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
    contract_op.data = fc::raw::pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(0), 0, 0, 4000000, solidityCode } );
    db._evaluating_from_apply_block = true;
    db.apply_operation( context, contract_op );

    contract_op.data = fc::raw::pack( eth_op{ contract_op.registrar,contract_id_type(0), asset_id_type(0), 50000, 0, 4000000 } );

    std::vector<asset> balancesAccount;
    std::vector<asset> balancesContract;
    for(size_t i = 0; i < 4; i++) {
        contract_op.data = fc::raw::pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(i), 50000, 0, 4000000 } );
        contract_op.fee = asset(0, asset_id_type(i));
        db.apply_operation( context, contract_op );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        balancesAccount.push_back(db.get_balance(account_id_type(5), asset_id_type(i)));
        balancesContract.push_back(db.get_balance(contract_id_type(0), asset_id_type(i)));
    }
    
    contract_op.fee = asset(0, asset_id_type(0));
    contract_op.data = fc::raw::pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(0), 0, 0, 4000000, "9c793c98" } );
    db.apply_operation( context, contract_op );
    db._evaluating_from_apply_block = false;

    BOOST_CHECK(get_evm_state().addresses().size() == 1);
    checkBalancesToStorage(*this, contract_id_type(0), balancesAccount, balancesContract);
}

BOOST_AUTO_TEST_CASE( many_asset_assetbalance_depth_test ){
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
    contract_op.data = fc::raw::pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(0), 0, 0, 4000000, solidityCode } );
    db._evaluating_from_apply_block = true;
    db.apply_operation( context, contract_op );

    contract_op.data = fc::raw::pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(0), 0, 0, 4000000, soliditySendCode } );
    db.apply_operation( context, contract_op );

    contract_op.data = fc::raw::pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(0), 50000, 0, 4000000 } );

    std::vector<asset> balancesAccount;
    std::vector<asset> balancesContract;
    for(size_t i = 0; i < 4; i++) {
        contract_op.data = fc::raw::pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(i), 50000, 0, 4000000 } );
        contract_op.fee = asset(0, asset_id_type(i));
        db.apply_operation( context, contract_op );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        balancesAccount.push_back(db.get_balance(account_id_type(5), asset_id_type(i)));
        balancesContract.push_back(db.get_balance(contract_id_type(0), asset_id_type(i)));
    }
    
    contract_op.fee = asset(0, asset_id_type(0));
    contract_op.data = fc::raw::pack( eth_op{ contract_op.registrar, contract_id_type(1), asset_id_type(0), 0, 0, 4000000, "178a18fe" } );
    db.apply_operation( context, contract_op );
    db._evaluating_from_apply_block = false;

    BOOST_CHECK(get_evm_state().addresses().size() == 2);
    checkBalancesToStorage(*this, contract_id_type(0), balancesAccount, balancesContract);
}

BOOST_AUTO_TEST_SUITE_END()
