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
std::string soliditySendCode = "608060405234801561001057600080fd5b506101e1806100206000396000f3fe60806040526004361061003b576000357c010000000000000000000000000000000000000000000000000000000090048063178a18fe1461003d575b005b34801561004957600080fd5b50610052610054565b005b6000730100000000000000000000000000000000000000905060008173ffffffffffffffffffffffffffffffffffffffff166040516024016040516020818303038152906040527f9c793c98000000000000000000000000000000000000000000000000000000007bffffffffffffffffffffffffffffffffffffffffffffffffffffffff19166020820180517bffffffffffffffffffffffffffffffffffffffffffffffffffffffff83818316178352505050506040518082805190602001908083835b6020831061013c5780518252602082019150602081019050602083039250610119565b6001836020036101000a0380198251168184511680821785525050505050509050019150506000604051808303816000865af19150503d806000811461019e576040519150601f19603f3d011682016040523d82523d6000602084013e6101a3565b606091505b50509050806101b157600080fd5b505056fea165627a7a72305820c692a7f27ec6f3213864024bb8d4815b7933b1fc46498e1c546d62f50abdfb4a0029";
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
std::string solidityCode = "60806040526005600c60006101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff160217905550730100000000000000000000000000000000000000600d60006101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff1602179055503480156100a757600080fd5b50610722806100b76000396000f3fe608060405260043610610105576000357c0100000000000000000000000000000000000000000000000000000000900480638de38f98116100a7578063c81d020d11610076578063c81d020d146102f8578063e953470114610323578063eabe09ad1461034e578063f4cadb51146103a557610105565b80638de38f98146102345780639c793c981461028b578063afa25c84146102a2578063c753244c146102cd57610105565b8063589ec336116100e3578063589ec3361461018857806361999fc4146101b35780636b66c3f3146101de5780638c5a4c8a1461020957610105565b80632a79ae831461010757806346f63af21461013257806347b7f4831461015d575b005b34801561011357600080fd5b5061011c6103d0565b6040518082815260200191505060405180910390f35b34801561013e57600080fd5b506101476103d6565b6040518082815260200191505060405180910390f35b34801561016957600080fd5b506101726103dc565b6040518082815260200191505060405180910390f35b34801561019457600080fd5b5061019d6103e2565b6040518082815260200191505060405180910390f35b3480156101bf57600080fd5b506101c86103e8565b6040518082815260200191505060405180910390f35b3480156101ea57600080fd5b506101f36103ee565b6040518082815260200191505060405180910390f35b34801561021557600080fd5b5061021e6103f4565b6040518082815260200191505060405180910390f35b34801561024057600080fd5b506102496103fa565b604051808273ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200191505060405180910390f35b34801561029757600080fd5b506102a0610420565b005b3480156102ae57600080fd5b506102b76106b2565b6040518082815260200191505060405180910390f35b3480156102d957600080fd5b506102e26106b8565b6040518082815260200191505060405180910390f35b34801561030457600080fd5b5061030d6106be565b6040518082815260200191505060405180910390f35b34801561032f57600080fd5b506103386106c4565b6040518082815260200191505060405180910390f35b34801561035a57600080fd5b506103636106ca565b604051808273ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200191505060405180910390f35b3480156103b157600080fd5b506103ba6106f0565b6040518082815260200191505060405180910390f35b60045481565b60055481565b60005481565b60065481565b600b5481565b60035481565b60015481565b600d60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1681565b600c60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166000bb600081905550600c60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166001bb600181905550600c60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166002bb600281905550600c60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166003bb600381905550600d60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166000bb600481905550600d60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166001bb600581905550600d60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166002bb600681905550600d60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166003bb6007819055503073ffffffffffffffffffffffffffffffffffffffff166000bb6008819055503073ffffffffffffffffffffffffffffffffffffffff166001bb6009819055503073ffffffffffffffffffffffffffffffffffffffff166002bb600a819055503073ffffffffffffffffffffffffffffffffffffffff166003bb600b81905550565b60085481565b60075481565b60095481565b60025481565b600c60009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1681565b600a548156fea165627a7a72305820ce4f98272c3a196f96995838cf8db8d97c5f87c98f11f642bb1a16f4cae82a900029";
 
void checkBalancesToStorage(database_fixture &df, contract_id_type contract_id, std::vector<asset>& balancesAccount, std::vector<asset>& balancesContract) {
    auto storage = df.get_evm_state().storage(id_to_address(contract_id.instance.value, 1));
    BOOST_CHECK_EQUAL( storage.size(), 14 );
    for (auto e : storage) {
        switch(uint64_t(e.second.first)) {
            case 0: BOOST_CHECK(uint64_t(e.second.second) == balancesAccount[0].amount-4000000); break;
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
    contract_op.vm_type = vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type(0));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 4000000, solidityCode } );
    db._evaluating_from_block = true;
    db.apply_operation( context, contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), std::set<uint64_t>(), asset_id_type(0), 50000, asset_id_type(0), 1, 4000000 } );

    std::vector<asset> balancesAccount;
    std::vector<asset> balancesContract;
    for(size_t i = 0; i < 4; i++) {
        contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), std::set<uint64_t>(), asset_id_type(i), 50000, asset_id_type(i), 1, 4000000 } );
        contract_op.fee = asset(0, asset_id_type(i));
        db.apply_operation( context, contract_op );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        balancesAccount.push_back(db.get_balance(account_id_type(5), asset_id_type(i)));
        balancesContract.push_back(db.get_balance(contract_id_type(0), asset_id_type(i)));
    }
    
    contract_op.fee = asset(0, asset_id_type(0));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 4000000, "9c793c98" } );
    db.apply_operation( context, contract_op );
    db._evaluating_from_block = false;

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
    contract_op.vm_type = vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type(0));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 4000000, solidityCode } );
    db._evaluating_from_block = true;
    db.apply_operation( context, contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 4000000, soliditySendCode } );
    db.apply_operation( context, contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), std::set<uint64_t>(), asset_id_type(0), 50000, asset_id_type(0), 1, 4000000 } );

    std::vector<asset> balancesAccount;
    std::vector<asset> balancesContract;
    for(size_t i = 0; i < 4; i++) {
        contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), std::set<uint64_t>(), asset_id_type(i), 50000, asset_id_type(i), 1, 4000000 } );
        contract_op.fee = asset(0, asset_id_type(i));
        db.apply_operation( context, contract_op );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        balancesAccount.push_back(db.get_balance(account_id_type(5), asset_id_type(i)));
        balancesContract.push_back(db.get_balance(contract_id_type(0), asset_id_type(i)));
    }
    
    contract_op.fee = asset(0, asset_id_type(0));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(1), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 4000000, "178a18fe" } );
    db.apply_operation( context, contract_op );
    db._evaluating_from_block = false;

    BOOST_CHECK(get_evm_state().addresses().size() == 2);
    checkBalancesToStorage(*this, contract_id_type(0), balancesAccount, balancesContract);
}

BOOST_AUTO_TEST_SUITE_END()
