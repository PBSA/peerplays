#include <boost/test/unit_test.hpp>
#include "../common/database_fixture.hpp"

#include <fc/crypto/digest.hpp>

#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/sidechain_proposal_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/primary_wallet_vout_object.hpp>

using namespace graphene::chain;
using namespace sidechain;

template< class SidechainOp > 
proposal_object create_op_and_prop( database& db, bool check_size = true )
{
   const auto& sidechain_proposal_idx = db.get_index_type<sidechain_proposal_index>().indices().get<by_proposal>();

   BOOST_CHECK( !check_size || sidechain_proposal_idx.size() == 0 );

   const flat_set<witness_id_type>& active_witnesses = db.get_global_properties().active_witnesses;
   const witness_id_type& witness_id = *active_witnesses.begin();
   const witness_object witness = witness_id(db);
   const account_object& witness_account = witness.witness_account(db);

   const auto& propose = db.create<proposal_object>( [&]( proposal_object& obj ){
      obj.expiration_time =  db.head_block_time() + fc::days(1);
      obj.review_period_time = db.head_block_time() + fc::days(1);
      obj.proposed_transaction.operations.push_back( SidechainOp() );
   } );

   const auto& spropose = db.create<sidechain_proposal_object>( [&]( sidechain_proposal_object& obj ){
      obj.proposal_id = propose.id;
   } );

   return propose;
}

template< class SidechainOp >
void check_fork_prop( database& db )
{
   db.modify( db.get_global_properties(), [&]( global_property_object& gpo ) {
      gpo.parameters.extensions.value.sidechain_parameters.reset();
      if( gpo.pending_parameters )
         gpo.pending_parameters->extensions.value.sidechain_parameters.reset();
   });

   BOOST_CHECK( db.is_sidechain_fork_needed() );

   transaction_evaluation_state context(&db);

   proposal_create_operation proposal_op;
   proposal_op.fee_paying_account = account_id_type();
   proposal_op.proposed_ops.emplace_back( SidechainOp() );
   proposal_op.expiration_time =  db.head_block_time() + fc::days(1);

   BOOST_CHECK_THROW( db.apply_operation( context, proposal_op ), fc::exception );
}

/////////////////////////////////// functions for roll_back tests

inline proposal_object create_proposals( database& db, bitcoin_transaction_send_operation op )
{
   const auto& propose_send = db.create<proposal_object>( [&]( proposal_object& obj ){
      obj.expiration_time =  db.head_block_time() + fc::days(1);
      obj.review_period_time = db.head_block_time() + fc::days(1);
      obj.proposed_transaction.operations.push_back( op );
   } );
   db.create<sidechain_proposal_object>( [&]( sidechain_proposal_object& obj ){
      obj.proposal_id = propose_send.id;
      obj.proposal_type = sidechain_proposal_type::SEND_BTC_TRANSACTION;
   } );

   return propose_send;
}

inline bitcoin_transaction_send_operation create_btc_tx_send_op( const test_environment& env, uint32_t offset = 0 )
{
   bitcoin_transaction_send_operation op; 
   op.pw_vin = env.pw_vin;
   op.vins = std::vector<info_for_vin>( env.vins.begin(), env.vins.end() - offset );
   for( const auto& vout : std::vector<info_for_vout>( env.vouts.begin(), env.vouts.end() - offset ) ) {
       op.vouts.push_back( vout.get_id() );
   }
   op.transaction = env.full_tx.first;
   op.fee_for_size = env.full_tx.second;
   return op;
}

inline void mark_vin( database& db, const test_environment& env )
{
   for( auto& iter: env.vins ) {
      db.i_w_info.mark_as_used_vin( iter );
   }
}

inline void mark_vout( database& db, const test_environment& env )
{
   for( auto& iter: env.vouts ) {
      db.i_w_info.mark_as_used_vout( iter );
   }
}

inline void mark_vin_vout_pw( database& db, const test_environment& env )
{
   mark_vin( db, env );
   mark_vout( db, env );
   db.pw_vout_manager.mark_as_used_vout( env.pw_vin.identifier );
}

///////////////////////////////////

BOOST_FIXTURE_TEST_SUITE( db_sidechain_tests, database_fixture )

BOOST_AUTO_TEST_CASE( remove_sidechain_proposals_test )
{
   const auto& sidechain_proposal_idx = db.get_index_type<sidechain_proposal_index>().indices().get<by_proposal>();

   BOOST_CHECK_EQUAL( sidechain_proposal_idx.size(), 0 );

   const auto& propose_send = create_op_and_prop<bitcoin_transaction_send_operation>( db, false );
   const auto& propose_issue = create_op_and_prop<bitcoin_issue_operation>( db, false );
   const auto& propose_revert = create_op_and_prop<bitcoin_transaction_revert_operation>( db, false );

   create_op_and_prop<bitcoin_transaction_send_operation>( db, false );
   create_op_and_prop<bitcoin_issue_operation>( db, false );
   create_op_and_prop<bitcoin_transaction_revert_operation>( db, false );

   BOOST_CHECK_EQUAL( sidechain_proposal_idx.size(), 6 );

   db.remove_sidechain_proposal_object( propose_send );
   db.remove_sidechain_proposal_object( propose_issue );
   db.remove_sidechain_proposal_object( propose_revert );

   BOOST_CHECK_EQUAL( sidechain_proposal_idx.size(), 3 );

   const auto& propose_transfer = db.create<proposal_object>( [&]( proposal_object& obj ){
      obj.expiration_time =  db.head_block_time() + fc::days(1);
      obj.review_period_time = db.head_block_time() + fc::days(1);
      obj.proposed_transaction.operations.push_back( transfer_operation() );
   } );

   db.remove_sidechain_proposal_object( propose_transfer );

   BOOST_CHECK_EQUAL( sidechain_proposal_idx.size(), 3 );
}

BOOST_AUTO_TEST_CASE( sidechain_fork_btc_issue_op_test )
{
   check_fork_prop<bitcoin_issue_operation> ( db );
}

BOOST_AUTO_TEST_CASE( sidechain_fork_btc_send_op_test )
{
   check_fork_prop<bitcoin_transaction_send_operation> ( db );
}

BOOST_AUTO_TEST_CASE( sidechain_fork_btc_revert_op_test )
{
   check_fork_prop<bitcoin_transaction_revert_operation> ( db );
}

BOOST_AUTO_TEST_CASE( roll_back_vin_test )
{
   test_environment env( db );

   mark_vin( db, env );

   bitcoin_transaction_send_operation op = create_btc_tx_send_op( env );
   const auto& propose_send = create_proposals( db, op );

   BOOST_CHECK_EQUAL( db.get_index_type<sidechain_proposal_index>().indices().get<by_proposal>().size(), 1 );
   BOOST_CHECK_EQUAL( db.i_w_info.get_info_for_vins().size(), 0 );
   BOOST_CHECK( !db.pw_vout_manager.get_vout( op.pw_vin.identifier )->used );

   db.roll_back_vin_and_vout( propose_send );

   BOOST_CHECK_EQUAL( db.i_w_info.get_info_for_vins().size(), 5 );

   BOOST_CHECK( !db.pw_vout_manager.get_vout( op.pw_vin.identifier )->used );
   BOOST_CHECK_EQUAL( db.get_index_type<sidechain_proposal_index>().indices().get<by_proposal>().size(), 0 );
}

BOOST_AUTO_TEST_CASE( roll_back_vout_test )
{
   test_environment env( db );

   mark_vout( db, env );

   bitcoin_transaction_send_operation op = create_btc_tx_send_op( env );
   const auto& propose_send = create_proposals( db, op );

   BOOST_CHECK_EQUAL( db.get_index_type<sidechain_proposal_index>().indices().get<by_proposal>().size(), 1 );
   BOOST_CHECK_EQUAL( db.i_w_info.get_info_for_vouts().size(), 0 );
   BOOST_CHECK( !db.pw_vout_manager.get_vout( op.pw_vin.identifier )->used );

   db.roll_back_vin_and_vout( propose_send );

   BOOST_CHECK_EQUAL( db.i_w_info.get_info_for_vouts().size(), 5 );

   BOOST_CHECK( !db.pw_vout_manager.get_vout( op.pw_vin.identifier )->used );
   BOOST_CHECK_EQUAL( db.get_index_type<sidechain_proposal_index>().indices().get<by_proposal>().size(), 0 );
}

BOOST_AUTO_TEST_CASE( roll_back_vin_and_vout_test )
{
   test_environment env( db );

   mark_vin_vout_pw( db, env );
   bitcoin_transaction_send_operation op = create_btc_tx_send_op( env );
   const auto& propose_send = create_proposals( db, op );

   BOOST_CHECK_EQUAL( db.get_index_type<sidechain_proposal_index>().indices().get<by_proposal>().size(), 1 );
   BOOST_CHECK_EQUAL( db.i_w_info.get_info_for_vins().size(), 0 );
   BOOST_CHECK_EQUAL( db.i_w_info.get_info_for_vouts().size(), 0 );
   BOOST_CHECK( db.pw_vout_manager.get_vout( op.pw_vin.identifier )->used );
   db.roll_back_vin_and_vout( propose_send );

   BOOST_CHECK_EQUAL( db.i_w_info.get_info_for_vins().size(), 5 );
   BOOST_CHECK_EQUAL( db.i_w_info.get_info_for_vouts().size(), 5 );

   BOOST_CHECK( !db.pw_vout_manager.get_vout( op.pw_vin.identifier )->used );
   BOOST_CHECK_EQUAL( db.get_index_type<sidechain_proposal_index>().indices().get<by_proposal>().size(), 0 );
}

BOOST_AUTO_TEST_CASE( roll_back_part_test )
{
   test_environment env( db );

   mark_vin_vout_pw( db, env );
   bitcoin_transaction_send_operation op = create_btc_tx_send_op( env, 2 );

   const auto& propose_send = create_proposals( db, op );
   BOOST_CHECK_EQUAL( db.get_index_type<sidechain_proposal_index>().indices().get<by_proposal>().size(), 1 );
   BOOST_CHECK_EQUAL( db.i_w_info.get_info_for_vins().size(), 0 );
   BOOST_CHECK_EQUAL( db.i_w_info.get_info_for_vouts().size(), 0 );
   BOOST_CHECK( db.pw_vout_manager.get_vout( op.pw_vin.identifier )->used );
   db.roll_back_vin_and_vout( propose_send );

   BOOST_REQUIRE_EQUAL( db.i_w_info.get_info_for_vins().size(), 3 );
   BOOST_REQUIRE_EQUAL( db.i_w_info.get_info_for_vouts().size(), 3 );

   BOOST_CHECK( !db.pw_vout_manager.get_vout( op.pw_vin.identifier )->used );
   BOOST_CHECK_EQUAL( db.get_index_type<sidechain_proposal_index>().indices().get<by_proposal>().size(), 0 );

   auto info_vins = db.i_w_info.get_info_for_vins();
   auto info_vouts = db.i_w_info.get_info_for_vouts();

   BOOST_REQUIRE_EQUAL( op.vins.size(), info_vins.size() );
   BOOST_REQUIRE_EQUAL( op.vouts.size(), info_vouts.size() );

   // no match for `operator==`; but `operator!=` exists
   for( uint32_t indx = 0; indx < op.vins.size(); indx++ ) {
      BOOST_CHECK( !( op.vins[indx] != info_vins[indx] ) );
   }

   for( uint32_t indx = 0; indx < info_vouts.size(); indx++ ) {
      BOOST_CHECK( env.vouts[indx] == info_vouts[indx] );
   }
}

BOOST_AUTO_TEST_CASE( not_enough_amount_for_fee )
{
   test_environment env( db );

   auto vins = env.vins;
   for( auto& vin: vins ) {
      db.i_w_info.modify_info_for_vin( vin, [&]( info_for_vin& obj ) {
         obj.out.amount = 1;
         obj.identifier = fc::sha256::hash( obj.out.hash_tx + std::to_string( obj.out.n_vout ) );
      } );
      vin.out.amount = 1;
   }

   auto tx_and_new_infos =  db.create_tx_with_valid_vin( env.vouts, env.pw_vin );

   BOOST_CHECK_EQUAL( tx_and_new_infos.second.size(), 0 );
   BOOST_CHECK_EQUAL( db.i_w_info.get_info_for_vins().size(), 0 );
}

BOOST_AUTO_TEST_CASE( not_enough_amount_for_fee_tree_of_five )
{
   test_environment env( db );

   auto vins = env.vins;

   for( uint32_t i = 0; i < 3; ++i ) {
      db.i_w_info.modify_info_for_vin( vins[i], [&]( info_for_vin& obj ) {
         obj.out.amount = 100 + i;
         obj.identifier = fc::sha256::hash( obj.out.hash_tx + std::to_string( obj.out.n_vout ) );
      } );
      vins[i].out.amount = 100 + i;
   }

   auto tx_and_new_infos =  db.create_tx_with_valid_vin( env.vouts, env.pw_vin );

   BOOST_CHECK_EQUAL( tx_and_new_infos.second.size(), 2 );
   BOOST_CHECK_EQUAL( db.i_w_info.get_info_for_vins().size(), 2 );
}

BOOST_AUTO_TEST_CASE( enough_amount_for_fee )
{
   test_environment env( db );

   auto tx_and_new_infos =  db.create_tx_with_valid_vin( env.vouts, env.pw_vin );

   BOOST_CHECK_EQUAL( tx_and_new_infos.second.size(), 5 );
   BOOST_CHECK_EQUAL( db.i_w_info.get_info_for_vins().size(), 5 );
}

BOOST_AUTO_TEST_SUITE_END()