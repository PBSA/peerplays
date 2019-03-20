#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"
#include <graphene/chain/contract_object.hpp>
#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/result_contract_object.hpp>

#include <fc/filesystem.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

namespace graphene { namespace app { namespace detail {
genesis_state_type create_example_genesis();
} } }

BOOST_FIXTURE_TEST_SUITE( contract_transfer_operation_tests, database_fixture )

/*
    pragma solidity >=0.4.22 <0.6.0;
    contract solidity {
        function addAmount() external payable {}

        function send() public {
            msg.sender.transfer(address(this).balance);
        }
    }
*/

std::string solidityCode = "608060405234801561001057600080fd5b5060f18061001f6000396000f3fe6080604052600436106043576000357c010000000000000000000000000000000000000000000000000000000090048063b46300ec146048578063b9c1457714605c575b600080fd5b348015605357600080fd5b50605a6064565b005b606260c3565b005b3373ffffffffffffffffffffffffffffffffffffffff166108fc3073ffffffffffffffffffffffffffffffffffffffff16319081150290604051600060405180830381858888f1935050505015801560c0573d6000803e3d6000fd5b50565b56fea165627a7a7230582032657a7dd53143a653361fe021665b8be9e075bf3a9644e542e703fd07478c020029";

/*
    pragma solidity >=0.4.22 <0.6.0;
    contract soliditySuicide {
        function sui(address payable addr) public {
            selfdestruct(addr);
        }
        function() external payable {}
    }
*/

std::string soliditySuicideCode = "608060405234801561001057600080fd5b5060cd8061001f6000396000f3fe6080604052600436106039576000357c01000000000000000000000000000000000000000000000000000000009004806365d2db5314603b575b005b348015604657600080fd5b50608660048036036020811015605b57600080fd5b81019080803573ffffffffffffffffffffffffffffffffffffffff1690602001909291905050506088565b005b8073ffffffffffffffffffffffffffffffffffffffff16fffea165627a7a72305820a8877a5ef185d8bd7d3316ce1bbc717d5bd6cc425aaa752ee38585be06efa85f0029";

/*
    pragma solidity >=0.4.22 <0.6.0;
    contract solidityPayable {
        function() external payable {}
    }
*/

std::string solidityPayableCode = "6080604052348015600f57600080fd5b50603280601d6000396000f3fe608060405200fea165627a7a72305820b860bcee58741a7fceefcfb571356ef0a5da01abba6d9fdb7e6bc160c8cd402f0029";

BOOST_AUTO_TEST_CASE( transfer_CORE_acc_to_contr_and_contr_to_acc ) {
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    transaction_evaluation_state context(&db);

    signed_transaction trx;
    set_expiration( db, trx );

    contract_operation contract_op;
    contract_op.vm_type = vms::base::vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type());

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(0), 0, 0, 4000000, solidityCode });

    trx.operations.push_back( contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(), 500000, 1, 2000000, "b9c14577" });

    trx.operations.push_back( contract_op );
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(), 0, 1, 2000000, "b46300ec" });
    
    trx.operations.push_back( contract_op );

    PUSH_TX( db, trx, ~0 );
    auto init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash(string("null_key")) );
    db.generate_block( db.get_slot_time(1), db.get_scheduled_witness(1), init_account_priv_key, ~0 );

    vector<operation_history_object> result;
    const auto& stats = account_id_type(5)(db).statistics(db);
    const account_transaction_history_object* node = &stats.most_recent_op(db);
    while( node ) {
        result.push_back( node->operation_id(db) );
        if( node->next == account_transaction_history_id_type() )
            node = nullptr;
        else node = &node->next(db);
    }
    
    BOOST_CHECK(result.size() == 5);
    auto& virtual_op = result[0].op.get<contract_transfer_operation>();
    auto& op = result[2].op.get<contract_operation>();
    BOOST_CHECK( virtual_op.fee == asset(0, asset_id_type()) && virtual_op.amount == asset(500000, asset_id_type()) );
    BOOST_CHECK( virtual_op.from == contract_id_type() && virtual_op.to == account_id_type(5) );
    eth_op eth_data = fc::raw::unpack<eth_op>( op.data );
    BOOST_CHECK( op.fee == asset(0, asset_id_type()) && eth_data.value == 500000 && eth_data.code == "b9c14577" );
    BOOST_CHECK( eth_data.receiver == contract_id_type() && op.registrar == account_id_type(5) );
}
BOOST_AUTO_TEST_CASE( transfer_asset_acc_to_contr_and_contr_to_acc ) {
    transaction_evaluation_state context(&db);
    asset_object temp_asset = create_user_issued_asset("ETB");
    issue_uia( account_id_type(5), temp_asset.amount( 10000000 ) );
    transfer(account_id_type(0),account_id_type(5),asset(10000000, asset_id_type()));

    asset_fund_fee_pool_operation op_fund_fee_pool;
    op_fund_fee_pool.asset_id = asset_id_type(1);
    op_fund_fee_pool.from_account = account_id_type(5);
    op_fund_fee_pool.amount = 10000000;

    signed_transaction trx;
    set_expiration( db, trx );

    trx.operations.push_back(op_fund_fee_pool);

    contract_operation contract_op;
    contract_op.vm_type = vms::base::vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type(1));

    contract_op.data = fc::raw::unsigned_pack( eth_op{  contract_op.registrar, optional<contract_id_type>(), asset_id_type(1), 0, 1, 2000000, solidityCode });
    trx.operations.push_back( contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{  contract_op.registrar, contract_id_type(0), asset_id_type(1), 500000, 1, 2000000, "b9c14577" });
    trx.operations.push_back( contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{  contract_op.registrar, contract_id_type(0), asset_id_type(1), 0, 1, 2000000, "b46300ec" });
    trx.operations.push_back( contract_op );

    PUSH_TX( db, trx, ~0 );
    auto init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash(string("null_key")) );
    db.generate_block( db.get_slot_time(1), db.get_scheduled_witness(1), init_account_priv_key, ~0 );

    vector<operation_history_object> result;
    const auto& stats = account_id_type(5)(db).statistics(db);
    const account_transaction_history_object* node = &stats.most_recent_op(db);
    while( node ) {
        result.push_back( node->operation_id(db) );
        if( node->next == account_transaction_history_id_type() )
            node = nullptr;
        else node = &node->next(db);
    }
    
    BOOST_CHECK(result.size() == 7);
    auto& virtual_op = result[0].op.get<contract_transfer_operation>();
    auto& op = result[2].op.get<contract_operation>();
    BOOST_CHECK( virtual_op.fee == asset(0, asset_id_type(0)) && virtual_op.amount == asset(500000, asset_id_type(1)) );
    BOOST_CHECK( virtual_op.from == contract_id_type() && virtual_op.to == account_id_type(5) );
    eth_op eth_data = fc::raw::unpack<eth_op>( op.data );
    BOOST_CHECK( op.fee == asset(0, asset_id_type(1)) && eth_data.value == 500000 && eth_data.code == "b9c14577" );
    BOOST_CHECK( eth_data.receiver == contract_id_type() && op.registrar == account_id_type(5) );
}

BOOST_AUTO_TEST_CASE( suicide_contr_to_acc_and_contr_to_contr_CORE ) {
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    transaction_evaluation_state context(&db);

    signed_transaction trx;
    set_expiration( db, trx );

    contract_operation contract_op;
    contract_op.vm_type = vms::base::vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type());
    contract_op.data = fc::raw::unsigned_pack( eth_op{  contract_op.registrar, optional<contract_id_type>(), asset_id_type(), 0, 1, 2000000, soliditySuicideCode } );
    trx.operations.push_back(contract_op);
    trx.operations.push_back(contract_op);

    contract_op.data = fc::raw::unsigned_pack( eth_op{  contract_op.registrar, optional<contract_id_type>(), asset_id_type(), 0, 1, 2000000, solidityPayableCode } );
    trx.operations.push_back(contract_op);

    contract_op.data = fc::raw::unsigned_pack( eth_op{  contract_op.registrar, contract_id_type(0), asset_id_type(), 500000, 1, 2000000 } );
    trx.operations.push_back( contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{  contract_op.registrar, contract_id_type(1), asset_id_type(), 500000, 1, 2000000 } );
    trx.operations.push_back( contract_op );

    std::string code_one = "65d2db530000000000000000000000000000000000000000000000000000000000000005"; // function sui(address addr)
    contract_op.data = fc::raw::unsigned_pack( eth_op{  contract_op.registrar, contract_id_type(), asset_id_type(), 0, 1, 2000000, code_one });

    trx.operations.push_back( contract_op );

    std::string code_two = "65d2db530000000000000000000000000100000000000000000000000000000000000002"; // function sui(address addr)
    contract_op.data = fc::raw::unsigned_pack( eth_op{  contract_op.registrar, contract_id_type(1), asset_id_type(), 0, 1, 2000000, code_two });

    trx.operations.push_back( contract_op );

    PUSH_TX( db, trx, ~0 );
    auto init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash(string("null_key")) );
    db.generate_block( db.get_slot_time(1), db.get_scheduled_witness(1), init_account_priv_key, ~0 );

    vector<operation_history_object> account_history = get_operation_history( account_id_type( 5 ) );

    auto c0_hist = get_operation_history( contract_id_type( 0 ) );
    auto c1_hist = get_operation_history( contract_id_type( 1 ) );
    auto c2_hist = get_operation_history( contract_id_type( 2 ) );
    BOOST_CHECK( c0_hist.size() == 1 );
    BOOST_CHECK( c0_hist[0].op.which() == operation::tag< contract_transfer_operation >::value );
   
    BOOST_CHECK( c1_hist.size() == 1 );
    BOOST_CHECK( c1_hist[0].op.which() == operation::tag< contract_transfer_operation >::value );
   
    BOOST_CHECK( c2_hist.size() == 1 );
    BOOST_CHECK( c2_hist[0].op.which() == operation::tag< contract_transfer_operation >::value );
   
    BOOST_CHECK( account_history.size() == 9 ); // transfer + 6 * contract_op + 2 * contract_transfer
}

BOOST_AUTO_TEST_CASE( suicide_contr_to_acc_and_contr_to_contr_ASSET ) {
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    transaction_evaluation_state context(&db);

    asset_object temp_asset = create_user_issued_asset("ETB");
    issue_uia( account_id_type(5), temp_asset.amount( 10000000 ) );
   
    asset_fund_fee_pool_operation op_fund_fee_pool;
    op_fund_fee_pool.asset_id = asset_id_type(1);
    op_fund_fee_pool.from_account = account_id_type(5);
    op_fund_fee_pool.amount = 10000000;

    signed_transaction trx;
    set_expiration( db, trx );

    trx.operations.push_back(op_fund_fee_pool);

    contract_operation contract_op;
    contract_op.vm_type = vms::base::vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, temp_asset.id);
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), temp_asset.id, 0, 1, 2000000, soliditySuicideCode });
    trx.operations.push_back(contract_op);
    trx.operations.push_back(contract_op);

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), temp_asset.id, 0, 1, 2000000, solidityPayableCode });
    trx.operations.push_back(contract_op);

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), temp_asset.id, 500000, 1, 2000000 });

    trx.operations.push_back( contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(1), temp_asset.id, 500000, 1, 2000000 });

    trx.operations.push_back( contract_op );

    std::string str_code_one = "65d2db530000000000000000000000000000000000000000000000000000000000000005"; // function sui(address addr)
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(), temp_asset.id, 0, 1, 2000000, str_code_one });
    trx.operations.push_back( contract_op );

    std::string str_code_two = "65d2db530000000000000000000000000100000000000000000000000000000000000002"; // function sui(address addr)
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(1), temp_asset.id, 0, 1, 2000000, str_code_two });
    trx.operations.push_back( contract_op );

    PUSH_TX( db, trx, ~0 );
    auto init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash(string("null_key")) );
    db.generate_block( db.get_slot_time(1), db.get_scheduled_witness(1), init_account_priv_key, ~0 );

    vector<operation_history_object> account_history = get_operation_history( account_id_type( 5 ) );
   

    auto c0_hist = get_operation_history( contract_id_type( 0 ) );
    auto cto0 = c0_hist[0].op.get<contract_transfer_operation>();
    BOOST_CHECK( c0_hist.size() == 1 );
    BOOST_CHECK( cto0.amount.asset_id == asset_id_type(1));
    auto c1_hist = get_operation_history( contract_id_type( 1 ) );
    auto cto1 = c1_hist[0].op.get<contract_transfer_operation>();
    BOOST_CHECK( c1_hist.size() == 1 );
    BOOST_CHECK( cto1.amount.asset_id == asset_id_type(1));

    auto c2_hist = get_operation_history( contract_id_type( 2 ) );
    auto cto2 = c2_hist[0].op.get<contract_transfer_operation>();
    BOOST_CHECK( c2_hist.size() == 1 );
    BOOST_CHECK( cto2.amount.asset_id == asset_id_type(1));
    BOOST_CHECK( account_history.size() == 11 ); // transfer + create_asset + fund_fee_pool + 6 * contract_op + 2 * contract_transfer
}

BOOST_AUTO_TEST_SUITE_END()
