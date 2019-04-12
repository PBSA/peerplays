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

inline uint32_t get_gas_used( database& db, result_contract_id_type result_id ) {
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
    generate_block();

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
    generate_block();
    BOOST_CHECK( db.get_evm_params().block_gas_limit  == 1);
}

BOOST_AUTO_TEST_CASE( generate_block_gas_limit_reached_test ){
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
    auto skip_sigs = database::skip_transaction_signatures | database::skip_authority_check;
    INVOKE( change_block_gas_limit_test );
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
    BOOST_CHECK( !b.transactions.size() );
}

BOOST_AUTO_TEST_CASE( many_trxs_generate_block_gas_limit_reached_test ){
    generate_block();
    auto skip_sigs = database::skip_transaction_signatures | database::skip_authority_check;
    transaction_evaluation_state context(&db);
    
    transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));

    contract_operation contract_op;
    contract_op.vm_type = vm_types::EVM;
    contract_op.registrar = account_id_type(5);
    contract_op.fee = asset(0, asset_id_type());
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 1000000, solidityAddCode });
    
    signed_transaction trx;
    set_expiration( db, trx );
    trx.operations.push_back( contract_op );
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 1000000, "4f2be91f" } );
    trx.operations.push_back( contract_op );
    trx.sign( init_account_priv_key, db.get_chain_id() );
    PUSH_TX( db, trx, skip_sigs );
    generate_block( skip_sigs );
    BOOST_CHECK( get_contracts( db, { contract_id_type() } ).size() == 1 );

    /*
     * Gas cost of below is `exec_contract_gas` = approx 343014.
     * So we need `op_amount = BLOCK_GAS_LIMIT_EVM / exec_contract_gas` operations to reach block gas limit
     * We split it into several transactions: `amount_in_trx` in each
     * To do it, we will add some amount to op_amount to div evenly
     * Example: op_amount = 10, amount_in_trx = 3
     *          We need op_amount = 12 to div evenly into trxs
     *          So formula is `op_amount = op_amount + amount_in_trx - op_amount % amount_in_trx`
     *          `12 = 10 + 3 - 1`
     * If you want to change amount of trxs in block, you can manipulate with `amount_in_trx`
    */
    auto exec_contract_gas = get_gas_used( db, result_contract_id_type(1) );
    const uint32_t amount_in_trx = 10;
    uint32_t op_amount = BLOCK_GAS_LIMIT_EVM / exec_contract_gas;
    op_amount += op_amount % amount_in_trx == 0 ? amount_in_trx : amount_in_trx  - op_amount % amount_in_trx;
 
    std::vector< signed_transaction > trxs( op_amount / amount_in_trx );
    uint32_t _time = 1;
    for( auto& trx: trxs ){
        set_expiration( db, trx );
        trx.set_expiration( trx.expiration + fc::seconds(30 + _time++) );
        for( uint32_t op_index = 0; op_index < amount_in_trx; ++op_index ){
            trx.operations.push_back( contract_op );
        }
        trx.sign( init_account_priv_key, db.get_chain_id() );
        PUSH_TX( db, trx, skip_sigs  );
    }

    auto save_size = trxs.size();
    do{
        auto b = db.generate_block( db.get_slot_time(1), db.get_scheduled_witness(1), init_account_priv_key, skip_sigs );
        
        uint32_t found = 0;
        for( const auto& trx: trxs ){
           for( auto& block_trx: b.transactions ){
               if( block_trx.expiration == trx.expiration ){
                   ++found;
                   break;
               }
           }
        }
        if( trxs.size() == save_size ) {
            BOOST_CHECK( trxs.size() - found != 0 );
        } else {
            BOOST_REQUIRE( found );
        }
        trxs = vector<signed_transaction>( trxs.begin() + found, trxs.end() );
    } while( trxs.size() );

    generate_blocks( 10 );       
}

inline signed_block get_block_to_push( database& db ){
    auto skip_sigs = database::skip_transaction_signatures | database::skip_authority_check;
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

    trx.sign( init_account_priv_key, db.get_chain_id() );
    PUSH_TX( db, trx, skip_sigs );
    return db.generate_block( db.get_slot_time(1), db.get_scheduled_witness( 1 ), init_account_priv_key, skip_sigs );
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