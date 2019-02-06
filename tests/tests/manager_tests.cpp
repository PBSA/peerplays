#include <algorithm>

#include <boost/test/unit_test.hpp>
#include <fc/optional.hpp>

#include <graphene/chain/protocol/sport.hpp>
#include <graphene/chain/protocol/event_group.hpp>
#include <graphene/chain/protocol/event.hpp>
#include <graphene/chain/protocol/betting_market.hpp>

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/sport_object.hpp>
#include <graphene/chain/event_group_object.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/betting_market_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/witness_object.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

struct manager_tests_fixture : database_fixture
{
    internationalized_string_type default_sport_name = {{"TS", "TEST_SPORT"}};
    internationalized_string_type default_event_group_name = {{"EG", "TEST_GROUP"}};
    internationalized_string_type default_event_name = {{"EN", "EVENT_NAME"}};
    event_status default_event_status = event_status::upcoming;
    internationalized_string_type default_betting_market_group_description = {{"BMG", "BET_GROUP"}};
    internationalized_string_type default_betting_market_description = {{"BM", "BET_MARKET"}};


    internationalized_string_type new_sport_name = {{"NS", "NEW_SPORT"}};
    internationalized_string_type new_event_group_name = {{"NG", "NEW_GROUP"}};
    internationalized_string_type new_event_name = {{"NE", "NEW_EVENT"}};
    event_status new_event_status = event_status::frozen;
    internationalized_string_type new_betting_market_group_description = {{"ND", "NEW_DESC"}};
    internationalized_string_type new_betting_market_description = {{"NM", "NEW_MARKET"}};


    processed_transaction process_operation_by_fee_payer( operation op )
    {
        signed_transaction trx;
        set_expiration( db, trx );
        trx.operations.push_back( op );
        return PUSH_TX( db, trx, ~0 );
    }

    sport_id_type create_sport_by_witness( optional<account_id_type> new_manager = optional<account_id_type>() )
    {
        auto scop = create_sport_create_op( new_manager );
        process_operation_by_witnesses( scop );

        const sport_object& sport_obj = *db.get_index_type<sport_object_index>().indices().get<by_id>().begin();
        return sport_obj.id;
    }

    void update_sport_by_witness( sport_id_type sport_id,
        optional<account_id_type> new_manager = optional<account_id_type>() )
    {
        auto suop = create_sport_update_op( sport_id, new_manager );
        process_operation_by_witnesses( suop );
    }

    void update_sport_by_fee_payer( sport_id_type sport_id,
        optional<account_id_type> new_manager,
        optional<account_id_type> fee_paying_account )
    {
        auto suop = create_sport_update_op( sport_id, new_manager, fee_paying_account );
        process_operation_by_fee_payer( suop );
    }

    void delete_sport_by_witness( sport_id_type sport_id )
    {
        auto sdop = create_sport_delete_op( sport_id );
        process_operation_by_witnesses( sdop );
    }

    void delete_sport_by_fee_payer( sport_id_type sport_id,
        optional<account_id_type> fee_paying_account )
    {
        auto sdop = create_sport_delete_op( sport_id, fee_paying_account );
        process_operation_by_fee_payer( sdop );
    }

    event_group_id_type create_event_group_by_witness( sport_id_type sport_id,
        optional<account_id_type> new_manager = optional<account_id_type>() )
    {
        auto egcop = create_event_group_create_op( sport_id, new_manager );
        process_operation_by_witnesses( egcop );

        const auto& eg_obj = *db.get_index_type<event_group_object_index>().indices().get<by_id>().begin();
        return eg_obj.id;
    }

    event_group_id_type create_event_group_by_fee_payer( sport_id_type sport_id, 
        optional<account_id_type> new_manager,
        optional<account_id_type> fee_paying_account )
    {
        auto egcop = create_event_group_create_op( sport_id, new_manager, fee_paying_account );
        process_operation_by_fee_payer( egcop );

        const auto& eg_obj = *db.get_index_type<event_group_object_index>().indices().get<by_id>().begin();
        return eg_obj.id;
    }

    void update_event_group_by_witness( event_group_id_type event_group_id,
        optional<account_id_type> new_manager = optional<account_id_type>() )
    {
        auto eguop = create_event_group_update_op( event_group_id, new_manager );
        process_operation_by_witnesses( eguop );
    }

    void update_event_group_by_fee_payer( event_group_id_type event_group_id,
        optional<account_id_type> new_manager,
        optional<account_id_type> fee_paying_account )
    {
        auto eguop = create_event_group_update_op( event_group_id, new_manager, fee_paying_account );
        process_operation_by_fee_payer( eguop );        
    }

    void delete_event_group_by_witness( event_group_id_type event_group_id )
    {
        auto egdop = create_event_group_delete_op( event_group_id );
        process_operation_by_witnesses( egdop );
    }

    void delete_event_group_by_fee_payer( event_group_id_type event_group_id,
        optional<account_id_type> fee_paying_account )
    {
        auto egdop = create_event_group_delete_op( event_group_id, fee_paying_account );
        process_operation_by_fee_payer( egdop );
    }

    event_id_type create_event_by_witness( event_group_id_type event_group_id,
        optional<account_id_type> new_manager = optional<account_id_type>() )
    {
        auto ecop = create_event_create_op( event_group_id, new_manager );
        process_operation_by_witnesses( ecop );

        const auto& e_obj = *db.get_index_type<event_object_index>().indices().get<by_id>().begin();
        return e_obj.id;
    }

    event_id_type create_event_by_fee_payer( event_group_id_type event_group_id,
        optional<account_id_type> new_manager,
        optional<account_id_type> fee_paying_account )
    {
        auto ecop = create_event_create_op( event_group_id, new_manager, fee_paying_account );
        process_operation_by_fee_payer( ecop );
        
        const auto& e_obj = *db.get_index_type<event_object_index>().indices().get<by_id>().begin();
        return e_obj.id;
    }

    void update_event_by_witness( event_id_type event_id,
        optional<account_id_type> new_manager = optional<account_id_type>() )
    {
        auto euop = create_event_update_op( event_id, new_manager );
        process_operation_by_witnesses( euop );
    }

    void update_event_by_fee_payer( event_id_type event_id,
        optional<account_id_type> new_manager,
        optional<account_id_type> fee_paying_account )
    {
        auto euop = create_event_update_op( event_id, new_manager, fee_paying_account );
        process_operation_by_fee_payer( euop );
    }    

    void update_event_status_by_witness( event_id_type event_id )
    {
        auto eusop = create_event_update_status_op( event_id );
        process_operation_by_witnesses( eusop );
    }

    void update_event_status_by_fee_payer( event_id_type event_id,
        optional<account_id_type> fee_paying_account )
    {
        auto eusop = create_event_update_status_op( event_id, fee_paying_account );
        process_operation_by_fee_payer( eusop );
    }

    betting_market_rules_id_type create_betting_market_rules_by_witness()
    {
        auto bmrcop = create_betting_market_rules_create_op();
        process_operation_by_witnesses( bmrcop );

        auto& betting_market_rules_obj = *db.get_index_type<betting_market_rules_object_index>().indices().get<by_id>().begin();
        return betting_market_rules_obj.id;
    }

    betting_market_group_id_type create_betting_market_group_by_witness( event_id_type event_id,
        betting_market_rules_id_type rules_id )
    {
        auto bmgcop = create_betting_market_group_create_op( event_id, rules_id );
        process_operation_by_witnesses( bmgcop );

        auto& betting_market_group_obj = *db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().begin();
        return betting_market_group_obj.id;
    }

    betting_market_group_id_type create_betting_market_group_by_fee_payer( event_id_type event_id,
        betting_market_rules_id_type rules_id,
        optional<account_id_type> fee_paying_account )
    {
        auto bmgcop = create_betting_market_group_create_op( event_id, rules_id, fee_paying_account );
        process_operation_by_fee_payer( bmgcop );

        auto& betting_market_group_obj = *db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().begin();
        return betting_market_group_obj.id;
    }

    void update_betting_market_group_by_witness( betting_market_group_id_type betting_market_group_id )
    {
        auto bmguop = create_betting_market_group_update_op( betting_market_group_id );
        process_operation_by_witnesses( bmguop );
    }

    void update_betting_market_group_by_fee_payer( betting_market_group_id_type betting_market_id,
        optional<account_id_type> fee_paying_account )
    {   
        auto bmguop = create_betting_market_group_update_op( betting_market_id, fee_paying_account );
        process_operation_by_fee_payer( bmguop );
    }

    void resolve_betting_market_group_by_witness( betting_market_group_id_type betting_market_group_id,
        betting_market_id_type betting_market_id )
    {
        auto bmgrop = create_betting_market_group_resolve_op( betting_market_group_id, betting_market_id );
        process_operation_by_witnesses( bmgrop );
    }

    void resolve_betting_market_group_by_fee_payer( betting_market_group_id_type betting_market_group_id,
        betting_market_id_type betting_market_id,
        optional<account_id_type> fee_paying_account )
    {
        auto bmgrop = create_betting_market_group_resolve_op( betting_market_group_id, betting_market_id, fee_paying_account );
        process_operation_by_fee_payer( bmgrop );
    }

    void cancel_betting_market_group_unmatched_bets_by_witness( betting_market_group_id_type betting_market_group_id )
    {
        auto bmgcubop = create_bettting_market_group_cancel_unmatched_bets_op( betting_market_group_id );
        process_operation_by_witnesses( bmgcubop );
    }

    void cancel_betting_market_group_unmatched_bets_by_fee_payer( betting_market_group_id_type betting_market_group_id,
        optional<account_id_type> fee_paying_account)
    {
        auto bmgcubop = create_bettting_market_group_cancel_unmatched_bets_op( betting_market_group_id, fee_paying_account );
        process_operation_by_fee_payer( bmgcubop );
    }

    betting_market_id_type create_betting_market_by_witness( betting_market_group_id_type betting_market_group_id )
    {
        auto bmcop = create_betting_market_create_op( betting_market_group_id );
        process_operation_by_witnesses( bmcop );

        auto& betting_market_obj = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().begin();
        return betting_market_obj.id;
    }

    betting_market_id_type create_betting_market_by_fee_payer( betting_market_group_id_type betting_market_group_id,
        optional<account_id_type> fee_paying_account )
    {
        auto bmcop = create_betting_market_create_op( betting_market_group_id, fee_paying_account );
        process_operation_by_fee_payer( bmcop );

        auto& betting_market_obj = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().begin();
        return betting_market_obj.id;
    }

    void update_betting_market_by_witness( betting_market_id_type betting_market_id )
    {
        auto bmuop = create_betting_market_update_op( betting_market_id );
        process_operation_by_witnesses( bmuop );
    }

    void update_betting_market_by_fee_payer( betting_market_id_type betting_market_id,
        optional<account_id_type> fee_paying_account )
    {
        auto bmuop = create_betting_market_update_op( betting_market_id, fee_paying_account );
        process_operation_by_fee_payer( bmuop );
    }

    sport_create_operation create_sport_create_op( optional<account_id_type> new_manager )
    {
        sport_create_operation scop;
        scop.name = default_sport_name;
        if( new_manager )
            scop.extensions.value.manager = new_manager;
        
        return scop;
    }

    sport_update_operation create_sport_update_op( sport_id_type sport_id, 
        optional<account_id_type> new_manager = optional<account_id_type>(),
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {
        sport_update_operation suop;
        suop.sport_id = sport_id;
        suop.new_name = new_sport_name;
        if( new_manager )
            suop.extensions.value.new_manager = new_manager;
        if( fee_paying_account )
            suop.extensions.value.fee_paying_account = fee_paying_account;

        return suop;
    }

    sport_delete_operation create_sport_delete_op( sport_id_type sport_id,
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {
        sport_delete_operation sdop;
        sdop.sport_id = sport_id;
        if( fee_paying_account )
            sdop.extensions.value.fee_paying_account = fee_paying_account;
    
        return sdop;
    }

    event_group_create_operation create_event_group_create_op( sport_id_type sport_id,
        optional<account_id_type> new_manager,
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {
        event_group_create_operation egcop;
        egcop.sport_id = sport_id;
        egcop.name = default_event_group_name;
        if( new_manager )
            egcop.extensions.value.new_manager = new_manager;
        if( fee_paying_account )
            egcop.extensions.value.fee_paying_account = fee_paying_account;

        return egcop;
    }

    event_group_update_operation create_event_group_update_op( event_group_id_type event_group_id,
        optional<account_id_type> new_manager,
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {
        event_group_update_operation eguop;
        eguop.event_group_id = event_group_id;
        eguop.new_name       = new_event_group_name;
        if( new_manager  )
            eguop.extensions.value.new_manager = new_manager;
        if( fee_paying_account )
            eguop.extensions.value.fee_paying_account = fee_paying_account;

        return eguop;
    }

    event_group_delete_operation create_event_group_delete_op( event_group_id_type event_group_id, 
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {
        event_group_delete_operation egdop;
        egdop.event_group_id = event_group_id;
        if( fee_paying_account )
            egdop.extensions.value.fee_paying_account = fee_paying_account;

        return egdop;
    }

    event_create_operation create_event_create_op( event_group_id_type event_group_id,
        optional<account_id_type> new_manager,
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {
        event_create_operation ecop;
        ecop.event_group_id  = event_group_id;
        ecop.name            = default_event_name;
        ecop.season          = { { "SN", "SEASON_NAME" } };
        if( new_manager )
            ecop.extensions.value.new_manager        = new_manager;
        if( fee_paying_account )
            ecop.extensions.value.fee_paying_account = fee_paying_account;

        return ecop;
    }

    event_update_operation create_event_update_op( event_id_type event_id,
        optional<account_id_type> new_manager,
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {
        event_update_operation euop;
        euop.event_id = event_id;
        euop.new_name = new_event_name;
        if( new_manager )
            euop.extensions.value.new_manager = new_manager;
        if( fee_paying_account )
            euop.extensions.value.fee_paying_account = fee_paying_account;

        return euop;
    }

    event_update_status_operation create_event_update_status_op( event_id_type event_id,
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {
        event_update_status_operation eusop;
        eusop.event_id = event_id;
        eusop.status = new_event_status;
        if( fee_paying_account )
            eusop.extensions.value.fee_paying_account = fee_paying_account;

        return eusop;
    }

    betting_market_rules_create_operation create_betting_market_rules_create_op()
    {
        internationalized_string_type default_betting_market_rules_name = {{"MR", "MARKET_RULES"}};
        internationalized_string_type default_betting_market_rules_description = {{"DC", "DISCRIPTION"}};

        betting_market_rules_create_operation bmrcop;
        bmrcop.name = default_betting_market_rules_name; 
        bmrcop.description = default_betting_market_rules_description;

        return bmrcop;
    }

    betting_market_group_create_operation create_betting_market_group_create_op(
        event_id_type event_id,
        betting_market_rules_id_type betting_market_rules_id,
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {
        betting_market_group_create_operation bmgcop;
        bmgcop.event_id = event_id;
        bmgcop.rules_id = betting_market_rules_id;
        bmgcop.description = default_betting_market_group_description;
        if( fee_paying_account != account_id_type() )
            bmgcop.extensions.value.fee_paying_account = fee_paying_account;

        return bmgcop;
    }

    betting_market_group_update_operation create_betting_market_group_update_op( 
        betting_market_group_id_type betting_market_group_id,
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {
        betting_market_group_update_operation bmguop;
        bmguop.betting_market_group_id = betting_market_group_id;
        bmguop.new_description = new_betting_market_group_description;
        if( fee_paying_account )
            bmguop.extensions.value.fee_paying_account = fee_paying_account;

        return bmguop;
    }

    betting_market_group_resolve_operation create_betting_market_group_resolve_op(
        betting_market_group_id_type betting_market_group_id,
        betting_market_id_type betting_market_id,
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {
        betting_market_group_resolve_operation bmgrop;
        bmgrop.betting_market_group_id = betting_market_group_id;
        bmgrop.resolutions = {{betting_market_id, betting_market_resolution_type::cancel}};
        if( fee_paying_account )
            bmgrop.extensions.value.fee_paying_account = fee_paying_account;

        return bmgrop;
    }

    betting_market_group_cancel_unmatched_bets_operation create_bettting_market_group_cancel_unmatched_bets_op(
        betting_market_group_id_type betting_market_group_id,
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {
        betting_market_group_cancel_unmatched_bets_operation bmgcubop;
        bmgcubop.betting_market_group_id = betting_market_group_id;
        if( fee_paying_account )
            bmgcubop.extensions.value.fee_paying_account = fee_paying_account;

        return bmgcubop;
    }

    betting_market_create_operation create_betting_market_create_op( betting_market_group_id_type betting_market_group_id,
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {
        betting_market_create_operation bmcop;
        bmcop.group_id = betting_market_group_id;
        bmcop.description = default_betting_market_description;
        if( fee_paying_account )
            bmcop.extensions.value.fee_paying_account = fee_paying_account;

        return bmcop;
    }

    betting_market_update_operation create_betting_market_update_op( betting_market_id_type betting_market_id,
        optional<account_id_type> fee_paying_account = optional<account_id_type>() )
    {   
        betting_market_update_operation bmuop;
        bmuop.betting_market_id = betting_market_id;
        bmuop.new_description = new_betting_market_description;
        if( fee_paying_account )
            bmuop.extensions.value.fee_paying_account = fee_paying_account;

        return bmuop;
    }
};

BOOST_FIXTURE_TEST_SUITE( manager_tests, manager_tests_fixture )

#define GRAPHENE_CHECK_NO_THROW( op, bool_description ) \
    {                                                   \
        bool bool_description = true;                   \
        try {                                           \
            op;                                         \
        }                                               \
        catch( fc::assert_exception &e ) {              \
            bool_description = false;                   \
        }                                               \
        BOOST_CHECK( bool_description );                \
    }


BOOST_AUTO_TEST_CASE( before_hf_create_modify_and_delete_all_objects_and_check_default_manager_with_witness )
{ try {
    generate_blocks( HARDFORK_1000_TIME );
    generate_block();

    auto witness_id = GRAPHENE_WITNESS_ACCOUNT;
    

    auto sport_id = create_sport_by_witness();
    BOOST_CHECK( sport_id(db).name == default_sport_name 
        && sport_id(db).manager == witness_id );

    auto event_group_id = create_event_group_by_witness( sport_id );
    BOOST_CHECK( event_group_id(db).name == default_event_group_name 
        && event_group_id(db).manager == witness_id );

    auto event_id = create_event_by_witness( event_group_id );
    BOOST_CHECK( event_id(db).name == default_event_name 
        && event_id(db).manager == witness_id );

    auto rules_id = create_betting_market_rules_by_witness();

    auto betting_market_group_id = create_betting_market_group_by_witness( event_id, rules_id );
    BOOST_CHECK( betting_market_group_id(db).description == default_betting_market_group_description );

    auto betting_market_id = create_betting_market_by_witness( betting_market_group_id );
    BOOST_CHECK( betting_market_id(db).description == default_betting_market_description );



    update_sport_by_witness( sport_id );
    BOOST_CHECK( sport_id(db).name == new_sport_name );

    update_event_group_by_witness( event_group_id );
    BOOST_CHECK( event_group_id(db).name == new_event_group_name );

    update_event_by_witness( event_id );
    BOOST_CHECK( event_id(db).name == new_event_name );

    update_event_status_by_witness( event_id );
    BOOST_CHECK( event_id(db).get_status() == new_event_status );

    update_betting_market_group_by_witness( betting_market_group_id );
    BOOST_CHECK( betting_market_group_id(db).description == new_betting_market_group_description );

    GRAPHENE_CHECK_NO_THROW( resolve_betting_market_group_by_witness( betting_market_group_id, betting_market_id ),
        resolve_betting_market_group );

    GRAPHENE_CHECK_NO_THROW( cancel_betting_market_group_unmatched_bets_by_witness( betting_market_group_id ),
        cancel_betting_market_group_unmatched_bets );

    update_betting_market_by_witness( betting_market_id );
    BOOST_CHECK( betting_market_id(db).description == new_betting_market_description );


    
    GRAPHENE_CHECK_NO_THROW( delete_event_group_by_witness( event_group_id ),
        delete_event_group );

    GRAPHENE_CHECK_NO_THROW( delete_sport_by_witness( sport_id ),
        delete_sport );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( before_hf_check_manager_and_fee_paying_account_asserts )
{ try {
    generate_blocks( HARDFORK_1000_TIME );
    generate_block();


    auto default_id = optional<account_id_type>();
    auto witness_id = GRAPHENE_WITNESS_ACCOUNT;

    auto sport_id = create_sport_by_witness();
    auto event_group_id = create_event_group_by_witness( sport_id );
    auto event_id = create_event_by_witness( event_group_id );
    auto rules_id = create_betting_market_rules_by_witness();
    auto betting_market_group_id = create_betting_market_group_by_witness( event_id, rules_id );
    auto betting_market_id = create_betting_market_by_witness( betting_market_group_id );



    GRAPHENE_REQUIRE_THROW( update_sport_by_fee_payer( sport_id, witness_id, default_id ),
        fc::assert_exception );

    GRAPHENE_REQUIRE_THROW( update_sport_by_fee_payer( sport_id, default_id, witness_id ),
        fc::assert_exception );

    GRAPHENE_REQUIRE_THROW( delete_sport_by_fee_payer( sport_id, witness_id ),
        fc::assert_exception );



    GRAPHENE_REQUIRE_THROW( create_event_group_by_fee_payer( sport_id, witness_id, default_id ),
        fc::assert_exception );

    GRAPHENE_REQUIRE_THROW( create_event_group_by_fee_payer( sport_id, default_id, witness_id ),
        fc::assert_exception );

    GRAPHENE_REQUIRE_THROW( update_event_group_by_fee_payer( event_group_id, witness_id, default_id ),
        fc::assert_exception );

    GRAPHENE_REQUIRE_THROW( update_event_group_by_fee_payer( event_group_id, default_id, witness_id ),
        fc::assert_exception );

    GRAPHENE_REQUIRE_THROW( delete_event_group_by_fee_payer( event_group_id, witness_id ),
        fc::assert_exception );

    
    
    GRAPHENE_REQUIRE_THROW( create_event_by_fee_payer( event_group_id, witness_id, default_id ),
        fc::assert_exception );

    GRAPHENE_REQUIRE_THROW( create_event_by_fee_payer( event_group_id, default_id, witness_id ),
        fc::assert_exception );

    GRAPHENE_REQUIRE_THROW( update_event_by_fee_payer( event_id, witness_id, default_id ),
        fc::assert_exception );

    GRAPHENE_REQUIRE_THROW( update_event_by_fee_payer( event_id, default_id, witness_id ),
        fc::assert_exception );

    GRAPHENE_REQUIRE_THROW( update_event_status_by_fee_payer( event_id, witness_id ),
        fc::assert_exception );

    
    
    GRAPHENE_REQUIRE_THROW( create_betting_market_group_by_fee_payer( event_id, rules_id, witness_id ),
        fc::assert_exception );

    GRAPHENE_REQUIRE_THROW( update_betting_market_group_by_fee_payer( betting_market_group_id, witness_id ),
        fc::assert_exception );    

    GRAPHENE_REQUIRE_THROW( cancel_betting_market_group_unmatched_bets_by_fee_payer( betting_market_group_id, witness_id ),
        fc::assert_exception );    

    GRAPHENE_REQUIRE_THROW( resolve_betting_market_group_by_fee_payer( betting_market_group_id, betting_market_id, witness_id ),
        fc::assert_exception ); 

    
    
    GRAPHENE_REQUIRE_THROW( create_betting_market_by_fee_payer( betting_market_group_id, witness_id ),
        fc::assert_exception );

    GRAPHENE_REQUIRE_THROW( update_betting_market_by_fee_payer( betting_market_id, witness_id ),
        fc::assert_exception );


} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( after_hf_check_all_is_not_manager_asserts )
{ try {
    generate_blocks( HARDFORK_1000_TIME );
    generate_block();


    ACTOR( alice );
    auto witness_id = GRAPHENE_WITNESS_ACCOUNT;

    
    auto sport_id = create_sport_by_witness();
    
    GRAPHENE_REQUIRE_THROW( create_event_group_by_fee_payer( sport_id, witness_id, alice_id),
        fc::assert_exception );

    auto event_group_id = create_event_group_by_witness( sport_id );
    GRAPHENE_REQUIRE_THROW( create_event_by_fee_payer( event_group_id, witness_id, alice_id),
        fc::assert_exception );

    auto event_id = create_event_by_witness( event_group_id );
    auto rules_id = create_betting_market_rules_by_witness();
    GRAPHENE_REQUIRE_THROW( create_betting_market_group_by_fee_payer( event_id, rules_id, alice_id ),
        fc::assert_exception );

    auto betting_market_group_id = create_betting_market_group_by_witness( event_id, rules_id );
    GRAPHENE_REQUIRE_THROW( create_betting_market_by_fee_payer( betting_market_group_id, alice_id ),
        fc::assert_exception );

    auto betting_market_id = create_betting_market_by_witness( betting_market_group_id );



    GRAPHENE_REQUIRE_THROW( update_sport_by_fee_payer( sport_id, witness_id, alice_id ),
        fc::assert_exception );    

    GRAPHENE_REQUIRE_THROW( update_event_group_by_fee_payer( event_group_id, witness_id, alice_id ),
        fc::assert_exception );    

    GRAPHENE_REQUIRE_THROW( update_event_by_fee_payer( event_id, witness_id, alice_id ),
        fc::assert_exception );    

    GRAPHENE_REQUIRE_THROW( update_event_status_by_fee_payer( event_id, alice_id ),
        fc::assert_exception );    

    GRAPHENE_REQUIRE_THROW( update_betting_market_group_by_fee_payer( betting_market_group_id, alice_id ),
        fc::assert_exception );    
    
    GRAPHENE_REQUIRE_THROW( resolve_betting_market_group_by_fee_payer( betting_market_group_id, betting_market_id, alice_id ),
        fc::assert_exception );    

    GRAPHENE_REQUIRE_THROW( cancel_betting_market_group_unmatched_bets_by_fee_payer( betting_market_group_id, alice_id ),
        fc::assert_exception );    

    GRAPHENE_REQUIRE_THROW( update_betting_market_by_fee_payer( betting_market_id, alice_id ),
        fc::assert_exception );    



    GRAPHENE_REQUIRE_THROW( delete_sport_by_fee_payer( sport_id, alice_id ),
        fc::assert_exception );    

    GRAPHENE_REQUIRE_THROW( delete_event_group_by_fee_payer( event_group_id, alice_id ),
        fc::assert_exception );    

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( after_hf_create_modify_and_delete_all_objects_and_check_default_manager_with_witness )
{
    generate_blocks( HARDFORK_MANAGER_TIME );
    generate_block();

    auto witness_id = GRAPHENE_WITNESS_ACCOUNT;
    

    auto sport_id = create_sport_by_witness();
    BOOST_CHECK( sport_id(db).name == default_sport_name 
        && sport_id(db).manager == witness_id );

    auto event_group_id = create_event_group_by_witness( sport_id );
    BOOST_CHECK( event_group_id(db).name == default_event_group_name 
        && event_group_id(db).manager == witness_id );

    auto event_id = create_event_by_witness( event_group_id );
    BOOST_CHECK( event_id(db).name == default_event_name 
        && event_id(db).manager == witness_id );

    auto rules_id = create_betting_market_rules_by_witness();

    auto betting_market_group_id = create_betting_market_group_by_witness( event_id, rules_id );
    BOOST_CHECK( betting_market_group_id(db).description == default_betting_market_group_description );

    auto betting_market_id = create_betting_market_by_witness( betting_market_group_id );
    BOOST_CHECK( betting_market_id(db).description == default_betting_market_description );



    update_sport_by_witness( sport_id );
    BOOST_CHECK( sport_id(db).name == new_sport_name );

    update_event_group_by_witness( event_group_id );
    BOOST_CHECK( event_group_id(db).name == new_event_group_name );

    update_event_by_witness( event_id );
    BOOST_CHECK( event_id(db).name == new_event_name );

    update_event_status_by_witness( event_id );
    BOOST_CHECK( event_id(db).get_status() == new_event_status );

    update_betting_market_group_by_witness( betting_market_group_id );
    BOOST_CHECK( betting_market_group_id(db).description == new_betting_market_group_description );

    GRAPHENE_CHECK_NO_THROW( resolve_betting_market_group_by_witness( betting_market_group_id, betting_market_id ),
        resolve_betting_market_group );

    GRAPHENE_CHECK_NO_THROW( cancel_betting_market_group_unmatched_bets_by_witness( betting_market_group_id ),
        cancel_betting_market_group_unmatched_bets );

    update_betting_market_by_witness( betting_market_id );
    BOOST_CHECK( betting_market_id(db).description == new_betting_market_description );


    
    GRAPHENE_CHECK_NO_THROW( delete_event_group_by_witness( event_group_id ),
        delete_event_group );

    GRAPHENE_CHECK_NO_THROW( delete_sport_by_witness( sport_id ),
        delete_sport );
}

BOOST_AUTO_TEST_CASE( after_hf_create_modify_and_delete_all_obj_with_manager )
{
    ACTOR( alice );

    generate_blocks( HARDFORK_MANAGER_TIME );
    generate_block();



    auto sport_id = create_sport_by_witness( alice_id );
    BOOST_CHECK( sport_id(db).manager == alice_id );

    auto event_group_id = create_event_group_by_fee_payer( sport_id, alice_id, alice_id );
    BOOST_CHECK( event_group_id(db).manager == alice_id );

    auto event_id = create_event_by_fee_payer( event_group_id, alice_id, alice_id );
    BOOST_CHECK( event_id(db).manager == alice_id );

    auto rules_id = create_betting_market_rules_by_witness();

    auto betting_market_group_id = create_betting_market_group_by_fee_payer( event_id, rules_id, alice_id );
    BOOST_CHECK( betting_market_group_id(db).description == default_betting_market_group_description );

    auto betting_market_id = create_betting_market_by_fee_payer( betting_market_group_id, alice_id );
    BOOST_CHECK( betting_market_id(db).description == default_betting_market_description );



    update_sport_by_fee_payer( sport_id, alice_id, alice_id );
    BOOST_CHECK( sport_id(db).name == new_sport_name );

    update_event_group_by_fee_payer( event_group_id, alice_id, alice_id );
    BOOST_CHECK( event_group_id(db).name == new_event_group_name );

    update_event_by_fee_payer( event_id, alice_id, alice_id );
    BOOST_CHECK( event_id(db).name == new_event_name );

    update_event_status_by_witness( event_id );
    BOOST_CHECK( event_id(db).get_status() == new_event_status );

    update_betting_market_group_by_fee_payer( betting_market_group_id, alice_id );
    BOOST_CHECK( betting_market_group_id(db).description == new_betting_market_group_description );

    GRAPHENE_CHECK_NO_THROW( resolve_betting_market_group_by_witness( betting_market_group_id, betting_market_id ),
        resolve_betting_market_group );

    GRAPHENE_CHECK_NO_THROW( cancel_betting_market_group_unmatched_bets_by_fee_payer( betting_market_group_id, alice_id ),
        cancel_betting_market_group_unmatched_bets );

    update_betting_market_by_fee_payer( betting_market_id, alice_id );
    BOOST_CHECK( betting_market_id(db).description == new_betting_market_description );


    
    GRAPHENE_CHECK_NO_THROW( delete_event_group_by_fee_payer( event_group_id, alice_id ),
        delete_event_group );

    GRAPHENE_CHECK_NO_THROW( delete_sport_by_fee_payer( sport_id, alice_id ),
        delete_sport );

}

BOOST_AUTO_TEST_CASE( after_hf_create_modify_and_delete_all_obj_with_manager_set_by_witness )
{
    ACTOR( alice );
    
    generate_blocks( HARDFORK_MANAGER_TIME );
    generate_block();
    

    auto sport_id = create_sport_by_witness( alice_id );
    BOOST_CHECK( sport_id(db).name == default_sport_name 
        && sport_id(db).manager == alice_id );

    auto event_group_id = create_event_group_by_witness( sport_id, alice_id );
    BOOST_CHECK( event_group_id(db).name == default_event_group_name 
        && event_group_id(db).manager == alice_id );

    auto event_id = create_event_by_witness( event_group_id, alice_id );
    BOOST_CHECK( event_id(db).name == default_event_name 
        && event_id(db).manager == alice_id );

    auto rules_id = create_betting_market_rules_by_witness();

    auto betting_market_group_id = create_betting_market_group_by_witness( event_id, rules_id );
    BOOST_CHECK( betting_market_group_id(db).description == default_betting_market_group_description );

    auto betting_market_id = create_betting_market_by_witness( betting_market_group_id );
    BOOST_CHECK( betting_market_id(db).description == default_betting_market_description );



    update_sport_by_witness( sport_id );
    BOOST_CHECK( sport_id(db).name == new_sport_name );

    update_event_group_by_witness( event_group_id );
    BOOST_CHECK( event_group_id(db).name == new_event_group_name );

    update_event_by_witness( event_id );
    BOOST_CHECK( event_id(db).name == new_event_name );

    update_event_status_by_witness( event_id );
    BOOST_CHECK( event_id(db).get_status() == new_event_status );

    update_betting_market_group_by_witness( betting_market_group_id );
    BOOST_CHECK( betting_market_group_id(db).description == new_betting_market_group_description );

    GRAPHENE_CHECK_NO_THROW( resolve_betting_market_group_by_witness( betting_market_group_id, betting_market_id ),
        resolve_betting_market_group );

    GRAPHENE_CHECK_NO_THROW( cancel_betting_market_group_unmatched_bets_by_witness( betting_market_group_id ),
        cancel_betting_market_group_unmatched_bets );

    update_betting_market_by_witness( betting_market_id );
    BOOST_CHECK( betting_market_id(db).description == new_betting_market_description );


    
    GRAPHENE_CHECK_NO_THROW( delete_event_group_by_witness( event_group_id ),
        delete_event_group );

    GRAPHENE_CHECK_NO_THROW( delete_sport_by_witness( sport_id ),
        delete_sport );
}

BOOST_AUTO_TEST_CASE( after_hf_test_is_manager_cascade )
{ try {
    ACTOR( alice );
    auto witness_id = GRAPHENE_WITNESS_ACCOUNT;

    generate_blocks( HARDFORK_MANAGER_TIME );
    generate_block();



    auto sport_id = create_sport_by_witness( alice_id );
    BOOST_CHECK( sport_id(db).manager == alice_id );

    auto event_group_id = create_event_group_by_fee_payer( sport_id, witness_id, alice_id );
    BOOST_CHECK( event_group_id(db).manager == witness_id );

    auto event_id = create_event_by_fee_payer( event_group_id, witness_id, alice_id );
    BOOST_CHECK( event_id(db).manager == witness_id );

    auto rules_id = create_betting_market_rules_by_witness();

    auto betting_market_group_id = create_betting_market_group_by_fee_payer( event_id, rules_id, alice_id );
    BOOST_CHECK( betting_market_group_id(db).description == default_betting_market_group_description );

    auto betting_market_id = create_betting_market_by_fee_payer( betting_market_group_id, alice_id );
    BOOST_CHECK( betting_market_id(db).description == default_betting_market_description );



    update_sport_by_fee_payer( sport_id, alice_id, alice_id );
    BOOST_CHECK( sport_id(db).name == new_sport_name );

    update_event_group_by_fee_payer( event_group_id, witness_id, alice_id );
    BOOST_CHECK( event_group_id(db).name == new_event_group_name );

    update_event_by_fee_payer( event_id, witness_id, alice_id );
    BOOST_CHECK( event_id(db).name == new_event_name );

    update_event_status_by_witness( event_id );
    BOOST_CHECK( event_id(db).get_status() == new_event_status );

    update_betting_market_group_by_fee_payer( betting_market_group_id, alice_id );
    BOOST_CHECK( betting_market_group_id(db).description == new_betting_market_group_description );

    GRAPHENE_CHECK_NO_THROW( resolve_betting_market_group_by_witness( betting_market_group_id, betting_market_id ),
        resolve_betting_market_group );

    GRAPHENE_CHECK_NO_THROW( cancel_betting_market_group_unmatched_bets_by_fee_payer( betting_market_group_id, alice_id ),
        cancel_betting_market_group_unmatched_bets );

    update_betting_market_by_fee_payer( betting_market_id, alice_id );
    BOOST_CHECK( betting_market_id(db).description == new_betting_market_description );


    
    GRAPHENE_CHECK_NO_THROW( delete_event_group_by_fee_payer( event_group_id, alice_id ),
        delete_event_group );

    GRAPHENE_CHECK_NO_THROW( delete_sport_by_fee_payer( sport_id, alice_id ),
        delete_sport );

} FC_LOG_AND_RETHROW() }


BOOST_AUTO_TEST_CASE( after_hf_check_relative_object_creation_by_witness )
{ try {
    ACTOR( alice );
    auto witness_id =  GRAPHENE_WITNESS_ACCOUNT;

    generate_blocks( HARDFORK_MANAGER_TIME );
    generate_block();



    sport_id_type rel_sport_id;
    event_group_id_type rel_event_group_id;
    event_id_type rel_event_id;
    betting_market_group_id_type rel_betting_market_group_id;
    betting_market_rules_id_type rules_id = create_betting_market_rules_by_witness();
    betting_market_id_type rel_betting_market_id;

    auto sport_create_op = create_sport_create_op( alice_id );
    auto event_group_create_op = create_event_group_create_op( rel_sport_id, alice_id );
    auto event_create_op = create_event_create_op( rel_event_group_id, alice_id );
    auto betting_market_group_create_op = create_betting_market_group_create_op( rel_event_id, rules_id );
    auto betting_market_create_op = create_betting_market_create_op( rel_betting_market_group_id );



    proposal_create_operation pcop;
    pcop.fee_paying_account = witness_id;
    pcop.expiration_time = db.head_block_time() + fc::hours(24);
    pcop.proposed_ops.emplace_back( sport_create_op );
    pcop.proposed_ops.emplace_back( event_group_create_op );
    pcop.proposed_ops.emplace_back( event_create_op );
    pcop.proposed_ops.emplace_back( betting_market_group_create_op );
    pcop.proposed_ops.emplace_back( betting_market_create_op );

    auto process_proposal_by_witness = [this, &pcop]() {
        signed_transaction tx;
        tx.operations.push_back(pcop);
        set_expiration(db, tx);
        sign(tx, init_account_priv_key);
        
        processed_transaction processed_tx = db.push_transaction(tx);
        proposal_id_type proposal_id = processed_tx.operation_results[0].get<object_id_type>();

        auto& witnesses_set = db.get_global_properties().active_witnesses;
        std::vector<witness_id_type> witnesses_vec( witnesses_set.begin(), witnesses_set.end() );
        process_proposal_by_witnesses( witnesses_vec, proposal_id, false );
    };
    process_proposal_by_witness();



    const auto& s_obj   = *db.get_index_type<sport_object_index>().indices().get<by_id>().begin();
    const auto& eg_obj  = *db.get_index_type<event_group_object_index>().indices().get<by_id>().begin();
    const auto& e_obj   = *db.get_index_type<event_object_index>().indices().get<by_id>().begin();
    const auto& bmg_obj = *db.get_index_type<betting_market_group_object_index>().indices().get<by_id>().begin();
    const auto& bm_obj  = *db.get_index_type<betting_market_object_index>().indices().get<by_id>().begin();

    BOOST_CHECK( s_obj.manager == alice_id );
    BOOST_CHECK( eg_obj.manager == alice_id );
    BOOST_CHECK( e_obj.manager == alice_id );
    BOOST_CHECK( bmg_obj.description == default_betting_market_group_description );
    BOOST_CHECK( bm_obj.description == default_betting_market_description );

} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
