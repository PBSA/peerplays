#include <boost/test/unit_test.hpp>

#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/sport_object.hpp>
#include <graphene/chain/event_group_object.hpp>
#include <graphene/chain/betting_market_object.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/protocol/proposal.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/protocol/committee_member.hpp>
#include <fc/crypto/digest.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

namespace
{
    transfer_operation make_transfer_operation(const account_id_type& from, const account_id_type& to, const asset& amount)
    {
        transfer_operation transfer;
        transfer.from = from;
        transfer.to   = to;
        transfer.amount = amount;

        return transfer;
    }

    committee_member_create_operation make_committee_member_create_operation(const asset& fee, const account_id_type& member, const string& url)
    {
        committee_member_create_operation member_create_operation;
        member_create_operation.fee = fee;
        member_create_operation.committee_member_account = member;
        member_create_operation.url = url;

        return member_create_operation;
    }

    void create_proposal(database_fixture& fixture, const std::vector<operation>& operations)
    {
        signed_transaction transaction;
        set_expiration(fixture.db, transaction);

        transaction.operations = operations;

        fixture.db.create<proposal_object>([&](proposal_object& proposal)
        {
            proposal.proposed_transaction = transaction;
        });
    }

    signed_transaction make_signed_transaction_with_proposed_operation(database_fixture& fixture, const std::vector<operation>& operations)
    {
        proposal_create_operation operation_proposal;

        for (auto& operation: operations)
        {
            operation_proposal.proposed_ops.push_back(op_wrapper(operation));
        }

        signed_transaction transaction;
        set_expiration(fixture.db, transaction);
        transaction.operations = {operation_proposal};

        return transaction;
    }

    void push_proposal(database_fixture& fixture, const account_object& fee_payer, const std::vector<operation>& operations)
    {
        proposal_create_operation operation_proposal;
        operation_proposal.fee_paying_account = fee_payer.id;

        for (auto& operation: operations)
        {
            operation_proposal.proposed_ops.push_back(op_wrapper(operation));
        }

        operation_proposal.expiration_time = fixture.db.head_block_time() + fc::days(1);

        signed_transaction transaction;
        transaction.operations.push_back(operation_proposal);
        set_expiration( fixture.db, transaction );

        fixture.sign( transaction,  fixture.init_account_priv_key  );
        PUSH_TX( fixture.db, transaction );
    }
}

BOOST_FIXTURE_TEST_SUITE( check_tansaction_for_duplicated_operations, database_fixture )

BOOST_AUTO_TEST_CASE( test_exception_throwing_for_the_same_operation_proposed_twice )
{
    try
    {
        ACTORS((alice))

        create_proposal(*this, {make_transfer_operation(account_id_type(), alice_id, asset(500))});

        auto trx = make_signed_transaction_with_proposed_operation(*this, {make_transfer_operation(account_id_type(), alice_id, asset(500))});
        BOOST_CHECK_THROW(db.check_tansaction_for_duplicated_operations(trx), fc::exception);
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( check_passes_without_duplication )
{
    try
    {
        ACTORS((alice))

        auto trx = make_signed_transaction_with_proposed_operation(*this, {make_transfer_operation(account_id_type(), alice_id, asset(500))});
        BOOST_CHECK_NO_THROW(db.check_tansaction_for_duplicated_operations(trx));
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( check_passes_for_the_same_operation_with_different_assets )
{
    try
    {
        ACTORS((alice))

        create_proposal(*this, {make_transfer_operation(account_id_type(), alice_id, asset(500))});

        auto trx = make_signed_transaction_with_proposed_operation(*this, {make_transfer_operation(account_id_type(), alice_id, asset(501))});
        BOOST_CHECK_NO_THROW(db.check_tansaction_for_duplicated_operations(trx));
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( check_fails_for_duplication_in_transaction_with_several_operations )
{
    try
    {
        ACTORS((alice))

        create_proposal(*this, {make_transfer_operation(account_id_type(), alice_id, asset(500))});

        auto trx = make_signed_transaction_with_proposed_operation(*this, {make_transfer_operation(account_id_type(), alice_id, asset(501)),
                                                                           make_transfer_operation(account_id_type(), alice_id, asset(500))}); //duplicated one
        BOOST_CHECK_THROW(db.check_tansaction_for_duplicated_operations(trx), fc::exception);
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( check_fails_for_duplicated_operation_in_existed_proposal_with_several_operations_and_transaction_with_several_operations )
{
    try
    {
        ACTORS((alice))

        create_proposal(*this, {make_transfer_operation(account_id_type(), alice_id, asset(499)),
                                make_transfer_operation(account_id_type(), alice_id, asset(500))}); //duplicated one

        auto trx = make_signed_transaction_with_proposed_operation(*this, {make_transfer_operation(account_id_type(), alice_id, asset(501)),
                                                                           make_transfer_operation(account_id_type(), alice_id, asset(500))}); //duplicated one
        BOOST_CHECK_THROW(db.check_tansaction_for_duplicated_operations(trx), fc::exception);
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( check_fails_for_duplicated_operation_in_existed_proposal_with_several_operations )
{
    try
    {
        ACTORS((alice))

        create_proposal(*this, {make_transfer_operation(account_id_type(), alice_id, asset(499)),
                                make_transfer_operation(account_id_type(), alice_id, asset(500))}); //duplicated one

        auto trx = make_signed_transaction_with_proposed_operation(*this, {make_transfer_operation(account_id_type(), alice_id, asset(500))}); //duplicated one
        BOOST_CHECK_THROW(db.check_tansaction_for_duplicated_operations(trx), fc::exception);
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( check_passes_for_different_operations_types )
{
    try
    {
        ACTORS((alice))

        create_proposal(*this, {make_transfer_operation(account_id_type(), alice_id, asset(500))});

        auto trx = make_signed_transaction_with_proposed_operation(*this, {make_committee_member_create_operation(asset(1000), account_id_type(), "test url")});
        BOOST_CHECK_NO_THROW(db.check_tansaction_for_duplicated_operations(trx));
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( check_fails_for_same_member_create_operations )
{
    try
    {
        create_proposal(*this, {make_committee_member_create_operation(asset(1000), account_id_type(), "test url")});

        auto trx = make_signed_transaction_with_proposed_operation(*this, {make_committee_member_create_operation(asset(1000), account_id_type(), "test url")});
        BOOST_CHECK_THROW(db.check_tansaction_for_duplicated_operations(trx), fc::exception);
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( check_passes_for_different_member_create_operations )
{
    try
    {
        create_proposal(*this, {make_committee_member_create_operation(asset(1000), account_id_type(), "test url")});

        auto trx = make_signed_transaction_with_proposed_operation(*this, {make_committee_member_create_operation(asset(1001), account_id_type(), "test url")});
        BOOST_CHECK_NO_THROW(db.check_tansaction_for_duplicated_operations(trx));
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( check_failes_for_several_operations_of_mixed_type )
{
    try
    {
        ACTORS((alice))

        create_proposal(*this, {make_transfer_operation(account_id_type(), alice_id, asset(500)),
                                make_committee_member_create_operation(asset(1000), account_id_type(), "test url")});

        create_proposal(*this, {make_transfer_operation(account_id_type(), alice_id, asset(501)), //duplicate
                                make_committee_member_create_operation(asset(1001), account_id_type(), "test url")});

        auto trx = make_signed_transaction_with_proposed_operation(*this, {make_transfer_operation(account_id_type(), alice_id, asset(501)), //duplicate
                                                                           make_committee_member_create_operation(asset(1002), account_id_type(), "test url")});

        BOOST_CHECK_THROW(db.check_tansaction_for_duplicated_operations(trx), fc::exception);
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( check_failes_for_duplicates_in_pending_transactions_list )
{
    try
    {
        ACTORS((alice))

        fc::ecc::private_key committee_key = init_account_priv_key;

        const account_object& moneyman = create_account("moneyman", init_account_pub_key);
        const asset_object& core = asset_id_type()(db);

        transfer(account_id_type()(db), moneyman, core.amount(1000000));

        auto duplicate = make_transfer_operation(alice.id, moneyman.get_id(), asset(100));
        push_proposal(*this, moneyman, {duplicate});

        auto trx = make_signed_transaction_with_proposed_operation(*this, {duplicate});
        BOOST_CHECK_THROW(db.check_tansaction_for_duplicated_operations(trx), fc::exception);
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( check_passes_for_no_duplicates_in_pending_transactions_list )
{
    try
    {
        ACTORS((alice))

        fc::ecc::private_key committee_key = init_account_priv_key;

        const account_object& moneyman = create_account("moneyman", init_account_pub_key);
        const asset_object& core = asset_id_type()(db);

        transfer(account_id_type()(db), moneyman, core.amount(1000000));

        push_proposal(*this, moneyman, {make_transfer_operation(alice.id, moneyman.get_id(), asset(100))});

        auto trx = make_signed_transaction_with_proposed_operation(*this, {make_transfer_operation(alice.id, moneyman.get_id(), asset(101))});
        BOOST_CHECK_NO_THROW(db.check_tansaction_for_duplicated_operations(trx));
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( check_fails_for_several_transactions_with_duplicates_in_pending_list )
{
    try
    {
        ACTORS((alice))

        fc::ecc::private_key committee_key = init_account_priv_key;

        const account_object& moneyman = create_account("moneyman", init_account_pub_key);
        const asset_object& core = asset_id_type()(db);

        transfer(account_id_type()(db), moneyman, core.amount(1000000));

        auto duplicate = make_transfer_operation(alice.id, moneyman.get_id(), asset(100));
        push_proposal(*this, moneyman, {make_transfer_operation(alice.id, moneyman.get_id(), asset(101)),
                                        duplicate});

        auto trx = make_signed_transaction_with_proposed_operation(*this, {duplicate,
                                                                           make_transfer_operation(alice.id, moneyman.get_id(), asset(102))});
        BOOST_CHECK_THROW(db.check_tansaction_for_duplicated_operations(trx), fc::exception);
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE( check_passes_for_duplicated_betting_market_or_group )
{
    generate_blocks( HARDFORK_1000_TIME + fc::seconds(300) );

    try
    {
        const sport_id_type sport_id = create_sport( {{"SN","SPORT_NAME"}} ).id;
        const event_group_id_type event_group_id = create_event_group( {{"EG", "EVENT_GROUP"}}, sport_id ).id;
        const betting_market_rules_id_type betting_market_rules_id = 
            create_betting_market_rules( {{"EN", "Rules"}}, {{"EN", "Some rules"}} ).id;
        
        event_create_operation evcop1;
        evcop1.event_group_id = event_group_id;
        evcop1.name           = {{"NO", "NAME_ONE"}};
        evcop1.season         = {{"NO", "NAME_ONE"}};

        event_create_operation evcop2;
        evcop2.event_group_id = event_group_id;
        evcop2.name           = {{"NT", "NAME_TWO"}};
        evcop2.season         = {{"NT", "NAME_TWO"}};

        betting_market_group_create_operation bmgcop;
        bmgcop.description = {{"NN", "NO_NAME"}};
        bmgcop.event_id = object_id_type(relative_protocol_ids, 0, 0);
        bmgcop.rules_id = betting_market_rules_id;
        bmgcop.asset_id = asset_id_type();      

        betting_market_create_operation bmcop;
        bmcop.group_id = object_id_type(relative_protocol_ids, 0, 1);
        bmcop.payout_condition.insert( internationalized_string_type::value_type( "CN", "CONDI_NAME" ) );

        proposal_create_operation pcop1 = proposal_create_operation::committee_proposal( 
            db.get_global_properties().parameters,
            db.head_block_time() 
        );
        pcop1.review_period_seconds.reset();

        proposal_create_operation pcop2 = pcop1;

        pcop1.proposed_ops.emplace_back( evcop1 );
        pcop1.proposed_ops.emplace_back( bmgcop );
        pcop1.proposed_ops.emplace_back( bmcop  );

        pcop2.proposed_ops.emplace_back( evcop2 );
        pcop2.proposed_ops.emplace_back( bmgcop );
        pcop2.proposed_ops.emplace_back( bmcop  );

        create_proposal(*this, { pcop1, pcop2 });

        auto trx = make_signed_transaction_with_proposed_operation(*this, { pcop1, pcop2 });
        BOOST_CHECK_NO_THROW( db.check_tansaction_for_duplicated_operations(trx) );
    }
    catch( const fc::exception& e )
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_SUITE_END()
