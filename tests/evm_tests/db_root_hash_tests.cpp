#include <boost/test/unit_test.hpp>
#include "../common/database_fixture.hpp"

#include <graphene/chain/database.hpp>
#include <graphene/utilities/tempdir.hpp>
#include <graphene/chain/result_contract_object.hpp>

#include <evm_result.hpp>

#include <aleth/libdevcore/SHA3.h>
#include <aleth/libdevcore/RLP.h>

#include <fc/exception/exception.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

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

BOOST_FIXTURE_TEST_SUITE( db_root_hash_tests, database_fixture )

std::vector<contract_id_type> ids { contract_id_type(0),
                                    contract_id_type(1) };

genesis_state_type make_genesis() {
   genesis_state_type genesis_state;

   genesis_state.initial_timestamp = time_point_sec( GRAPHENE_TESTING_GENESIS_TIMESTAMP );

   auto init_account_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")));
   genesis_state.initial_active_witnesses = 10;
   for( int i = 0; i < genesis_state.initial_active_witnesses; ++i )
   {
      auto name = "init"+fc::to_string(i);
      genesis_state.initial_accounts.emplace_back(name,
                                                  init_account_priv_key.get_public_key(),
                                                  init_account_priv_key.get_public_key(),
                                                  true);
      genesis_state.initial_committee_candidates.push_back({name});
      genesis_state.initial_witness_candidates.push_back({name, init_account_priv_key.get_public_key()});
   }
   genesis_state.initial_parameters.current_fees->zero_all_fees();
   return genesis_state;
}

signed_block get_block_to_push( database& db ){
    auto skip_sigs = database::skip_transaction_signatures | database::skip_authority_check;
    auto init_account_priv_key  = fc::ecc::private_key::regenerate(fc::sha256::hash(string("null_key")) );
    public_key_type init_account_pub_key  = init_account_priv_key.get_public_key();
    const graphene::db::index& account_idx = db.get_index(protocol_ids, account_object_type);
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
    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 4000000, solidityCode });
    trx.operations.push_back( contract_op );

    contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 4000000, soliditySuicideCode });
    trx.operations.push_back( contract_op );

    trx.sign( init_account_priv_key, db.get_chain_id() );
    PUSH_TX( db, trx, skip_sigs );
    return db.generate_block( db.get_slot_time(1), db.get_scheduled_witness( 1 ), init_account_priv_key, skip_sigs );
}

inline void not_equal_root_hashes_tests( database_fixture& df, fc::ecc::private_key &init_account_priv_key, bool with_first = false ){
    fc::temp_directory dir1( graphene::utilities::temp_directory_path() ),
                       dir2( graphene::utilities::temp_directory_path() );
    database db1,
             db2;
    db1.open(dir1.path(), make_genesis);
    db2.open(dir2.path(), make_genesis);
    BOOST_CHECK( db1.get_chain_id() == db2.get_chain_id() );
    if( with_first ){
       db1.generate_block( db1.get_slot_time(1), db1.get_scheduled_witness( 1 ), init_account_priv_key, database::skip_transaction_signatures | database::skip_authority_check );
       db2.generate_block( db2.get_slot_time(1), db2.get_scheduled_witness( 1 ), init_account_priv_key, database::skip_transaction_signatures | database::skip_authority_check );
    }
    auto b = get_block_to_push( db1 );
    b.transactions[0].operations.pop_back();
    auto save_root_hash = db2.db_res.root_hash();

    PUSH_BLOCK( db2, b, database::skip_transaction_signatures | database::skip_authority_check 
                        | database::skip_witness_signature | database::skip_witness_schedule_check | database::skip_merkle_check );

    BOOST_CHECK( db2.db_res.root_hash() == save_root_hash );
    BOOST_CHECK( db1.fetch_block_by_number( 1 ).valid() );
    bool valid_second = db1.fetch_block_by_number( 2 ).valid();
    BOOST_CHECK( with_first ? valid_second : !valid_second );
    BOOST_CHECK( db2.head_block_num() == with_first ? 1 : 0 );

    auto raw_res = db2.db_res.get_results( std::string( (object_id_type)result_contract_id_type() ) );
    BOOST_CHECK( !raw_res.valid() );

    BOOST_CHECK( !df.get_contracts( db2, ids ).size() );
    BOOST_CHECK( df.get_contracts( db1, ids ).size() == 2 );
}

BOOST_AUTO_TEST_CASE( not_equal_root_hashes_with_first_block ) {
    not_equal_root_hashes_tests( *this, init_account_priv_key );
}

BOOST_AUTO_TEST_CASE( not_equal_root_hashes_with_not_first_block ) {
    not_equal_root_hashes_tests( *this, init_account_priv_key, true);
}

BOOST_AUTO_TEST_CASE( equal_root_hashes ) {
    fc::temp_directory dir1( graphene::utilities::temp_directory_path() ),
                       dir2( graphene::utilities::temp_directory_path() );
    database db1,
             db2;
    db1.open(dir1.path(), make_genesis);
    db2.open(dir2.path(), make_genesis);
    BOOST_CHECK( db1.get_chain_id() == db2.get_chain_id() );
    auto b = get_block_to_push( db1 );
    PUSH_BLOCK( db2, b, database::skip_transaction_signatures | database::skip_authority_check
                        | database::skip_witness_signature | database::skip_witness_schedule_check );
    BOOST_CHECK( get_contracts( db2, ids ).size() == 2 );
    auto raw_res = db2.db_res.get_results( std::string( (object_id_type)result_contract_id_type() ) );
    BOOST_CHECK( raw_res.valid() );
}

BOOST_AUTO_TEST_SUITE_END()
