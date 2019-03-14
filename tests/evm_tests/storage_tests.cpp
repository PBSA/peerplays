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

string u256_to_string(u256& data){
    std::stringstream ss;
    ss << data;
    return ss.str();
}

BOOST_FIXTURE_TEST_SUITE( storage_tests, database_fixture )

BOOST_AUTO_TEST_CASE( storage_test ){
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    transaction_evaluation_state context(&db);

    asset fee(0, asset_id_type());
    std::string code = "6060604052346000575b600d6000819055505b5b6052806100206000396000f360606040526000357c0100000000000000000000000000000000000000000000000000000000900480634f2be91f146036575b6000565b603c603e565b005b600d6000600082825401925050819055505b56";
    execute_contract(context, db, account_id_type(5), 0, code, asset_id_type(), fee, optional<contract_id_type>());
    execute_contract(context, db, account_id_type(5), 0, "4f2be91f", asset_id_type(), fee, contract_id_type(0));

    auto storage = get_evm_state().storage(id_to_address(contract_id_type(0).instance.value, 1));
    for (auto e : storage) {
        if (u256_to_string(e.second.first) == "0") {
            BOOST_CHECK(u256_to_string(e.second.second) == "26");
        }
    }
}

BOOST_AUTO_TEST_CASE( suicide_test ){
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    transaction_evaluation_state context(&db);

    asset fee(0, asset_id_type());
    std::string code1 = "6060604052346000575b60108060156000396000f360606040523615505b600e5b5b565b00";
    std::string code2 = "6060604052730100000000000000000000000000000000000001600060006101000a81548173ffffffffffffffffffffffffffffffffffffffff02191690836c0100000000000000000000000090810204021790555034610000575b6084806100686000396000f3606060405236156037576000357c01000000000000000000000000000000000000000000000000000000009004806341c0e1b514603f575b603d5b5b565b005b60456047565b005b600060009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16ff5b56";
    execute_contract(context, db, account_id_type(5), 0, code1, asset_id_type(), fee, optional<contract_id_type>());
    execute_contract(context, db, account_id_type(5), 0, code1, asset_id_type(), fee, optional<contract_id_type>());
    execute_contract(context, db, account_id_type(5), 0, code2, asset_id_type(), fee, optional<contract_id_type>());
    execute_contract(context, db, account_id_type(5), 0, "4f2be91f", asset_id_type(), fee, contract_id_type(0));

    auto& index = db.get_index_type<contract_index>().indices().get<by_id>();
    BOOST_CHECK(get_evm_state().addresses().size() == 3);
    BOOST_CHECK(index.end() != index.find(contract_id_type(0)));
    BOOST_CHECK(index.end() != index.find(contract_id_type(1)));
    BOOST_CHECK(index.end() != index.find(contract_id_type(2)));

    execute_contract(context, db, account_id_type(5), 0, "41c0e1b5", asset_id_type(), fee, contract_id_type(2));

    BOOST_CHECK(get_evm_state().addresses().size() == 2);
    BOOST_CHECK(index.end() != index.find(contract_id_type(0)));
    BOOST_CHECK(index.end() != index.find(contract_id_type(1)));
    BOOST_CHECK(index.end() != index.find(contract_id_type(2)));
}

BOOST_AUTO_TEST_CASE( contract_create_contract_test ){
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    transaction_evaluation_state context(&db);

    asset fee(0, asset_id_type());
    std::string code = "606060405234610000575b6102ca806100186000396000f360606040526000357c0100000000000000000000000000000000000000000000000000000000900480633f811b80146100435780636b8ff57414610060575b610000565b346100005761005e600480803590602001909190505061007d565b005b346100005761007b6004808035906020019091905050610149565b005b60008160405160a6806102248339018082600019168152602001915050604051809103906000f08015610000579050600180548060010182818154818355818115116100f5578183600052602060002091820191016100f491905b808211156100f05760008160009055506001016100d8565b5090565b5b505050916000526020600020900160005b83909190916101000a81548173ffffffffffffffffffffffffffffffffffffffff02191690836c01000000000000000000000000908102040217905550505b5050565b6000600182815481101561000057906000526020600020900160005b9054906101000a900473ffffffffffffffffffffffffffffffffffffffff1690508073ffffffffffffffffffffffffffffffffffffffff16638052474d600060405160200152604051817c0100000000000000000000000000000000000000000000000000000000028152600401809050602060405180830381600087803b156100005760325a03f1156100005750505060405180519060200150600083815481101561000057906000526020600020900160005b50819055505b505056606060405234610000576040516020806100a6833981016040528080519060200190919050505b806000819055505b505b60698061003d6000396000f3606060405236156037576000357c0100000000000000000000000000000000000000000000000000000000900480638052474d14603f575b603d5b5b565b005b3460005760496063565b604051808260001916815260200191505060405180910390f35b6000548156";
    execute_contract(context, db, account_id_type(5), 0, code, asset_id_type(), fee, optional<contract_id_type>());

    BOOST_CHECK(get_evm_state().addresses().size() == 1);

    for(size_t i = 0; i < 10; i++) {
        execute_contract(context, db, account_id_type(5), 0, "3f811b80", asset_id_type(), fee, contract_id_type(0));
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
    std::string code = "6060604052346000575b600d6000819055505b5b6052806100206000396000f360606040526000357c0100000000000000000000000000000000000000000000000000000000900480634f2be91f146036575b6000565b603c603e565b005b600d6000600082825401925050819055505b56";
    execute_contract(context, db, account_id_type(5), 0, code, asset_id_type(), fee, optional<contract_id_type>());

    const simple_index<account_statistics_object>& statistics_index = db.get_index_type<simple_index<account_statistics_object>>();
    auto iter = statistics_index.begin();
    for(size_t i = 0; i < 5; i++)
        iter++;
    const account_statistics_object& a = *iter;
    BOOST_CHECK(a.pending_fees + a.pending_vested_fees == 94797);

    asset balance_sender = db.get_balance(account_id_type(5), asset_id_type());
    BOOST_CHECK(1000000000 - 94797 == balance_sender.amount);
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
    contract_op.version_vm = 1;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type(1));
    std::string code_sol = string("6060604052346000575b600d6000819055505b5b6052806100206000396000f360606040526000357c0100000000000000000000000000000000000000000000000000000000900480634f2be91f146036575b6000565b603c603e565b005b600d6000600082825401925050819055505b56");
    contract_op.data = fc::raw::pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(1), 0, 1, 1000000, code_sol } );

    db._evaluating_from_apply_block = true;
    result = db.apply_operation( context, contract_op );
    db._evaluating_from_apply_block = false;

    asset balance1 = db.get_balance(account_id_type(0), asset_id_type());
    asset balance2 = db.get_balance(account_id_type(5), asset_id_type());
    asset balance3 = db.get_balance(account_id_type(5), temp_asset.get_id());

    const simple_index<account_statistics_object>& statistics_index = db.get_index_type<simple_index<account_statistics_object>>();
    auto iter = statistics_index.begin();
    for(size_t i = 0; i < 5; i++)
        iter++;
    const account_statistics_object& a = *iter;

    BOOST_CHECK(a.pending_fees + a.pending_vested_fees == 94797);
    BOOST_CHECK(balance1.amount.value == 1000000000000000 - 1000000);
    BOOST_CHECK(balance2.amount.value == 0);
    BOOST_CHECK(balance3.amount.value == 1000000 - 94797);
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
    std::string code1 = "6060604052346000575b60108060156000396000f360606040523615505b600e5b5b565b00";
    std::string code2 = "6060604052730100000000000000000000000000000000000000600060006101000a81548173ffffffffffffffffffffffffffffffffffffffff02191690836c010000000000000000000000009081020402179055505b5b5b6084806100656000396000f3606060405236156037576000357c01000000000000000000000000000000000000000000000000000000009004806341c0e1b514603f575b603d5b5b565b005b60456047565b005b600060009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16ff5b56";
    std::string code3 = "6060604052730100000000000000000000000000000000000000600060006101000a81548173ffffffffffffffffffffffffffffffffffffffff02191690836c010000000000000000000000009081020402179055505b5b5b6084806100656000396000f3606060405236156037576000357c01000000000000000000000000000000000000000000000000000000009004806341c0e1b514603f575b603d5b5b565b005b60456047565b005b600060009054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16ff5b56";
    execute_contract(context, db, account_id_type(5), 0, code1, asset_id_type(1), fee, optional<contract_id_type>());
    execute_contract(context, db, account_id_type(5), 50000, code2, asset_id_type(1), fee, optional<contract_id_type>());

    fee = asset(0, asset_id_type(1));
    execute_contract(context, db, account_id_type(5), 50000, code3, asset_id_type(1), fee, optional<contract_id_type>());
    fee = asset(0, asset_id_type());
    execute_contract(context, db, account_id_type(5), 50000, "00", asset_id_type(), fee, contract_id_type(1));

    BOOST_CHECK(db.get_balance(contract_id_type(1), asset_id_type(0)) == asset(50000, asset_id_type(0)));
    BOOST_CHECK(db.get_balance(contract_id_type(1), asset_id_type(1)) == asset(50000, asset_id_type(1)));

    // suicide contract_id_type(1)
    // All assets must transfer to contract_id_type(0)
    execute_contract(context, db, account_id_type(5), 0, "41c0e1b5", asset_id_type(), fee, contract_id_type(1));

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
    std::string code = "6060604052346000575b600d6000819055505b5b606e806100206000396000f360606040526000357c0100000000000000000000000000000000000000000000000000000000900480634f2be91f146036575b6000565b603c6052565b6040518082815260200191505060405180910390f35b6000600d60006000828254019250508190555060005490505b9056";
    execute_contract(context, db, account_id_type(5), 0, code, asset_id_type(), fee, optional<contract_id_type>());
    execute_contract(context, db, account_id_type(5), 0, "4f2be91f", asset_id_type(), fee, contract_id_type(0));

    auto& index = db.get_index_type<result_contract_index>().indices().get<by_id>();
    auto itr = index.find(result_contract_id_type(1));
    result_contract_object object = *itr;

    auto res = get_evm_state().getResult( std::string( (object_id_type)result_contract_id_type(1) ) );

    BOOST_CHECK(object.contracts_id.size() == 1);
    BOOST_CHECK(res->first.gasUsed == u256(26854));
    BOOST_CHECK(res->first.newAddress == Address("0000000000000000000000000000000000000000"));
    BOOST_CHECK(res->first.codeDeposit == CodeDeposit::None);
    BOOST_CHECK(res->first.excepted == TransactionException::None);
    BOOST_CHECK(res->first.output == dev::bytes({0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                                            0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1a}));
    BOOST_CHECK(res->second.cumulativeGasUsed() == u256(26854));
    BOOST_CHECK(res->second.log().size() == 0);
}

/*
    contract solidity {
        function() payable {}
    }
*/
std::string solidityCode = "60606040523415600b57fe5b5b60398060196000396000f3006060604052";

/*
    contract solidityTransfer {
    function tr() {
        address addr = 0x0000000000000000000000000000000000000003;
        addr.transfer(10000);
    }
    function() payable {}
}
*/
std::string solidityTransferCode = "6060604052341561000c57fe5b5b60cb8061001b6000396000f30060606040523615603d576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff168063c5917cbd146045575b60435b5b565b005b3415604c57fe5b60526054565b005b6000600390508073ffffffffffffffffffffffffffffffffffffffff166108fc6127109081150290604051809050600060405180830381858888f193505050501515609b57fe5b5b505600a165627a7a723058201f685dedd138d48bce571a1cf3c2809eaeb12a02414c9eedc9d19cad55accea70029";

/*
    contract soliditySend {
        function se() {
            address addr = 0x0100000000000000000000000000000000000000;
            addr.call(bytes4(sha3("tr()")));
        }
        function() payable {}
    }
*/
std::string soliditySendCode = "6060604052341561000c57fe5b5b6101588061001c6000396000f3006060604052361561003f576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff168063178a18fe14610048575b6100465b5b565b005b341561005057fe5b61005861005a565b005b600073010000000000000000000000000000000000000090508073ffffffffffffffffffffffffffffffffffffffff1660405180807f7472282900000000000000000000000000000000000000000000000000000000815250600401905060405180910390207c010000000000000000000000000000000000000000000000000000000090046040518163ffffffff167c010000000000000000000000000000000000000000000000000000000002815260040180905060006040518083038160008761646e5a03f192505050505b505600a165627a7a72305820f6d92b18e5e36a9eec335e89fe12d977f0e07b85685f6cfff2b6a0bcd28228e70029";

/*
    contract solidityTest {
        function addAmount() payable {}
        function send() public {
            msg.sender.transfer(this.balance);
        }
    }
*/

std::string solidityTestCode = "6060604052341561000c57fe5b5b60e38061001b6000396000f30060606040526000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff168063b46300ec146044578063b9c14577146053575bfe5b3415604b57fe5b6051605b565b005b605960b4565b005b3373ffffffffffffffffffffffffffffffffffffffff166108fc3073ffffffffffffffffffffffffffffffffffffffff16319081150290604051809050600060405180830381858888f19350505050151560b157fe5b5b565b5b5600a165627a7a723058209dc04782d603c1970f823bf002af9e600b8c4938141ef67717d89986476216a10029";

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
        const std::pair<ExecutionResult, TransactionReceipt>* res = get_evm_state().getResult( std::string( (object_id_type)result_contract_id_type( i + 1 ) ) );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        BOOST_CHECK(db.get_balance(account_id_type(5), asset_id_type(i)) == asset(assetSender.amount - 50000 - res->first.gasUsed, asset_id_type(i)));
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
        const std::pair<ExecutionResult, TransactionReceipt>* res = get_evm_state().getResult( std::string( (object_id_type)result_contract_id_type( i + 1 ) ) );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        BOOST_CHECK(db.get_balance(account_id_type(5), asset_id_type(i)) == asset(assetSender.amount - 50000 - res->first.gasUsed, asset_id_type(i)));
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
        const std::pair<ExecutionResult, TransactionReceipt>* res = get_evm_state().getResult( std::string( (object_id_type)result_contract_id_type( i + 2) ) );
        BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(i)) == asset(50000, asset_id_type(i)));
        BOOST_CHECK(db.get_balance(account_id_type(5), asset_id_type(i)) == asset(assetSender.amount - 50000 - res->first.gasUsed, asset_id_type(i)));
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

    const std::pair<ExecutionResult, TransactionReceipt>* res = get_evm_state().getResult( std::string( (object_id_type)result_contract_id_type(1) ) );
    asset expectedBalance(assetAccount.amount.value - (500000 + uint64_t(res->first.gasUsed)), asset_id_type(1));
    BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(1)) == asset(500000, asset_id_type(1)));
    BOOST_CHECK(db.get_balance(account_id_type(5), asset_id_type(1)) == expectedBalance);

    assetAccount = db.get_balance(account_id_type(5), asset_id_type(1));
    execute_contract(context, db, account_id_type(5), 0, "b46300ec", asset_id_type(1), fee, contract_id_type(0));
    res = get_evm_state().getResult( std::string( (object_id_type)result_contract_id_type(2) ) );
    expectedBalance = asset((assetAccount.amount.value - uint64_t(res->first.gasUsed)) + 500000, asset_id_type(1));
    BOOST_CHECK(db.get_balance(contract_id_type(0), asset_id_type(1)) == asset(0, asset_id_type(1)));
    BOOST_CHECK(db.get_balance(account_id_type(5), asset_id_type(1)) == expectedBalance);
}

BOOST_AUTO_TEST_SUITE_END()
