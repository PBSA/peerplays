#include <boost/test/unit_test.hpp>
#include "../common/database_fixture.hpp"

#include <graphene/chain/contracts_results_in_block_object.hpp>

using namespace graphene::chain::test;

/*
    contract solidityPayable {
        function() payable {}
    }
*/

std::string solidityPayableCode = "60606040523415600b57fe5b5b60398060196000396000f30060606040525b600b5b5b565b0000a165627a7a72305820b78de2de40744ce26d408c09eb352691ee05ca2570a58eb869764c9fceb274f30029";

BOOST_FIXTURE_TEST_SUITE( roll_back_db_tests, database_fixture )

BOOST_AUTO_TEST_CASE( sequential_rollback_test ) {
   transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
   
   signed_transaction trx;
   set_expiration( db, trx );

   contract_operation contract_op;
   contract_op.vm_type = vms::base::vm_types::EVM;
   contract_op.registrar = account_id_type(5);
   contract_op.fee = asset(0, asset_id_type());
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(0), 0, asset_id_type(0), 0, 4000000, solidityPayableCode });

   for( size_t i = 0; i < 5; i++ ) {
      trx.operations.push_back( contract_op );
   }

   auto init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash(string("null_key")) );

   PUSH_TX( db, trx, ~0 );
   const auto block1 = generate_block();
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 5 );

   PUSH_TX( db, trx, ~0 );
   const auto block2 = generate_block();
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 10 );

   PUSH_TX( db, trx, ~0 );
   const auto block3 = generate_block();
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 15 );

   db._executor->roll_back_db( block2.block_num() );
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 10 );
   db._executor->roll_back_db( block1.block_num() );
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 5 );
}

BOOST_AUTO_TEST_CASE( not_sequential_rollback_test ) {
   transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
   
   signed_transaction trx;
   contract_operation contract_op;
   contract_op.vm_type = vms::base::vm_types::EVM;
   contract_op.registrar = account_id_type(5);
   contract_op.fee = asset(0, asset_id_type());
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(0), 0, asset_id_type(0), 0, 4000000, solidityPayableCode });
   for( size_t i = 0; i < 5; i++ ) {
      trx.operations.push_back( contract_op );
   }

   auto init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash(string("null_key")) );
   const auto start_block = db.head_block_num();

   set_expiration( db, trx );
   PUSH_TX( db, trx, ~0 );
   const auto block1 = generate_block();
   generate_blocks( 10 );
   const auto interval1 = start_block + 10;

   set_expiration( db, trx );
   PUSH_TX( db, trx, ~0 );
   const auto block2 = generate_block();
   generate_blocks( 10 );
   const auto interval2 = interval1 + 10;

   db._executor->roll_back_db( block2.block_num() );
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 10 );
   db._executor->roll_back_db( interval2 - 3 );
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 10 );
   db._executor->roll_back_db( block1.block_num() );
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 5 );
   db._executor->roll_back_db( interval1 - 3 );
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 5 );
   db._executor->roll_back_db( 1 );
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 0 );
}

BOOST_AUTO_TEST_CASE( switch_to_late_block_test ) {
   transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
   
   signed_transaction trx;
   set_expiration( db, trx );

   contract_operation contract_op;
   contract_op.vm_type = vms::base::vm_types::EVM;
   contract_op.registrar = account_id_type(5);
   contract_op.fee = asset(0, asset_id_type());
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(0), 0, asset_id_type(0), 0, 4000000, solidityPayableCode });

   for( size_t i = 0; i < 5; i++ ) {
      trx.operations.push_back( contract_op );
   }

   auto init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash(string("null_key")) );

   PUSH_TX( db, trx, ~0 );
   const auto block1 = generate_block();

   PUSH_TX( db, trx, ~0 );
   const auto block2 = generate_block();

   PUSH_TX( db, trx, ~0 );
   const auto block3 = generate_block();

   db._executor->roll_back_db( 1 );
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 0 );
   db._executor->roll_back_db( block1.block_num() );
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 5 );
   db._executor->roll_back_db( block2.block_num() );
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 10 );
   db._executor->roll_back_db( block3.block_num() );
   BOOST_CHECK( db._executor->get_contracts( vms::base::vm_types::EVM ).size() == 15 );
}

BOOST_AUTO_TEST_SUITE_END()
