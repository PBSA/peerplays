#include <boost/test/unit_test.hpp>
#include "../common/database_fixture.hpp"

#include <graphene/chain/contracts_results_in_block_object.hpp>
#include <graphene/chain/result_contract_object.hpp>
#include <evm_adapter.hpp>
#include <evm_result.hpp>

using namespace graphene::chain::test;

std::vector<contract_id_type> ids { contract_id_type(0),
                                    contract_id_type(1),
                                    contract_id_type(2),
                                    contract_id_type(3),
                                    contract_id_type(4),
                                    contract_id_type(5),
                                    contract_id_type(6),
                                    contract_id_type(7),
                                    contract_id_type(8),
                                    contract_id_type(9),
                                    contract_id_type(10),
                                    contract_id_type(11),
                                    contract_id_type(12),
                                    contract_id_type(13),
                                    contract_id_type(14) };

/*
    contract solidityPayable {
        function() payable {}
    }
*/

std::string solidityPayableCode = "60606040523415600b57fe5b5b60398060196000396000f30060606040525b600b5b5b565b0000a165627a7a72305820b78de2de40744ce26d408c09eb352691ee05ca2570a58eb869764c9fceb274f30029";

BOOST_FIXTURE_TEST_SUITE( roll_back_db_tests, database_fixture )

void create_result( database& db, uint64_t block_num, std::string hash ) {
   std::vector<result_contract_id_type> results_contracts;
   for( size_t i = 0; i < 5; i++ ) {
      result_contract_id_type id = db.create<result_contract_object>( [&]( result_contract_object& obj ) {
         obj.vm_type = vm_types::EVM;
      }).id;

      vms::evm::evm_result evm_res;
      evm_res.state_root = hash;
      db.db_res.add_result( std::string( object_id_type( id ) ), fc::raw::unsigned_pack( evm_res ) );

      results_contracts.push_back( id );
   }

   db.create<contracts_results_in_block_object>( [&]( contracts_results_in_block_object& obj ) {
      obj.block_num  = block_num;
      obj.results_id = results_contracts;
   } );
}

std::map<contract_id_type, vms::evm::evm_account_info> get_contracts( database& db, const vector<contract_id_type>& contract_ids)
{
   std::map<contract_id_type, vms::evm::evm_account_info> results;
   const auto& raw_contracts = db._executor->get_contracts( vm_types::EVM, contract_ids );
   for( const auto& raw_contract : raw_contracts ) {
      if( !raw_contract.second.empty() ) {
         const auto& id = contract_id_type( raw_contract.first );
         results.insert( std::make_pair( id, fc::raw::unpack< vms::evm::evm_account_info >( raw_contract.second ) ) );
      }
   }
   return results;
}

BOOST_AUTO_TEST_CASE( sequential_rollback_test )
{
   transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
   
   signed_transaction trx;
   set_expiration( db, trx );

   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type(5);
   contract_op.fee = asset(0, asset_id_type());
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 0, 4000000, solidityPayableCode });

   for( size_t i = 0; i < 5; i++ ) {
      trx.operations.push_back( contract_op );
   }

   auto init_account_priv_key = fc::ecc::private_key::regenerate( fc::sha256::hash(string("null_key")) );

   PUSH_TX( db, trx, ~0 );
   const auto block1 = generate_block();
   BOOST_CHECK( get_contracts( db, ids ).size() == 5 );

   PUSH_TX( db, trx, ~0 );
   const auto block2 = generate_block();
   BOOST_CHECK( get_contracts( db, ids ).size() == 10 );

   PUSH_TX( db, trx, ~0 );
   const auto block3 = generate_block();
   BOOST_CHECK( get_contracts( db, ids ).size() == 15 );

   db._executor->roll_back_db( block2.block_num() );
   BOOST_CHECK( get_contracts( db, ids ).size() == 10 );
   db._executor->roll_back_db( block1.block_num() );
   BOOST_CHECK( get_contracts( db, ids ).size() == 5 );
}

BOOST_AUTO_TEST_CASE( not_sequential_rollback_test )
{
   transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
   
   signed_transaction trx;
   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type(5);
   contract_op.fee = asset(0, asset_id_type());
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 0, 4000000, solidityPayableCode });
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
   BOOST_CHECK( get_contracts( db, ids ).size() == 10 );
   db._executor->roll_back_db( interval2 - 3 );
   BOOST_CHECK( get_contracts( db, ids ).size() == 10 );
   db._executor->roll_back_db( block1.block_num() );
   BOOST_CHECK( get_contracts( db, ids ).size() == 5 );
   db._executor->roll_back_db( interval1 - 3 );
   BOOST_CHECK( get_contracts( db, ids ).size() == 5 );
   db._executor->roll_back_db( 1 );
   BOOST_CHECK( get_contracts( db, ids ).size() == 0 );
}

BOOST_AUTO_TEST_CASE( switch_to_late_block_test )
{
   transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));
   
   signed_transaction trx;
   set_expiration( db, trx );

   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type(5);
   contract_op.fee = asset(0, asset_id_type());
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 0, 4000000, solidityPayableCode });

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
   BOOST_CHECK( get_contracts( db, ids ).size() == 0 );
   db._executor->roll_back_db( block1.block_num() );
   BOOST_CHECK( get_contracts( db, ids ).size() == 5 );
   db._executor->roll_back_db( block2.block_num() );
   BOOST_CHECK( get_contracts( db, ids ).size() == 10 );
   db._executor->roll_back_db( block3.block_num() );
   BOOST_CHECK( get_contracts( db, ids ).size() == 15 );
}

BOOST_AUTO_TEST_CASE( no_results_test )
{
   vms::evm::evm_adapter adapter(db);

   BOOST_CHECK( !adapter.get_last_valid_state_root(0) );

   BOOST_CHECK( !adapter.get_last_valid_state_root(13) );
}

BOOST_AUTO_TEST_CASE( border_conditions_test )
{
   vms::evm::evm_adapter adapter(db);

   std::string hash1(64, 'a');
   create_result( db, 5, hash1 );
   BOOST_CHECK( !adapter.get_last_valid_state_root( 0 ) );
   BOOST_CHECK( *adapter.get_last_valid_state_root( 5 ) == fc::sha256( hash1 ) );

   std::string hash2(64, 'b');
   create_result( db, 0, hash2 );
   BOOST_CHECK( *adapter.get_last_valid_state_root( 0 ) == fc::sha256( hash2 ) );
   BOOST_CHECK( *adapter.get_last_valid_state_root( 13 ) == fc::sha256( hash1 ) );

   std::string hash3(64, 'c');
   create_result( db, 13, hash3 );
   BOOST_CHECK( *adapter.get_last_valid_state_root(13) == fc::sha256( hash3 ) );
   BOOST_CHECK( *adapter.get_last_valid_state_root(130) == fc::sha256( hash3 ) );
   BOOST_CHECK( *adapter.get_last_valid_state_root(12) == fc::sha256( hash1 ) );
}

BOOST_AUTO_TEST_SUITE_END()
