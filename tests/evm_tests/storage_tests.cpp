#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"
#include <graphene/chain/contract_object.hpp>
#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/result_contract_object.hpp>

#include <evm_result.hpp>

#include <fc/filesystem.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace vms::evm;

namespace graphene { namespace app { namespace detail {
genesis_state_type create_example_genesis();
} } }

string u256_to_string(u256& data){
    std::stringstream ss;
    ss << data;
    return ss.str();
}

BOOST_FIXTURE_TEST_SUITE( storage_tests, database_fixture )
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

/*
    pragma solidity >=0.4.22 <0.6.0;
    contract solidityPayable {
        function() external payable {}
    }
*/
std::string solidityPayableCode = "6080604052348015600f57600080fd5b50603280601d6000396000f3fe608060405200fea165627a7a72305820b860bcee58741a7fceefcfb571356ef0a5da01abba6d9fdb7e6bc160c8cd402f0029";

/*
    pragma solidity >=0.4.22 <0.6.0;
    contract soliditySuicide {
        address payable addr = 0x0100000000000000000000000000000000000000;
        function sui() public {
            selfdestruct(addr);
        }
        function() external payable {}
        constructor () public payable {}
    }
*/
std::string soliditySuicideCode = "60806040527301000000000000000000000000000000000000006000806101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff16021790555060b5806100666000396000f3fe6080604052600436106039576000357c010000000000000000000000000000000000000000000000000000000090048063c421249a14603b575b005b348015604657600080fd5b50604d604f565b005b6000809054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16fffea165627a7a72305820480d2be102167c8ebd7c740c33277236ae0ec15bc7a84e106f80de2780c0d9560029";

/*
    pragma solidity >=0.4.22 <0.6.0;
    
    contract Contract {
        uint Name;
    
        constructor(uint name) public {
            Name = name;
        }
    }
    
    contract Factory {
        uint name = 0;
        function createContract() public {
            Contract newContract = new Contract(name);
            name += 1;
        } 
    }
*/
std::string solidityFactoryCode = "60806040526000805534801561001457600080fd5b50610163806100246000396000f3fe608060405234801561001057600080fd5b5060043610610048576000357c010000000000000000000000000000000000000000000000000000000090048063412a5a6d1461004d575b600080fd5b610055610057565b005b60008054604051610067906100a3565b80828152602001915050604051809103906000f08015801561008d573d6000803e3d6000fd5b5090506001600080828254019250508190555050565b6088806100b08339019056fe6080604052348015600f57600080fd5b50604051602080608883398101806040526020811015602d57600080fd5b8101908080519060200190929190505050806000819055505060358060536000396000f3fe6080604052600080fdfea165627a7a72305820a80f36f6424d98a4d04470934d8fc703539b49448199a262c854afb14abe0f6f0029a165627a7a72305820133cfecea043f1084c7a476f7a21ef3eb2be393714cc2528a373492217b9cfb50029";

/*
    pragma solidity >=0.4.22 <0.6.0;
    
    contract ContractReturn {
        function retFunc() public returns (uint){
            return 0x1a;
        }
    }
*/
std::string solidityReturnCode = "6080604052348015600f57600080fd5b50609b8061001e6000396000f3fe6080604052348015600f57600080fd5b50600436106045576000357c0100000000000000000000000000000000000000000000000000000000900480632f74848214604a575b600080fd5b60506066565b6040518082815260200191505060405180910390f35b6000601a90509056fea165627a7a7230582068db50066b6123e7cc36df93844beff5302de144be65a895b971fc44910fcf790029";

BOOST_AUTO_TEST_CASE( storage_test ){
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    transaction_evaluation_state context(&db);

    asset fee(0, asset_id_type());
  
    execute_contract(context, db, account_id_type(5), 0, solidityAddCode, asset_id_type(), fee, optional<contract_id_type>());
    execute_contract(context, db, account_id_type(5), 0, "4f2be91f", asset_id_type(), fee, contract_id_type(0));

    auto storage = get_evm_state().storage(id_to_address(contract_id_type(0).instance.value, 1));
    for (auto e : storage) {
        if (u256_to_string(e.second.first) == "0") {
            BOOST_CHECK(u256_to_string(e.second.second) == "27");
        }
    }
}

BOOST_AUTO_TEST_CASE( suicide_test ){
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    transaction_evaluation_state context(&db);

    asset fee(0, asset_id_type());
    execute_contract(context, db, account_id_type(5), 0, solidityAddCode, asset_id_type(), fee, optional<contract_id_type>());
    execute_contract(context, db, account_id_type(5), 0, solidityAddCode, asset_id_type(), fee, optional<contract_id_type>());
    execute_contract(context, db, account_id_type(5), 0, soliditySuicideCode, asset_id_type(), fee, optional<contract_id_type>());
    execute_contract(context, db, account_id_type(5), 0, "4f2be91f", asset_id_type(), fee, contract_id_type(0));

    auto& index = db.get_index_type<contract_index>().indices().get<by_id>();
    BOOST_CHECK(get_evm_state().addresses().size() == 3);
    BOOST_CHECK(index.end() != index.find(contract_id_type(0)));
    BOOST_CHECK(index.end() != index.find(contract_id_type(1)));
    BOOST_CHECK(index.end() != index.find(contract_id_type(2)));

    execute_contract(context, db, account_id_type(5), 0, "c421249a", asset_id_type(), fee, contract_id_type(2));

    BOOST_CHECK(get_evm_state().addresses().size() == 2);
    BOOST_CHECK(index.end() != index.find(contract_id_type(0)));
    BOOST_CHECK(index.end() != index.find(contract_id_type(1)));
    BOOST_CHECK(index.end() != index.find(contract_id_type(2)));
}

BOOST_AUTO_TEST_CASE( contract_create_contract_test ){
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    transaction_evaluation_state context(&db);

    asset fee(0, asset_id_type());
    execute_contract(context, db, account_id_type(5), 0, solidityFactoryCode, asset_id_type(), fee, optional<contract_id_type>());

    BOOST_CHECK(get_evm_state().addresses().size() == 1);

    for(size_t i = 0; i < 10; i++) {
        execute_contract(context, db, account_id_type(5), 0, "412a5a6d", asset_id_type(), fee, contract_id_type(0));
    }

    auto& index = db.get_index_type<contract_index>().indices().get<by_id>();
    BOOST_CHECK(get_evm_state().addresses().size() == 11);
    for(size_t i = 0; i < 11; i++) {
        BOOST_CHECK(index.end() != index.find(contract_id_type(i)));
    }
}

BOOST_AUTO_TEST_CASE( fee_test ){
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    transaction_evaluation_state context(&db);

    asset fee(0, asset_id_type());
    execute_contract(context, db, account_id_type(5), 0, solidityAddCode, asset_id_type(), fee, optional<contract_id_type>());

    const simple_index<account_statistics_object>& statistics_index = db.get_index_type<simple_index<account_statistics_object>>();
    auto iter = statistics_index.begin();
    for(size_t i = 0; i < 5; i++)
        iter++;
    const account_statistics_object& a = *iter;
    BOOST_CHECK(a.pending_fees + a.pending_vested_fees == 108843);

    asset balance_sender = db.get_balance(account_id_type(5), asset_id_type());
    BOOST_CHECK(1000000000 - 108843 == balance_sender.amount);
}

BOOST_AUTO_TEST_CASE( not_CORE_fee_test ){
    asset_object temp_asset = create_user_issued_asset("ETB");
    issue_uia( account_id_type(5), temp_asset.amount( 1000000 ) );
    transaction_evaluation_state context(&db);

    asset_fund_fee_pool_operation op_fund_fee_pool;
    op_fund_fee_pool.asset_id = asset_id_type(1);
    op_fund_fee_pool.from_account = account_id_type(0);
    op_fund_fee_pool.amount = 1000000;

    operation_result result = db.apply_operation( context, op_fund_fee_pool );

    contract_operation contract_op;
    contract_op.vm_type = vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type(1));
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(1), 0, asset_id_type(1),1, 1000000, solidityAddCode } );

    db._evaluating_from_block = true;
    result = db.apply_operation( context, contract_op );
    db._evaluating_from_block = false;

    asset balance1 = db.get_balance(account_id_type(0), asset_id_type());
    asset balance2 = db.get_balance(account_id_type(5), asset_id_type());
    asset balance3 = db.get_balance(account_id_type(5), temp_asset.get_id());

    const simple_index<account_statistics_object>& statistics_index = db.get_index_type<simple_index<account_statistics_object>>();
    auto iter = statistics_index.begin();
    for(size_t i = 0; i < 5; i++)
        iter++;
    const account_statistics_object& a = *iter;

    BOOST_CHECK(a.pending_fees + a.pending_vested_fees == 108843);
    BOOST_CHECK(balance1.amount.value == 1000000000000000 - 1000000);
    BOOST_CHECK(balance2.amount.value == 0);
    BOOST_CHECK(balance3.amount.value == 1000000 - 108843);
}

BOOST_AUTO_TEST_CASE( many_asset_contract_test ){
    asset_object temp_asset = create_user_issued_asset("ETB");
    issue_uia( account_id_type(5), temp_asset.amount( 10000000 ) );
    transfer(account_id_type(0),account_id_type(5),asset(10000000, asset_id_type()));

    transaction_evaluation_state context(&db);
    asset_fund_fee_pool_operation op_fund_fee_pool;
    op_fund_fee_pool.asset_id = asset_id_type(1);
    op_fund_fee_pool.from_account = account_id_type(0);
    op_fund_fee_pool.amount = 10000000;
    operation_result result = db.apply_operation( context, op_fund_fee_pool );

    asset fee(0, asset_id_type(1));
    execute_contract(context, db, account_id_type(5), 0, solidityPayableCode, asset_id_type(1), fee, optional<contract_id_type>());
    execute_contract(context, db, account_id_type(5), 50000, soliditySuicideCode, asset_id_type(1), fee, optional<contract_id_type>());

    fee = asset(0, asset_id_type(1));
    execute_contract(context, db, account_id_type(5), 50000, solidityPayableCode, asset_id_type(1), fee, optional<contract_id_type>());
    fee = asset(0, asset_id_type());
    execute_contract(context, db, account_id_type(5), 50000, "00", asset_id_type(), fee, contract_id_type(1));

    BOOST_CHECK(db.get_balance(contract_id_type(1), asset_id_type(0)) == asset(50000, asset_id_type(0)));
    BOOST_CHECK(db.get_balance(contract_id_type(1), asset_id_type(1)) == asset(50000, asset_id_type(1)));

    // suicide contract_id_type(1)
    // All assets must transfer to contract_id_type(0)
    execute_contract(context, db, account_id_type(5), 0, "c421249a", asset_id_type(), fee, contract_id_type(1));

    auto& index = db.get_index_type<result_contract_index>().indices().get<by_id>();
    auto itr = index.find(contract_id_type(1));
    BOOST_CHECK(itr == index.end());
    BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(0)) == asset(50000, asset_id_type(0)));
    BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(1)) == asset(50000, asset_id_type(1)));
}

BOOST_AUTO_TEST_CASE( result_contract_object_test ){
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    transaction_evaluation_state context(&db);

    asset fee(0, asset_id_type());
    execute_contract(context, db, account_id_type(5), 0, solidityReturnCode, asset_id_type(), fee, optional<contract_id_type>());
    asset balance1 = db.get_balance(account_id_type(5), asset_id_type());
    execute_contract(context, db, account_id_type(5), 0, "2f748482", asset_id_type(), fee, contract_id_type(0));
    asset balance2 = db.get_balance(account_id_type(5), asset_id_type());

    auto& index = db.get_index_type<result_contract_index>().indices().get<by_id>();
    auto itr = index.find(result_contract_id_type(1));
    result_contract_object object = *itr;

    auto raw_res = db.db_res.get_results( std::string( (object_id_type)result_contract_id_type(1) ) );

    BOOST_CHECK( raw_res.valid() );
    auto res = fc::raw::unpack< vms::evm::evm_result >( *raw_res );

    BOOST_CHECK(object.contracts_ids.size() == 1);
    BOOST_CHECK(res.exec_result.gasUsed == u256(21468));
    BOOST_CHECK(res.exec_result.newAddress == Address("0000000000000000000000000000000000000000"));
    BOOST_CHECK(res.exec_result.codeDeposit == CodeDeposit::None);
    BOOST_CHECK(res.exec_result.excepted == TransactionException::None);
    BOOST_CHECK(res.exec_result.output == dev::bytes({0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                                            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1a}));
    BOOST_CHECK(res.receipt.cumulativeGasUsed() == u256(21468));
    BOOST_CHECK(res.receipt.log().size() == 0);
}

/*
    pragma solidity >=0.4.22 <0.6.0;
    contract solidityPayable {
        function() external payable {}
    }
*/
std::string solidityCode = "6080604052348015600f57600080fd5b50603280601d6000396000f3fe608060405200fea165627a7a72305820b860bcee58741a7fceefcfb571356ef0a5da01abba6d9fdb7e6bc160c8cd402f0029";

/*
    pragma solidity >=0.4.22 <0.6.0;
    contract solidityTransfer {
      function tr() public {
        address payable addr = 0x0000000000000000000000000000000000000003;
        addr.transfer(10000);
      }
      function() external payable {}
    }
*/
std::string solidityTransferCode = "608060405234801561001057600080fd5b5060cc8061001f6000396000f3fe6080604052600436106039576000357c010000000000000000000000000000000000000000000000000000000090048063c5917cbd14603b575b005b348015604657600080fd5b50604d604f565b005b6000600390508073ffffffffffffffffffffffffffffffffffffffff166108fc6127109081150290604051600060405180830381858888f19350505050158015609c573d6000803e3d6000fd5b505056fea165627a7a72305820ca0e36f1a07ff7552acf00484ca8a92f408d9dfde989a895524b271dfca93ba00029";

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
std::string soliditySendCode = "608060405234801561001057600080fd5b506101e1806100206000396000f3fe60806040526004361061003b576000357c010000000000000000000000000000000000000000000000000000000090048063178a18fe1461003d575b005b34801561004957600080fd5b50610052610054565b005b6000730100000000000000000000000000000000000000905060008173ffffffffffffffffffffffffffffffffffffffff166040516024016040516020818303038152906040527fc5917cbd000000000000000000000000000000000000000000000000000000007bffffffffffffffffffffffffffffffffffffffffffffffffffffffff19166020820180517bffffffffffffffffffffffffffffffffffffffffffffffffffffffff83818316178352505050506040518082805190602001908083835b6020831061013c5780518252602082019150602081019050602083039250610119565b6001836020036101000a0380198251168184511680821785525050505050509050019150506000604051808303816000865af19150503d806000811461019e576040519150601f19603f3d011682016040523d82523d6000602084013e6101a3565b606091505b50509050806101b157600080fd5b505056fea165627a7a72305820c1e392e6a523ec0ec71ea73780ba8290ddb295a0c7f9d0b1437a99e25174fb160029";

/*
    pragma solidity >=0.4.22 <0.6.0;
    contract solidityTest {
        function addAmount() external payable {}
        function send() public {
            msg.sender.transfer(address(this).balance);
        }
    }
*/

std::string solidityTestCode = "608060405234801561001057600080fd5b5060f18061001f6000396000f3fe6080604052600436106043576000357c010000000000000000000000000000000000000000000000000000000090048063b46300ec146048578063b9c1457714605c575b600080fd5b348015605357600080fd5b50605a6064565b005b606260c3565b005b3373ffffffffffffffffffffffffffffffffffffffff166108fc3073ffffffffffffffffffffffffffffffffffffffff16319081150290604051600060405180830381858888f1935050505015801560c0573d6000803e3d6000fd5b50565b56fea165627a7a723058202818d6274b168dd38ea21e98f1d8e7893bc43d783176b937edd88861f188e0270029";

BOOST_AUTO_TEST_CASE( transfer_money_many_asset_acc_to_contr_test ){

    std::vector<asset_object> assets = { create_user_issued_asset("ETB"), create_user_issued_asset("EEB"), create_user_issued_asset("EEE") };
    transaction_evaluation_state context(&db);
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    for(asset_object& a : assets) {
        issue_uia( account_id_type(5), a.amount( 10000000 ) );
        asset_fund_fee_pool_operation op_fund_fee_pool;
        op_fund_fee_pool.asset_id = a.id;
        op_fund_fee_pool.from_account = account_id_type(0);
        op_fund_fee_pool.amount = 10000000;
        db.apply_operation( context, op_fund_fee_pool );
    }

    asset fee(0, asset_id_type());
    execute_contract(context, db, account_id_type(5), 0, solidityCode, asset_id_type(), fee, optional<contract_id_type>());

    for(size_t i = 0; i < 4; i++) {
        asset assetSender = db.get_balance(account_id_type(5), asset_id_type(i));
        fee = asset(0, asset_id_type(i));
        execute_contract(context, db, account_id_type(5), 50000, "", asset_id_type(i), fee, contract_id_type(0));

        auto raw_res = db.db_res.get_results( std::string( (object_id_type)result_contract_id_type(i+1) ) );

        BOOST_CHECK( raw_res.valid() );
        auto res = fc::raw::unpack< vms::evm::evm_result >( *raw_res );

        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        BOOST_CHECK(db.get_balance(account_id_type(5), asset_id_type(i)) == asset(assetSender.amount - 50000 - res.exec_result.gasUsed, asset_id_type(i)));
    }
}

BOOST_AUTO_TEST_CASE( transfer_money_many_asset_contr_to_acc_test ){
    std::vector<asset_object> assets = { create_user_issued_asset("ETB"), create_user_issued_asset("EEB"), create_user_issued_asset("EEE") };
    transaction_evaluation_state context(&db);
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    for(asset_object& a : assets) {
        issue_uia( account_id_type(5), a.amount( 10000000 ) );
        asset_fund_fee_pool_operation op_fund_fee_pool;
        op_fund_fee_pool.asset_id = a.id;
        op_fund_fee_pool.from_account = account_id_type(0);
        op_fund_fee_pool.amount = 10000000;
        db.apply_operation( context, op_fund_fee_pool );
    }

    asset fee(0, asset_id_type());
    execute_contract(context, db, account_id_type(5), 0, solidityTransferCode, asset_id_type(), fee, optional<contract_id_type>());

    for(size_t i = 0; i < 4; i++) {
        asset assetSender = db.get_balance(account_id_type(5), asset_id_type(i));
        fee = asset(0, asset_id_type(i));
        execute_contract(context, db, account_id_type(5), 50000, "", asset_id_type(i), fee, contract_id_type(0));

        auto raw_res = db.db_res.get_results( std::string( (object_id_type)result_contract_id_type(i+1) ) );

        BOOST_CHECK( raw_res.valid() );
        auto res = fc::raw::unpack< vms::evm::evm_result >( *raw_res );

        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        BOOST_CHECK(db.get_balance(account_id_type(5), asset_id_type(i)) == asset(assetSender.amount - 50000 - res.exec_result.gasUsed, asset_id_type(i)));
    }

    for(size_t i = 0; i < 4; i++) {
        fee = asset(0, asset_id_type(i));
        execute_contract(context, db, account_id_type(5), 0, "c5917cbd", asset_id_type(i), fee, contract_id_type(0));
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(40000, asset_id_type(i)));
        BOOST_CHECK(db.get_balance(account_id_type(3), asset_id_type(i)) == asset(10000, asset_id_type(i)));
    }
}

BOOST_AUTO_TEST_CASE( transfer_money_many_asset_contr_to_acc_depth_test ){
    std::vector<asset_object> assets = { create_user_issued_asset("ETB"), create_user_issued_asset("EEB"), create_user_issued_asset("EEE") };
    transaction_evaluation_state context(&db);
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    for(asset_object& a : assets) {
        issue_uia( account_id_type(5), a.amount( 10000000 ) );
        asset_fund_fee_pool_operation op_fund_fee_pool;
        op_fund_fee_pool.asset_id = a.id;
        op_fund_fee_pool.from_account = account_id_type(0);
        op_fund_fee_pool.amount = 10000000;
        db.apply_operation( context, op_fund_fee_pool );
    }

    asset fee(0, asset_id_type());
    execute_contract(context, db, account_id_type(5), 0, solidityTransferCode, asset_id_type(), fee, optional<contract_id_type>());
    execute_contract(context, db, account_id_type(5), 0, soliditySendCode, asset_id_type(), fee, optional<contract_id_type>());

    for(size_t i = 0; i < 4; i++) {
        asset assetSender = db.get_balance(account_id_type(5), asset_id_type(i));
        fee = asset(0, asset_id_type(i));
        execute_contract(context, db, account_id_type(5), 50000, "", asset_id_type(i), fee, contract_id_type(0));

        auto raw_res = db.db_res.get_results( std::string( (object_id_type)result_contract_id_type(i+2) ) );

        BOOST_CHECK( raw_res.valid() );
        auto res = fc::raw::unpack< vms::evm::evm_result >( *raw_res );

        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        BOOST_CHECK(db.get_balance(account_id_type(5), asset_id_type(i)) == asset(assetSender.amount - 50000 - res.exec_result.gasUsed, asset_id_type(i)));
    }

    for(size_t i = 0; i < 4; i++) {
        fee = asset(0, asset_id_type(i));
        execute_contract(context, db, account_id_type(5), 0, "178a18fe", asset_id_type(i), fee, contract_id_type(1));
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(40000, asset_id_type(i)));
        BOOST_CHECK(db.get_balance(account_id_type(3), asset_id_type(i)) == asset(10000, asset_id_type(i)));
    }
}

BOOST_AUTO_TEST_CASE( transfer_asset_contr_to_acc_method_transfer ){
    asset_object temp_asset = create_user_issued_asset("ETB");
    issue_uia( account_id_type(5), temp_asset.amount( 10000000 ) );
    transaction_evaluation_state context(&db);

    asset_fund_fee_pool_operation op_fund_fee_pool;
    op_fund_fee_pool.asset_id = asset_id_type(1);
    op_fund_fee_pool.from_account = account_id_type(0);
    op_fund_fee_pool.amount = 10000000;

    operation_result result = db.apply_operation( context, op_fund_fee_pool );

    asset fee(0, asset_id_type(1));
    execute_contract(context, db, account_id_type(5), 0, solidityTestCode, asset_id_type(1), fee, optional<contract_id_type>());

    asset assetAccount = db.get_balance(account_id_type(5), asset_id_type(1));
    execute_contract(context, db, account_id_type(5), 500000, "b9c14577", asset_id_type(1), fee, contract_id_type(0));

    auto raw_res = db.db_res.get_results( std::string( (object_id_type)result_contract_id_type(1) ) );

    BOOST_CHECK( raw_res.valid() );
    auto res = fc::raw::unpack< vms::evm::evm_result >( *raw_res );

    asset expectedBalance(assetAccount.amount.value - (500000 + uint64_t(res.exec_result.gasUsed)), asset_id_type(1));
    BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(1)) == asset(500000, asset_id_type(1)));
    BOOST_CHECK(db.get_balance(account_id_type(5), asset_id_type(1)) == expectedBalance);

    assetAccount = db.get_balance(account_id_type(5), asset_id_type(1));
    execute_contract(context, db, account_id_type(5), 0, "b46300ec", asset_id_type(1), fee, contract_id_type(0));

    raw_res = db.db_res.get_results( std::string( (object_id_type)result_contract_id_type(2) ) );

    BOOST_CHECK( raw_res.valid() );
    res = fc::raw::unpack< vms::evm::evm_result >( *raw_res );

    expectedBalance = asset((assetAccount.amount.value - uint64_t(res.exec_result.gasUsed)) + 500000, asset_id_type(1));
    BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(1)) == asset(0, asset_id_type(1)));
    BOOST_CHECK(db.get_balance(account_id_type(5), asset_id_type(1)) == expectedBalance);
}

BOOST_AUTO_TEST_SUITE_END()
