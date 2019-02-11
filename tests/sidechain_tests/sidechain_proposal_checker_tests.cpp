#include <boost/test/unit_test.hpp>
#include "../common/database_fixture.hpp"
#include <sidechain/sidechain_proposal_checker.hpp>

#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/proposal_evaluator.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/bitcoin_transaction_object.hpp>

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

BOOST_AUTO_TEST_CASE( twice_approve_btc_tx_send_test )
{
   sidechain_proposal_checker checker( db );
   const flat_set<witness_id_type>& active_witnesses = db.get_global_properties().active_witnesses;
   const auto& proposal_idx = db.get_index_type<graphene::chain::proposal_index>().indices().get< graphene::chain::by_id >();
   transaction_evaluation_state context(&db);

   auto propose = db.create<proposal_object>( [&]( proposal_object& obj ){
      obj.expiration_time =  db.head_block_time() + fc::days(1);
      obj.review_period_time = db.head_block_time() + fc::days(1);
   } );

   const witness_id_type& witness_id = *active_witnesses.begin();
   const witness_object witness = witness_id(db);
   const account_object& witness_account = witness.witness_account(db);

   proposal_update_operation op;
   {
      op.proposal = propose.id;
      op.fee_paying_account = witness_account.id;
      op.active_approvals_to_add.insert( witness_account.id );
   }

   BOOST_CHECK( proposal_idx.size() == 1 );
   BOOST_CHECK_EQUAL( proposal_idx.begin()->available_active_approvals.size(), 0 );
   BOOST_CHECK( checker.check_witness_opportunity_to_approve( witness, *proposal_idx.begin() ) );

   db.apply_operation( context, op );

   BOOST_CHECK( proposal_idx.size() == 1 );
   BOOST_CHECK_EQUAL( proposal_idx.begin()->available_active_approvals.size(), 1 );
   BOOST_CHECK( !checker.check_witness_opportunity_to_approve( witness, *proposal_idx.begin() ) );
}

BOOST_AUTO_TEST_CASE( not_active_wit_try_to_approve_btc_send_test )
{
   ACTOR(nathan);
   upgrade_to_lifetime_member( nathan_id );
   trx.clear();
   witness_id_type nathan_witness_id = create_witness( nathan_id, nathan_private_key ).id;

   const auto& witnesses = db.get_global_properties().active_witnesses;
   BOOST_CHECK( std::find(witnesses.begin(), witnesses.end(), nathan_witness_id) == witnesses.end() );

   const auto& witness_idx = db.get_index_type<graphene::chain::witness_index>().indices().get<graphene::chain::by_id>();
   auto itr = witness_idx.find( nathan_witness_id );
   BOOST_CHECK( itr != witness_idx.end() );

   sidechain_proposal_checker checker( db );
   const auto& proposal_idx = db.get_index_type<graphene::chain::proposal_index>().indices().get< graphene::chain::by_id >();
   auto propose = db.create<proposal_object>( [&]( proposal_object& obj ){
      obj.expiration_time =  db.head_block_time() + fc::days(1);
      obj.review_period_time = db.head_block_time() + fc::days(1);
   } );

   BOOST_CHECK( !checker.check_witness_opportunity_to_approve( *itr, *proposal_idx.begin() ) );
}

BOOST_AUTO_TEST_CASE( not_account_auths_wit_try_to_approve_btc_send_test )
{
   ACTOR(nathan);
   upgrade_to_lifetime_member(nathan_id);
   trx.clear();
   witness_id_type nathan_witness_id = create_witness(nathan_id, nathan_private_key).id;
   // Give nathan some voting stake
   transfer(committee_account, nathan_id, asset(10000000));
   generate_block();
   test::set_expiration( db, trx );

   {
      account_update_operation op;
      op.account = nathan_id;
      op.new_options = nathan_id(db).options;
      op.new_options->votes.insert(nathan_witness_id(db).vote_id);
      op.new_options->num_witness = std::count_if(op.new_options->votes.begin(), op.new_options->votes.end(),
                                                  [](vote_id_type id) { return id.type() == vote_id_type::witness; });
      op.new_options->num_committee = std::count_if(op.new_options->votes.begin(), op.new_options->votes.end(),
                                                    [](vote_id_type id) { return id.type() == vote_id_type::committee; });
      trx.operations.push_back(op);
      sign( trx, nathan_private_key );
      PUSH_TX( db, trx );
      trx.clear();
   }

   generate_blocks(db.get_dynamic_global_properties().next_maintenance_time);
   const auto& witnesses = db.get_global_properties().active_witnesses;
   auto itr_ = std::find(witnesses.begin(), witnesses.end(), nathan_witness_id);
   BOOST_CHECK(itr_ != witnesses.end());

   const auto& witness_idx = db.get_index_type<graphene::chain::witness_index>().indices().get<graphene::chain::by_id>();
   auto itr = witness_idx.find( nathan_witness_id );
   BOOST_CHECK( itr != witness_idx.end() );

   sidechain_proposal_checker checker( db );
   const auto& proposal_idx = db.get_index_type<graphene::chain::proposal_index>().indices().get< graphene::chain::by_id >();
   auto propose = db.create<proposal_object>( [&]( proposal_object& obj ){
      obj.expiration_time =  db.head_block_time() + fc::days(1);
      obj.review_period_time = db.head_block_time() + fc::days(1);
   } );

   BOOST_CHECK( !checker.check_witness_opportunity_to_approve( *itr, *proposal_idx.begin() ) );
}

void create_missing_bto( graphene::chain::database& db, uint32_t amount )
{
   BOOST_REQUIRE( amount < 9 );
   while( amount-- > 0 ) {
      std::set<fc::sha256> vins { fc::sha256( std::string( 64,'1' + amount ) ), fc::sha256( std::string( 64,'2' + amount ) ) };
      sidechain::bitcoin_transaction_confirmations  btc_trx_conf ( fc::sha256( std::string( 64,'1' + amount ) ), vins );
      btc_trx_conf.missing = true;
      db.bitcoin_confirmations.insert( btc_trx_conf );

      db.create<graphene::chain::bitcoin_transaction_object>( [&]( graphene::chain::bitcoin_transaction_object& obj ){
         obj.transaction_id = fc::sha256( std::string( 64,'1' + amount ) );
      } );
   }
}

std::vector< revert_trx_info > get_transactions_info( graphene::chain::database& db, uint32_t amount )
{
   using iter_by_missing = btc_tx_confirmations_index::index<by_missing_first>::type::iterator;
   std::vector< revert_trx_info > transactions_info;
   const auto& btc_trx_idx = db.get_index_type<graphene::chain::bitcoin_transaction_index>().indices().get<graphene::chain::by_transaction_id>();
   BOOST_CHECK_EQUAL( btc_trx_idx.size() , amount );

   db.bitcoin_confirmations.safe_for<by_missing_first>([&]( iter_by_missing itr_b, iter_by_missing itr_e ){
      for(auto iter = itr_b; iter != itr_e; iter++) {
         if( !iter->missing ) return;
         const auto& btc_tx = btc_trx_idx.find( iter->transaction_id );
         if( btc_tx == btc_trx_idx.end() ) continue;
         transactions_info.push_back( revert_trx_info( iter->transaction_id, iter->valid_vins ) ); 
      }
   });

   return transactions_info;
}

BOOST_AUTO_TEST_CASE( bitcoin_transaction_revert_operation_checker_test )
{
   using namespace graphene::chain;
   const uint32_t amount = 5;

   create_missing_bto( db, amount );
   sidechain_proposal_checker checker( db );

   bitcoin_transaction_revert_operation op;

   std::vector< revert_trx_info > transactions_info = get_transactions_info( db, amount );

   op.transactions_info = transactions_info;

   BOOST_CHECK_EQUAL( transactions_info.size(), amount );   
   BOOST_CHECK( checker.check_bitcoin_transaction_revert_operation( op ) );
}

BOOST_AUTO_TEST_CASE( bitcoin_transaction_revert_operation_checker_failed_test )
{
   using namespace graphene::chain;
   const uint32_t amount = 5;

   create_missing_bto( db, amount );

   std::set<fc::sha256> vins { fc::sha256( std::string( 64,'1' + 6 ) ), fc::sha256( std::string( 64,'1' + 8 ) ) };
   fc::sha256 trx_id( std::string( 64,'1' + 8 ) );
   sidechain::bitcoin_transaction_confirmations  btc_trx_conf ( trx_id, vins );
   db.bitcoin_confirmations.insert( btc_trx_conf );
   db.create<graphene::chain::bitcoin_transaction_object>( [&]( graphene::chain::bitcoin_transaction_object& obj ){
      obj.transaction_id = fc::sha256( std::string( 64,'1' + 7 ) );
   } );

   sidechain_proposal_checker checker( db );

   bitcoin_transaction_revert_operation op;
   std::vector< revert_trx_info > transactions_info = get_transactions_info( db, amount + 1 );

   op.transactions_info = transactions_info;
   op.transactions_info.push_back( revert_trx_info( trx_id, vins) );

   BOOST_CHECK_EQUAL( op.transactions_info.size(), amount + 1 );   
   BOOST_CHECK( !checker.check_bitcoin_transaction_revert_operation( op ) );
}

BOOST_AUTO_TEST_CASE( no_btc_trx_to_revert_test )
{
   sidechain_proposal_checker checker( db );

   bitcoin_transaction_revert_operation op;  
   BOOST_CHECK( !checker.check_bitcoin_transaction_revert_operation( op ) );
}

BOOST_AUTO_TEST_SUITE_END()
