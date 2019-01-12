#include <boost/test/unit_test.hpp>
#include "../common/database_fixture.hpp"

#include <fc/crypto/digest.hpp>

#include <graphene/chain/bitcoin_address_object.hpp>

using namespace graphene::chain;

BOOST_FIXTURE_TEST_SUITE( bitcoin_addresses_obj_tests, database_fixture )

BOOST_AUTO_TEST_CASE( create_bitcoin_address_test ) {
   transaction_evaluation_state context(&db);

   bitcoin_address_create_operation op;
   op.payer = account_id_type();
   op.owner = account_id_type();

   const auto& idx = db.get_index_type<bitcoin_address_index>().indices().get< by_id >();

   BOOST_CHECK( idx.size() == 0 );

   db.apply_operation( context, op );

   auto btc_address = idx.begin();
   BOOST_CHECK(btc_address->count_invalid_pub_key == 1);
}

BOOST_AUTO_TEST_SUITE_END()