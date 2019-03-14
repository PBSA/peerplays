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
// TODO: not working suite (2 failures)
BOOST_FIXTURE_TEST_SUITE( contract_transfer_operation_tests, database_fixture )

/*
    contract solidity {
        function addAmount() payable {}

        function send() public {
            msg.sender.transfer(this.balance);
        }
    }
*/

std::string solidityCode = "6060604052341561000c57fe5b5b60e38061001b6000396000f30060606040526000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff168063b46300ec146044578063b9c14577146053575bfe5b3415604b57fe5b6051605b565b005b605960b4565b005b3373ffffffffffffffffffffffffffffffffffffffff166108fc3073ffffffffffffffffffffffffffffffffffffffff16319081150290604051809050600060405180830381858888f19350505050151560b157fe5b5b565b5b5600a165627a7a72305820547c493469f888f1601628b24c96c98dd1e6be0e48b7c2f2d4ba10e7288a40850029";

/*
    contract soliditySuicide {
        function sui(address addr) {
            suicide(addr);
        }
        function() payable {}
    }
*/

std::string soliditySuicideCode = "6060604052341561000c57fe5b5b60c08061001b6000396000f30060606040523615603d576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff16806365d2db53146045575b60435b5b565b005b3415604c57fe5b6076600480803573ffffffffffffffffffffffffffffffffffffffff169060200190919050506078565b005b8073ffffffffffffffffffffffffffffffffffffffff16ff5b505600a165627a7a7230582038011d5383f869ea33c339a1b0f8556ac6f18e33d5258a16ccd18b4d3de5b5e30029";

/*
    contract solidityPayable {
        function() payable {}
    }
*/

std::string solidityPayableCode = "60606040523415600b57fe5b5b60398060196000396000f30060606040525b600b5b5b565b0000a165627a7a72305820b78de2de40744ce26d408c09eb352691ee05ca2570a58eb869764c9fceb274f30029";

BOOST_AUTO_TEST_CASE( transfer_CORE_acc_to_contr_and_contr_to_acc ) {
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    transaction_evaluation_state context(&db);

    signed_transaction trx;
    set_expiration( db, trx );

    contract_operation contract_op;
    contract_op.version_vm = 1;
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
    contract_op.version_vm = 1;
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
    contract_op.version_vm = 1;
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
    contract_op.version_vm = 1;
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
