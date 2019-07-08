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

BOOST_AUTO_TEST_CASE( create_son_test ){
    std::string test_url = "https://create_son_test";
    test_create_son_member_evaluator test_eval( db );
    son_member_create_operation op;
    op.owner_account = account_id_type(1);
    op.url = test_url;

    BOOST_CHECK_NO_THROW( test_eval.do_evaluate( op ) );
    auto id = test_eval.do_apply( op );
    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_account>();

    BOOST_REQUIRE( idx.size() == 1 );

    auto obj = idx.find( op.owner_account );
    BOOST_REQUIRE( obj != idx.end() );
    BOOST_CHECK( obj->url == test_url );
    BOOST_CHECK( id == obj->id );
}

BOOST_AUTO_TEST_CASE( delete_son_test ){
    INVOKE(create_son_test);
    test_delete_son_member_evaluator test_eval( db );

    son_member_delete_operation delete_op;
    delete_op.owner_account = account_id_type(1);
    BOOST_CHECK_NO_THROW( test_eval.do_evaluate( delete_op ) );
    test_eval.do_apply( delete_op );

    const auto& idx = db.get_index_type<son_member_index>().indices().get<by_account>();
    BOOST_REQUIRE( idx.empty() );
}

BOOST_AUTO_TEST_SUITE_END()