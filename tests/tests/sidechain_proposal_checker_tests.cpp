#include <boost/test/unit_test.hpp>
#include "../common/database_fixture.hpp"
#include <sidechain/sidechain_proposal_checker.hpp>

using namespace sidechain;

BOOST_FIXTURE_TEST_SUITE( sidechain_proposal_checker_tests, database_fixture )

class test_environment
{

public:

   test_environment( database& db )
   {

      for( size_t i = 1; i < 6; i++ ) {
         prev_out out{ std::string( 64, std::to_string( i )[0] ), static_cast<uint32_t>( i ), static_cast<uint64_t>( i * 10000 ) };
         db.i_w_info.insert_info_for_vin( out, std::to_string( i ), { 0x00, 0x01, 0x02 } );
         db.i_w_info.insert_info_for_vout( account_id_type( i ), "2Mt57VSFqBe7UpDad9QaYHev21E1VscAZMU", i * 5000 );
      }

      vins = db.i_w_info.get_info_for_vins();
      pw_vin = *db.i_w_info.get_info_for_pw_vin();
      vouts = db.i_w_info.get_info_for_vouts();
      full_tx = db.create_btc_transaction( vins, vouts, pw_vin );
   }

   std::vector<info_for_vin> vins;
   info_for_vin pw_vin;
   std::vector<info_for_vout> vouts;
   full_btc_transaction full_tx;

};

BOOST_AUTO_TEST_CASE( check_reuse_normal_test )
{
   sidechain_proposal_checker checker( db );
   bitcoin_transaction_send_operation op;

   op.pw_vin = info_for_vin( { std::string( 64, '1' ), 0, 0 }, "", bytes() );
   for( size_t i = 0; i < 5; i++ ) {
      op.vins.push_back( info_for_vin( { std::string( 64, std::to_string( i )[0] ), 0, 0 }, "", bytes() ) );
      op.vouts.push_back( info_for_vout_id_type( i ) );
   }

   BOOST_CHECK( checker.check_reuse( op ) );
}

BOOST_AUTO_TEST_CASE( check_reuse_not_normal_test )
{
   sidechain_proposal_checker checker( db );
   bitcoin_transaction_send_operation op;

   op.pw_vin = info_for_vin( { std::string( 64, '1' ), 0, 0 }, "", bytes() );
   for( size_t i = 0; i < 5; i++ ) {
      op.vins.push_back( info_for_vin( { std::string( 64, std::to_string( i )[0] ), 0, 0 }, "", bytes() ) );
      op.vouts.push_back( info_for_vout_id_type( i ) );
   }

   BOOST_CHECK( checker.check_reuse( op ) );
   BOOST_CHECK( !checker.check_reuse( op ) );
}

BOOST_AUTO_TEST_CASE( check_btc_tx_send_op_normal_test )
{
   sidechain_proposal_checker checker( db );
   test_environment env( db );

   bitcoin_transaction_send_operation op;
   op.pw_vin = env.pw_vin;
   op.vins = env.vins;
   for( const auto& vout : env.vouts ) {
      op.vouts.push_back( vout.get_id() );
   }
   op.transaction = env.full_tx.first;
   op.fee_for_size = env.full_tx.second;

   BOOST_CHECK( checker.check_bitcoin_transaction_send_operation( op ) );
}

BOOST_AUTO_TEST_CASE( check_btc_tx_send_op_incorrect_info_for_vin_test )
{
   sidechain_proposal_checker checker( db );
   test_environment env( db );

   env.vins[3].out.amount = 13;

   bitcoin_transaction_send_operation op;
   op.pw_vin = env.pw_vin;
   op.vins = env.vins;
   for( const auto& vout : env.vouts ) {
      op.vouts.push_back( vout.get_id() );
   }
   op.transaction = env.full_tx.first;
   op.fee_for_size = env.full_tx.second;

   BOOST_CHECK( !checker.check_bitcoin_transaction_send_operation( op ) );
}

BOOST_AUTO_TEST_CASE( check_btc_tx_send_op_incorrect_info_for_pw_vin_test )
{
   sidechain_proposal_checker checker( db );
   test_environment env( db );

   env.pw_vin.out.amount = 13;

   bitcoin_transaction_send_operation op;
   op.pw_vin = env.pw_vin;
   op.vins = env.vins;
   for( const auto& vout : env.vouts ) {
      op.vouts.push_back( vout.get_id() );
   }
   op.transaction = env.full_tx.first;
   op.fee_for_size = env.full_tx.second;

   BOOST_CHECK( !checker.check_bitcoin_transaction_send_operation( op ) );
}

BOOST_AUTO_TEST_CASE( check_btc_tx_send_op_incorrect_tx_test )
{
   sidechain_proposal_checker checker( db );
   test_environment env( db );

   bitcoin_transaction_send_operation op;
   op.pw_vin = env.pw_vin;
   op.vins = env.vins;
   for( const auto& vout : env.vouts ) {
      op.vouts.push_back( vout.get_id() );
   }

   env.full_tx.first.vout[0].value = 13;

   op.transaction = env.full_tx.first;
   op.fee_for_size = env.full_tx.second;

   BOOST_CHECK( !checker.check_bitcoin_transaction_send_operation( op ) );
}

BOOST_AUTO_TEST_SUITE_END()
