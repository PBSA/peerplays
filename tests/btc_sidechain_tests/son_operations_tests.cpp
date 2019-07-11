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
const uint64_t asset_amount = 30000000;

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
        df.transfer( account_id_type(), account_id_type(i), asset( asset_amount, asset_id_type() ) );
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

inline vote_id_type get_vote_id( database& db ){
    vote_id_type vote_id;
    db.modify(db.get_global_properties(), [&vote_id](global_property_object& p) {
        vote_id = get_next_vote_id(p, vote_id_type::son);
    });
    return vote_id;
}

inline void create_sons_without_op( database_fixture& df, std::vector<account_id_type>& accs, uint64_t start = 0, uint64_t end = 0, uint64_t days_salt = 0){
    for( uint64_t i = start; i < end; ++i ){
        vote_id_type vote_id = get_vote_id( df.db );
        df.db.create<son_member_object>( [&]( son_member_object& obj ){
            df.db.adjust_balance( accs[i], -asset( df.db.get_global_properties().parameters.son_deposit_amount, asset_id_type() ) );
            obj.son_member_account = accs[i];
            obj.vote_id            = vote_id;
            obj.url                = test_url;
            obj.time_of_deleting   = df.db.head_block_time() + fc::days( days_salt == 0 ? i : days_salt );
        });
    }
}

/// Simple son creating
BOOST_AUTO_TEST_CASE( create_son_test ){
    transfer( account_id_type(), account_id_type(1), asset( asset_amount, asset_id_type() ) );
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

    BOOST_REQUIRE( get_balance( op.owner_account, asset_id_type() ) == ( asset_amount - db.get_global_properties().parameters.son_deposit_amount ) );
}

/// All `time_of_deleting` are different. `by_deleted` must sort them in ascending order by timestamp. 
BOOST_AUTO_TEST_CASE( son_idx_comp_diff_values_test ){
    const uint64_t son_amount = 11;
    auto accs = create_accounts( *this, son_amount, asset_amount );
    create_sons_without_op( *this, accs, 0, son_amount );

    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_deleted>();

    auto first_iter = idx.begin();
    auto second_iter = ++idx.begin();
    while( second_iter != idx.end() ){
        BOOST_CHECK( *first_iter->time_of_deleting < *second_iter->time_of_deleting );
        ++first_iter;
        ++second_iter;
    }
}

/// Every second object have not valid `time_of_deleting`
BOOST_AUTO_TEST_CASE( son_idx_comp_not_valid_values_test ){
    const uint64_t son_amount = 10;
    auto accs = create_accounts( *this, son_amount, asset_amount );
    for( uint64_t i = 0; i < son_amount; ++i ){
        vote_id_type vote_id = get_vote_id( db );
        db.create<son_member_object>( [&]( son_member_object& obj ){
            db.adjust_balance( accs[i], -asset( db.get_global_properties().parameters.son_deposit_amount, asset_id_type() ) );
            obj.son_member_account = accs[i];
            obj.vote_id            = vote_id;
            obj.url                = test_url;
            if( i % 2 == 0)
                obj.time_of_deleting = db.head_block_time() + fc::days( i );
        });
    }

    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_deleted>();

    uint64_t counter = 0;
    for( auto iter: idx ){
        if( iter.time_of_deleting )
            ++counter;
        else {
            BOOST_REQUIRE( counter == son_amount / 2 );
            break;
        }
    }
}

/// Some objects have valid `time_of_deleting` (2 different values) and some not valid
BOOST_AUTO_TEST_CASE( son_idx_comp_not_valid_diff_values_test ){
    const uint64_t son_amount = 11;
    const uint64_t first_part = 4;
    const uint64_t second_part = 4;
    auto accs = create_accounts( *this, son_amount, asset_amount );

    create_sons_without_op( *this, accs, 0, first_part, first_part );
    create_sons_without_op( *this, accs, first_part, first_part + second_part, first_part + second_part );

    for( uint64_t i = first_part + second_part; i < son_amount; ++i ){
        vote_id_type vote_id = get_vote_id( db );
        db.create<son_member_object>( [&]( son_member_object& obj ){
            db.adjust_balance( accs[i], -asset( db.get_global_properties().parameters.son_deposit_amount, asset_id_type() ) );
            obj.son_member_account = accs[i];
            obj.vote_id            = vote_id;
            obj.url                = test_url;
        });
    }

    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_deleted>();
    uint64_t counter_one = 0;
    uint64_t counter_two = 0;
    for( auto iter: idx ){
        if( iter.time_of_deleting )
            if( iter.time_of_deleting == db.head_block_time() + fc::days( first_part ) )
                ++counter_one;
            else if( iter.time_of_deleting == db.head_block_time() + fc::days( first_part + second_part ) ) {
                BOOST_REQUIRE( counter_one == first_part );
                ++counter_two;
            }
        else {
            break;
        }
    }
    BOOST_CHECK( counter_one == first_part );
    BOOST_REQUIRE( counter_two == second_part );
}

/// Simple son deleting
BOOST_AUTO_TEST_CASE( delete_son_test ){
    INVOKE(create_son_test);
    test_delete_son_member_evaluator test_eval( db );

    son_member_delete_operation delete_op;
    delete_op.owner_account = account_id_type(1);
    BOOST_REQUIRE_NO_THROW( test_eval.do_evaluate( delete_op ) );
    test_eval.do_apply( delete_op );

    db.modify<dynamic_global_property_object> (db.get_dynamic_global_properties(), [this](dynamic_global_property_object& obj){
        obj.time += fc::days(2);
    });

    db.delete_sons();

    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_account>();
    BOOST_REQUIRE( idx.empty() );

    BOOST_REQUIRE( get_balance( delete_op.owner_account, asset_id_type() ) == asset_amount );
}

/// Delete not all SON members, some have not valid `time_of_deleting`(don't send son_member_delete_operation yet)
BOOST_AUTO_TEST_CASE( delete_some_sons_test ){
    const uint64_t acc_amount = 11;
    const uint64_t first_delete = 5;
    test_delete_son_member_evaluator test_eval( db );
    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_account>();
    auto accs = create_accounts( *this, acc_amount, asset_amount );
    auto sons = create_son_members( db, accs );

    for( auto iter: std::vector<account_id_type>(accs.begin(), accs.begin() + first_delete) ){
        son_member_delete_operation op;
        op.owner_account = iter;
        BOOST_REQUIRE_NO_THROW( test_eval.do_evaluate( op ) );
        test_eval.do_apply( op );
    }

    BOOST_REQUIRE( idx.size() == acc_amount );

    db.modify<dynamic_global_property_object> (db.get_dynamic_global_properties(), [this](dynamic_global_property_object& obj){
        obj.time += fc::days(2);
    });

    db.delete_sons();

    BOOST_REQUIRE( idx.size() == acc_amount - first_delete );
    for( auto iter: std::vector<account_id_type>(accs.begin(), accs.begin() + first_delete) ){
        BOOST_CHECK( idx.find(iter) == idx.end() );
        BOOST_CHECK( get_balance( iter, asset_id_type() ) == asset_amount );
    }
    for( auto iter: std::vector<account_id_type>(accs.begin() + first_delete + 1, accs.end() ) ){
        BOOST_CHECK( idx.find(iter) != idx.end() );
    }
}

/// Delete not all SON members, for some, the deposit period has not passed
BOOST_AUTO_TEST_CASE( all_send_delete_op_test ){
    const uint64_t acc_amount = 11;
    const uint64_t first_delete = 5;
    test_delete_son_member_evaluator test_eval( db );
    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_account>();
    auto accs = create_accounts( *this, acc_amount, asset_amount );
    auto sons = create_son_members( db, accs );

    for( auto iter: std::vector<account_id_type>(accs.begin(), accs.begin() + first_delete) ){
        son_member_delete_operation op;
        op.owner_account = iter;
        BOOST_REQUIRE_NO_THROW( test_eval.do_evaluate( op ) );
        test_eval.do_apply( op );
    }

    BOOST_REQUIRE( idx.size() == acc_amount );

    db.modify<dynamic_global_property_object> (db.get_dynamic_global_properties(), [this](dynamic_global_property_object& obj){
        obj.time += fc::days(2);
    });

    for( auto iter: std::vector<account_id_type>(accs.begin() + first_delete, accs.end() )){
        son_member_delete_operation op;
        op.owner_account = iter;
        BOOST_REQUIRE_NO_THROW( test_eval.do_evaluate( op ) );
        test_eval.do_apply( op );
    }

    for( auto iter: idx ){
        idump(( iter.time_of_deleting ));
    }

    db.delete_sons();    

    BOOST_REQUIRE( idx.size() == acc_amount - first_delete );
    for( auto iter: std::vector<account_id_type>(accs.begin(), accs.begin() + first_delete) ){
        BOOST_CHECK( idx.find(iter) == idx.end() );
    }
    for( auto iter: std::vector<account_id_type>(accs.begin() + first_delete + 1, accs.end() ) ){
        BOOST_CHECK( idx.find(iter) != idx.end() );
    }
}

/// Create active sons with new accounts
BOOST_AUTO_TEST_CASE( update_active_sons_with_new_account_test ) {
    const uint64_t acc_amount = 11;
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
    transfer( account_id_type(), account.get_id(), asset( asset_amount, asset_id_type() ) );

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
    trx2.operations.push_back( get_acc_update_op( account.get_id(), account_id_type(1)(db).options, votes, sons_amount ) );
    
    for( uint8_t i = 1; i <= sons_amount; ++i ){
        auto account_id = account_id_type(i);
        // transfer( account_id_type(), account_id, asset( 10000000, asset_id_type() ) );
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