#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/result_contract_object.hpp>
#include <pp_state.hpp>
#include <evm_result.hpp>
#include <evm_parameters.hpp>

#include <fc/exception/exception.hpp>
#include <graphene/utilities/tempdir.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace vms::evm;

inline uint64_t get_gas_used( database& db, result_contract_id_type result_id ) {
   auto raw_res = db.db_res.get_results( std::string( (object_id_type)result_id ) );
   auto res = fc::raw::unpack< vms::evm::evm_result >( *raw_res );
   return res.exec_result.gasUsed.convert_to<uint32_t>();
}

BOOST_FIXTURE_TEST_SUITE( block_gas_tests, database_fixture )

/*
    pragma solidity >=0.4.22 <0.6.0;
    contract solidityAdd {
        uint a = 26;
        uint[] structArray;
        function add() public {
            delete structArray;
            for(uint i=0; i<20; i++) {
                a = a * i;
                structArray.push(a);
            }
        }
        function() external payable {}
        constructor () public payable {}
    }
*/
std::string solidityAddCode = "6080604052601a60005561011e806100186000396000f3fe6080604052600436106039576000357c0100000000000000000000000000000000000000000000000000000000900480634f2be91f14603b575b005b348015604657600080fd5b50604d604f565b005b60016000605b919060b1565b60008090505b601481101560ae5780600054026000819055506001600054908060018154018082558091505090600182039060005260206000200160009091929091909150555080806001019150506061565b50565b508054600082559060005260206000209081019060cd919060d0565b50565b60ef91905b8082111560eb57600081600090555060010160d5565b5090565b9056fea165627a7a723058202ae68422489e465746996f2bca53ac306ecda0ebc7df1af13b0e1c5160172de00029";

BOOST_AUTO_TEST_CASE( change_block_gas_limit_test ) {
    auto skip_flags = database::skip_authority_check;
    db.clear_pending();

    db.modify(db.get_global_properties(), [](global_property_object& p) {
       p.parameters.committee_proposal_review_period = fc::hours(1).to_seconds();
    });

    {
        proposal_create_operation cop = proposal_create_operation::committee_proposal(db.get_global_properties().parameters, db.head_block_time());
        cop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
        cop.expiration_time = db.head_block_time() + *cop.review_period_seconds + 10;
        committee_member_update_global_parameters_operation uop;
        vms::evm::evm_parameters_extension evm_params;
        evm_params.block_gas_limit = 1;
        uop.new_parameters.extensions.value.evm_parameters = evm_params;
        cop.proposed_ops.emplace_back(uop);
    
        signed_transaction tx;
        set_expiration( db, tx );
        tx.operations.push_back( cop );
        sign( tx, init_account_priv_key );
        PUSH_TX( db, tx, skip_flags );
    }

    {
      proposal_update_operation uop;
      uop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
      uop.active_approvals_to_add = {get_account("init0").get_id(), get_account("init1").get_id(), get_account("init2").get_id(), get_account("init3").get_id(),
                                     get_account("init4").get_id(), get_account("init5").get_id(), get_account("init6").get_id(), get_account("init7").get_id()};
      signed_transaction tx;
      set_expiration( db, tx );
      tx.operations.push_back(uop);
      sign( tx, init_account_priv_key );
      PUSH_TX( db, tx, skip_flags );
      BOOST_CHECK(proposal_id_type()(db).is_authorized_to_execute(db));
    }

    generate_blocks(proposal_id_type()(db).expiration_time + 7);
    generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
    BOOST_CHECK( db.get_evm_params().block_gas_limit  == 1);
}

BOOST_AUTO_TEST_CASE( generate_block_gas_limit_reached_test ){
    INVOKE( change_block_gas_limit_test );
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    auto skip_sigs = database::skip_transaction_signatures | database::skip_authority_check;
    contract_operation contract_op;
    contract_op.vm_type = vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(10000000, asset_id_type());
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 1000000, solidityAddCode });
    
    signed_transaction trx;
    set_expiration( db, trx );
    trx.operations.push_back( contract_op );
    trx.sign( init_account_priv_key, db.get_chain_id() );

    PUSH_TX( db, trx, skip_sigs );
    auto save_head_block_num = db.head_block_num();
    auto b = generate_block( skip_sigs );
    BOOST_CHECK( db.head_block_num() - save_head_block_num == 1 );
    BOOST_CHECK( b.transactions.size() == 1 );
}

inline void push_trx_with_contract_op( database& db, std::string code, uint32_t skip_flags, bool contract_call = false ){
    contract_operation contract_op;
    contract_op.vm_type = vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type());
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_call ? contract_id_type() : optional<contract_id_type>(),
                                                       std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 1000000, code } );
    
    signed_transaction trx;
    set_expiration( db, trx );
    trx.operations.push_back( contract_op );
    PUSH_TX( db, trx, skip_flags );
}

inline void deploy_contract( database_fixture& df, std::string contractCode, uint32_t skip_flags = 0 ){
    df.transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));

    push_trx_with_contract_op( df.db, solidityAddCode, skip_flags );
    df.generate_block( skip_flags );
    BOOST_CHECK( df.get_contracts( df.db, { contract_id_type() } ).size() == 1 );
}

inline uint64_t deploy_contract_and_get_gas_for_op( database_fixture& df, uint32_t skip_flags = 0 ){
    deploy_contract( df, solidityAddCode, skip_flags );

    push_trx_with_contract_op( df.db, "4f2be91f", skip_flags, true );
    df.generate_block( skip_flags );
    return get_gas_used( df.db, result_contract_id_type(1) );
}

BOOST_AUTO_TEST_CASE( ops_have_big_gas_limit_test ){
    auto skip_sigs = database::skip_transaction_signatures | database::skip_authority_check;

    deploy_contract( *this, solidityAddCode, skip_sigs );

    const uint64_t gas_limit = 1000000;
    const uint8_t op_amount = 5;
    
    contract_operation contract_op;
    contract_op.vm_type = vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type());
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(),
                                                       std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, gas_limit, "4f2be91f" } );

    signed_transaction trx;
    set_expiration( db, trx );
    trx.set_expiration( trx.expiration + fc::seconds(100) );
    for( uint8_t counter = 0; counter < op_amount - 1; ++counter)
        trx.operations.push_back( contract_op );
    
    uint64_t new_gas_limit = BLOCK_GAS_LIMIT_EVM > gas_limit * (op_amount - 1) ? BLOCK_GAS_LIMIT_EVM - gas_limit * (op_amount - 1) : gas_limit;
    ++new_gas_limit; // to get block gas limit reached
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(),
                                                       std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, new_gas_limit, "4f2be91f" } );
    trx.operations.push_back( contract_op );

    PUSH_TX( db, trx, skip_sigs );
    auto save_head_block_num = db.head_block_num();
    auto b = generate_block( skip_sigs );
    BOOST_CHECK( db.head_block_num() - save_head_block_num == 1 );
    BOOST_CHECK( !b.transactions.size() );
}

/*
 *  In this test case we will create 3 trxs with different operations(but contract ops will be same) 
 *  1 - n * contract_op, transfer_op
 *  2 - n * contract_op, asset_create
 *  3 - n * contract_op
 */
inline void create_contract_operations( signed_transaction& trx, uint32_t n ){
    while( n-- > 0 ){
        contract_operation contract_op;
        contract_op.vm_type = vm_types::EVM;
        contract_op.registrar = account_id_type(5);
        contract_op.fee = asset(0, asset_id_type());
        contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(),
                                                           std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 1000000, "4f2be91f" } );
        trx.operations.push_back(contract_op);
    }
}

inline signed_transaction get_first_transaction( database_fixture& df, uint64_t exec_gas, uint32_t n, uint32_t skip_flags = 0 ){
    signed_transaction trx;
    set_expiration( df.db, trx );

    transfer_operation transfer_op;
    transfer_op.from   = account_id_type(0);
    transfer_op.to     = account_id_type(5);
    transfer_op.amount = asset(10, asset_id_type());
    trx.operations.push_back(transfer_op);
    
    create_contract_operations( trx, n );
    return trx;
}
inline signed_transaction get_second_transaction( database_fixture& df, uint64_t exec_gas, uint32_t n, uint32_t skip_flags = 0 ){
    signed_transaction trx;
    set_expiration( df.db, trx );

    asset_create_operation creator_op;
    creator_op.issuer = account_id_type(5);
    creator_op.fee = asset();
    creator_op.symbol = "ETB";
    creator_op.common_options.max_supply = 0;
    creator_op.precision = 2;
    creator_op.common_options.core_exchange_rate = price({asset(1,asset_id_type(1)),asset(1)});
    creator_op.common_options.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
    creator_op.common_options.flags = charge_market_fee;
    creator_op.common_options.issuer_permissions = charge_market_fee;
    trx.operations.push_back( creator_op );

    create_contract_operations( trx, n );
    return trx;
}
inline signed_transaction get_third_transaction( database_fixture& df, uint64_t exec_gas, uint32_t ops_amount, uint32_t n, uint32_t skip_flags = 0 ){
    signed_transaction trx;
    set_expiration( df.db, trx );

    auto reminder = ops_amount - 2 * n;
    create_contract_operations( trx, reminder );
    
    return trx;
}

BOOST_AUTO_TEST_CASE( many_trxs_generate_block_gas_limit_reached_test ){
    auto skip_sigs = database::skip_transaction_signatures | database::skip_authority_check;

    auto exec_gas = deploy_contract_and_get_gas_for_op( *this, skip_sigs );
    uint32_t ops_amount = BLOCK_GAS_LIMIT_EVM / exec_gas + 1;
    uint32_t n = ops_amount / 3;

    const auto& first_trx  = get_first_transaction( *this, exec_gas, n, skip_sigs );
    const auto& second_trx = get_second_transaction( *this, exec_gas, n, skip_sigs );
    const auto& third_trx  = get_third_transaction( *this, exec_gas, ops_amount, n, skip_sigs );

    db.clear_pending();
    PUSH_TX( db, first_trx, skip_sigs );
    PUSH_TX( db, second_trx, skip_sigs );
    PUSH_TX( db, third_trx, skip_sigs );

    auto b1 = db.generate_block( db.get_slot_time(1), db.get_scheduled_witness(1), init_account_priv_key, skip_sigs );
    auto b2 = db.generate_block( db.get_slot_time(1), db.get_scheduled_witness(1), init_account_priv_key, skip_sigs );
    auto b3 = db.generate_block( db.get_slot_time(1), db.get_scheduled_witness(1), init_account_priv_key, skip_sigs );

    BOOST_CHECK( b1.transactions.size() == 1 && b1.transactions[0].operations[0].which() == operation::tag<transfer_operation>::value );
    BOOST_CHECK( b2.transactions.size() == 1 && b2.transactions[0].operations[0].which() == operation::tag<asset_create_operation>::value );
    BOOST_CHECK( b3.transactions.size() == 1 && b3.transactions[0].operations[0].which() == operation::tag<contract_operation>::value );

    generate_blocks( 6 );       
}

inline signed_block get_block_to_push( database& db ){
    auto skip_flags = ~0 | database::skip_undo_history_check;
    auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")) );
    signed_transaction trx;
    set_expiration( db, trx );
    
    transfer_operation trans;
    trans.from = account_id_type(0);
    trans.to   = account_id_type(5);
    trans.amount = asset(1000000000, asset_id_type());
    trx.operations.push_back(trans);

    contract_operation contract_op;
    contract_op.vm_type = vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type());
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 4000000, solidityAddCode });
    trx.operations.push_back( contract_op );

    PUSH_TX( db, trx, skip_flags );
    return db.generate_block( db.get_slot_time(1), db.get_scheduled_witness( 1 ), init_account_priv_key, skip_flags );
}

BOOST_AUTO_TEST_CASE( apply_block_gas_limit_reached_test ){
    fc::temp_directory dir1( graphene::utilities::temp_directory_path() ),
                       dir2( graphene::utilities::temp_directory_path() );
    database db1,
             db2;
    db1.open(dir1.path(), make_genesis_for_db);
    db2.open(dir2.path(), make_genesis_for_db);
    BOOST_CHECK( db1.get_chain_id() == db2.get_chain_id() );
    auto b = get_block_to_push( db1 );
    
    db2.modify(db2.get_global_properties(), [](global_property_object& p) {
       p.parameters.extensions.value.evm_parameters->block_gas_limit = 1;
    });

    GRAPHENE_REQUIRE_THROW( db2.apply_block( b, ~0 ), fc::block_gas_limit_reached_exception );
}

BOOST_AUTO_TEST_SUITE_END()