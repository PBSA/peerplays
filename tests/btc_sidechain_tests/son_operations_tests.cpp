#include <boost/test/unit_test.hpp>

#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

#include <graphene/chain/btc-sidechain/son.hpp>
#include <graphene/chain/btc-sidechain/son_operations_evaluator.hpp>

using namespace graphene::chain;
using namespace graphene::chain::test;

class test_create_son_member_evaluator: public create_son_member_evaluator {
public:
    test_create_son_member_evaluator( database& link_db ): 
                                      ptr_trx_state( new transaction_evaluation_state( &link_db ) )
    {
        trx_state = ptr_trx_state.get();
    }
    std::unique_ptr<transaction_evaluation_state> ptr_trx_state;
};

class test_delete_son_member_evaluator: public delete_son_member_evaluator {
public:
    test_delete_son_member_evaluator( database& link_db ): 
                                      ptr_trx_state( new transaction_evaluation_state( &link_db ) )
    {
        trx_state = ptr_trx_state.get();
    }
    std::unique_ptr<transaction_evaluation_state> ptr_trx_state;
};

BOOST_FIXTURE_TEST_SUITE( son_operation_tests, database_fixture )

const std::string test_url = "https://create_son_test";
const auto skip_flags = database::skip_authority_check | database::skip_fork_db;
const uint64_t max_asset = 1000000000000000;

inline account_update_operation get_acc_update_op( account_id_type acc, account_options acc_opt, flat_set<vote_id_type> votes, uint16_t sons_amount ){
    account_update_operation op;
    op.account = acc;
    op.new_options = acc_opt;
    op.new_options->votes = votes;
    op.new_options->num_son = sons_amount;
    return op;
}

inline void create_active_sons( database_fixture& df, uint8_t sons_amount = 3 ) {
    signed_transaction trx1;
    for( uint8_t i = 1; i <= sons_amount; ++i ) {
        son_member_create_operation op;
        op.url = test_url;
        op.owner_account = account_id_type(i);
        trx1.operations.push_back(op);
    }

    trx1.validate();
    test::set_expiration( df.db, trx1 );
    df.sign( trx1, df.private_key );
    PUSH_TX(df.db, trx1, skip_flags);

    df.generate_block(skip_flags);

    const auto& idx = df.db.get_index_type<son_member_index>().indices().get<by_account>();
    BOOST_REQUIRE( idx.size() == sons_amount );

    flat_set<vote_id_type> votes;
    for( auto iter: idx ){
        votes.insert( iter.vote_id );
    }
    signed_transaction trx2;
    for( uint8_t i = 1; i <= sons_amount; ++i ){
        auto account_id = account_id_type(i);
        df.transfer( account_id_type(), account_id, asset( 10000000, asset_id_type() ) );
        trx2.operations.push_back( get_acc_update_op( account_id, account_id(df.db).options, votes, sons_amount ) );
    }

    trx2.validate();
    test::set_expiration( df.db, trx2 );
    df.sign( trx2, df.private_key );
    PUSH_TX(df.db, trx2, skip_flags);

    df.generate_blocks( df.db.get_dynamic_global_properties().next_maintenance_time, true, skip_flags );

    BOOST_REQUIRE( df.db.get_global_properties().active_son_members.size() == sons_amount );

    df.generate_blocks(10);
}

inline son_member_id_type create_son_member( account_id_type acc, database& db ){
    test_create_son_member_evaluator test_eval( db );
    son_member_create_operation op;
    op.owner_account = acc;
    op.url = test_url;

    BOOST_REQUIRE_NO_THROW( test_eval.do_evaluate( op ) );
    return test_eval.do_apply( op );
}

inline std::vector<account_id_type> create_accounts( database_fixture& df, uint16_t amount, uint32_t asset_amount, uint16_t salt = 0 ){
    std::vector<account_id_type> result;
    for( uint16_t i = 0; i < amount; ++i) {
        test::set_expiration( df.db, df.trx );
        const auto account = df.create_account( "sonmember" + std::to_string(salt + i) );
        df.upgrade_to_lifetime_member( account );
        df.transfer( account_id_type(), account.get_id(), asset( asset_amount, asset_id_type() ) );
        result.push_back( account.get_id() );
    }
    return result;
}

inline std::map<vote_id_type, son_member_id_type> create_son_members( database& db, std::vector<account_id_type> accs ){
    std::map<vote_id_type, son_member_id_type> result;
    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_id>();
    for(auto acc: accs) {
        auto son_id = create_son_member( acc, db );
        auto iter = idx.find( son_id );
        BOOST_REQUIRE( iter != idx.end() );
        result.insert( std::make_pair( iter->vote_id, son_id ) );
    }
    return result;
}

inline void set_maintenance_params( database& db, uint64_t acc_amount, uint64_t asset_amount, uint64_t new_active_sons_count, std::map<vote_id_type, son_member_id_type> sons ){
    db._son_count_histogram_buffer.resize( 501 );
    db._son_count_histogram_buffer[0] = max_asset - acc_amount * asset_amount;
    db._son_count_histogram_buffer[new_active_sons_count >> 1] = acc_amount * asset_amount;
    db._total_voting_stake = max_asset;
    db._vote_tally_buffer.resize( 20 + acc_amount );
    for( auto iter: sons ) {
        db._vote_tally_buffer[iter.first] = acc_amount * asset_amount;
    }
}

/// Simple son creating
BOOST_AUTO_TEST_CASE( create_son_test ){
    test_create_son_member_evaluator test_eval( db );
    son_member_create_operation op;
    op.owner_account = account_id_type(1);
    op.url = test_url;

    BOOST_REQUIRE_NO_THROW( test_eval.do_evaluate( op ) );
    auto id = test_eval.do_apply( op );
    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_account>();

    BOOST_REQUIRE( idx.size() == 1 );

    auto obj = idx.find( op.owner_account );
    BOOST_REQUIRE( obj != idx.end() );
    BOOST_CHECK( obj->url == test_url );
    BOOST_CHECK( id == obj->id );
}

/// Simple son deleting
BOOST_AUTO_TEST_CASE( delete_son_test ){
    INVOKE(create_son_test);
    test_delete_son_member_evaluator test_eval( db );

    son_member_delete_operation delete_op;
    delete_op.owner_account = account_id_type(1);
    BOOST_REQUIRE_NO_THROW( test_eval.do_evaluate( delete_op ) );
    test_eval.do_apply( delete_op );

    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_account>();
    BOOST_CHECK( idx.empty() );
}

/// Create active sons with new accounts
BOOST_AUTO_TEST_CASE( update_active_sons_with_new_account_test ) {
    const uint64_t acc_amount = 11;
    const uint64_t asset_amount = 30000000;
    auto accs = create_accounts( *this, acc_amount, asset_amount );
    auto sons = create_son_members( db, accs );

    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_account>();
    BOOST_REQUIRE( idx.size() == acc_amount );
    db._son_count_histogram_buffer.resize( 501 );
    db._son_count_histogram_buffer[0] = max_asset;
    db._total_voting_stake = max_asset;
    db._vote_tally_buffer.resize( 20 + acc_amount );
    for( auto iter: sons ) {
        db._vote_tally_buffer[iter.first] = acc_amount * asset_amount;
    }

    BOOST_REQUIRE( db.get_global_properties().active_son_members.empty() );
    db.update_active_son_members();
    auto active = db.get_global_properties().active_son_members;
    BOOST_REQUIRE( active.size() == sons.size() );
    for( auto iter: sons){
        BOOST_CHECK( active.find(iter.second) != active.end() );
    }
}

/// Create active sons with new accounts and number of active less than number of all
BOOST_AUTO_TEST_CASE( change_active_sons_count_less_test ) {
    const uint64_t acc_amount = 21;
    const uint64_t asset_amount = 30000000;
    const uint64_t new_active_sons_count = 11;
    auto accs = create_accounts( *this, acc_amount, asset_amount );
    auto sons = create_son_members( db, accs );

    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_account>();
    BOOST_REQUIRE( idx.size() == acc_amount );

    set_maintenance_params( db, acc_amount, asset_amount, new_active_sons_count, sons );

    BOOST_REQUIRE( db.get_global_properties().active_son_members.empty() );
    db.update_active_son_members();
    auto active = db.get_global_properties().active_son_members;
    BOOST_REQUIRE( active.size() == new_active_sons_count );
    std::vector<vote_id_type> sorted_sons;
    for( auto iter: sons ){
        sorted_sons.push_back( iter.first );
    }
    std::sort(sorted_sons.begin(), sorted_sons.end());
    for( uint32_t i = 0; i < new_active_sons_count; ++i ){
        BOOST_CHECK( active.find( sons[sorted_sons[i]] ) != active.end() );
    }
}

/// Create active sons with new accounts and number of active less than number of all
/// After that will change active
BOOST_AUTO_TEST_CASE( change_active_sons_less_test ) {
    const uint8_t acc_amount = 15;
    const uint32_t asset_amount = 30000000;
    const uint64_t new_active_sons_count = 11;
    auto accs = create_accounts( *this, acc_amount, asset_amount );
    auto sons = create_son_members( db, accs );
    
    set_maintenance_params( db, acc_amount, asset_amount, new_active_sons_count, sons );

    BOOST_REQUIRE( db.get_global_properties().active_son_members.empty() );

    db.update_active_son_members();

    auto iter = sons.begin();
    auto looper = acc_amount - new_active_sons_count;
    while( looper-- > 0 ){
        db._vote_tally_buffer[iter->first] = 0;
        ++iter;
        ++iter;
    }

    db.update_active_son_members();

    auto active = db.get_global_properties().active_son_members;
    BOOST_REQUIRE( active.size() == new_active_sons_count );

    for( auto iter: sons ) {
        BOOST_CHECK( active.find( iter.second ) != active.end() || db._vote_tally_buffer[iter.first] == 0 );
    }
}

/// Create active sons with new accounts and number of active greater than number of all
BOOST_AUTO_TEST_CASE( change_active_sons_count_greater_test ) {
    const uint8_t acc_amount = 11;
    const uint32_t asset_amount = 30000000;
    const uint64_t new_active_sons_count = 21;
    auto accs = create_accounts( *this, acc_amount, asset_amount );
    auto sons = create_son_members( db, accs );

    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_account>();
    BOOST_REQUIRE( idx.size() == acc_amount );
    
    set_maintenance_params( db, acc_amount, asset_amount, new_active_sons_count, sons );

    BOOST_REQUIRE( db.get_global_properties().active_son_members.empty() );
    db.update_active_son_members();
    auto active = db.get_global_properties().active_son_members;
    BOOST_REQUIRE( active.size() == sons.size() );
    for( auto iter: sons){
        BOOST_CHECK( active.find(iter.second) != active.end() );
    }
}

/// Create active sons (like integration test)
BOOST_AUTO_TEST_CASE( add_active_sons_test ){
    create_active_sons( *this );
}

/// Change one son (like integration test)
BOOST_AUTO_TEST_CASE( change_active_sons ){
    INVOKE( add_active_sons_test );
    const uint8_t sons_amount = 3;

    test::set_expiration( db, database_fixture::trx );
    const auto account = create_account( "sonmem" );
    upgrade_to_lifetime_member( account );

    signed_transaction trx1;
    son_member_create_operation op;
    op.url = test_url;
    op.owner_account = account.get_id();
    trx1.operations.push_back( op );
    trx1.validate();
    test::set_expiration( db, trx1 );
    sign( trx1, private_key );
    PUSH_TX(db, trx1, skip_flags);

    generate_block( skip_flags );

    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_account>();
    BOOST_REQUIRE( idx.size() == sons_amount + 1 );
    auto account_iter = idx.find(account.get_id());
    BOOST_REQUIRE( account_iter != idx.end() );

    flat_set<vote_id_type> votes;
    auto iter = idx.begin();
    for( ++iter; iter != idx.end(); ++iter ){
        votes.insert( iter->vote_id );
    }

    signed_transaction trx2;
    transfer( account_id_type(), account.get_id(), asset( 10000000, asset_id_type() ) );
    trx2.operations.push_back( get_acc_update_op( account.get_id(), account_id_type(1)(db).options, votes, sons_amount ) );
    
    for( uint8_t i = 1; i <= sons_amount; ++i ){
        auto account_id = account_id_type(i);
        transfer( account_id_type(), account_id, asset( 10000000, asset_id_type() ) );
        trx2.operations.push_back( get_acc_update_op( account_id, account_id(db).options, votes, sons_amount ) );
    }
    
    trx2.validate();
    test::set_expiration( db, trx2 );
    sign( trx2, private_key );
    PUSH_TX( db, trx2, skip_flags );

    generate_blocks(db.get_dynamic_global_properties().next_maintenance_time, true, skip_flags);
    BOOST_REQUIRE( db.get_global_properties().active_son_members.find(account_iter->get_id()) != db.get_global_properties().active_son_members.end() );
}

BOOST_AUTO_TEST_SUITE_END()