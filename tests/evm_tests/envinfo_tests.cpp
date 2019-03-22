#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"
#include <graphene/chain/result_contract_object.hpp>
#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/witness_object.hpp>

#include <pp_state.hpp>
#include <evm_result.hpp>


using namespace graphene::chain;
using namespace graphene::chain::test;

/*
contract BlockProperies {

    function timestamp() public returns (uint) {
        return block.timestamp;
    }
    
    function number() public returns (uint) {
        return block.number;
    }
    
    function gaslimit() public returns (uint) {
        return block.gaslimit;
    }
    
    function coinbase() public returns (address) {
        return block.coinbase;
    }
    
    function blockhash(uint blockNumber) public returns (bytes32) {
        return block.blockhash(blockNumber);
    }
    
}
*/

std::string BlockProperiesCode = "608060405234801561001057600080fd5b506101ea806100206000396000f30060806040526004361061006d576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff1680632a722839146100725780638381f58a1461009d57806384af0cae146100c8578063a6ae0aac14610111578063b80777ea14610168575b600080fd5b34801561007e57600080fd5b50610087610193565b6040518082815260200191505060405180910390f35b3480156100a957600080fd5b506100b261019b565b6040518082815260200191505060405180910390f35b3480156100d457600080fd5b506100f3600480360381019080803590602001909291905050506101a3565b60405180826000191660001916815260200191505060405180910390f35b34801561011d57600080fd5b506101266101ae565b604051808273ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200191505060405180910390f35b34801561017457600080fd5b5061017d6101b6565b6040518082815260200191505060405180910390f35b600045905090565b600043905090565b600081409050919050565b600041905090565b6000429050905600a165627a7a72305820c234432da66de8ed1e35b6dcbb151460c48e5aedf3c2c5bf1197204174b8a70c0029";

void create_block_prop_contract( database_fixture& df )
{
   df.generate_block();
   df.transfer( account_id_type(0), account_id_type(5), asset( 1000000000, asset_id_type() ) );
   signed_transaction trx;
   set_expiration( df.db, trx );

   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type(5);
   contract_op.fee = asset(0, asset_id_type());
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), asset_id_type(0), 0, asset_id_type(0), 1, 2000000, BlockProperiesCode });
   trx.operations.push_back( contract_op );

   PUSH_TX( df.db, trx, ~0 );
   trx.clear();
   df.generate_block();
}

dev::bytes execute_block_prop_contract( database_fixture& df, std::string code, int result_id ) 
{
   signed_transaction trx;
   set_expiration( df.db, trx );

   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type(5);
   contract_op.fee = asset(0, asset_id_type());
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), asset_id_type(0), 0, asset_id_type(0), 1, 2000000, code });
   trx.operations.push_back( contract_op );

   PUSH_TX( df.db, trx, ~0 );
   trx.clear();

   df.generate_block();

   auto raw_res = df.db.db_res.get_results( std::string( (object_id_type)result_contract_id_type(result_id) ) );

   auto res = fc::raw::unpack< vms::evm::evm_result >( *raw_res );
   return res.exec_result.output;
}

BOOST_FIXTURE_TEST_SUITE( envinfo_tests, database_fixture )

BOOST_AUTO_TEST_CASE( timestamp_test )
{
   dev::bytes result;

   auto check_timestamp = [&]( const dev::bytes output, database& db )
   { 
      auto temp = output;
      std::reverse( std::begin(temp), std::end(temp) );
      temp.resize(4);
      auto out_time = fc::raw::unpack<time_point_sec>( temp );
      auto block = db.fetch_block_by_number( db.head_block_num() );

      BOOST_CHECK( out_time == block->timestamp );
   };

   create_block_prop_contract( *this );
   result = execute_block_prop_contract( *this, "b80777ea", 1 );
   check_timestamp( result, db );

   generate_blocks(10);
   result = execute_block_prop_contract( *this, "b80777ea", 2);
   check_timestamp( result, db );
}

BOOST_AUTO_TEST_CASE( block_num_test )
{
   dev::bytes result;

   auto check_block_num = [&]( const dev::bytes output, database& db )
   { 
      auto temp = output;
      std::reverse(std::begin(temp), std::end(temp));
      temp.resize(4);
      auto out_block_num = fc::raw::unpack<uint32_t>( temp );

      BOOST_CHECK( out_block_num == db.head_block_num() );
   };

   create_block_prop_contract( *this );
   result = execute_block_prop_contract( *this, "8381f58a", 1);
   check_block_num( result, db );

   generate_blocks(10);
   result = execute_block_prop_contract( *this, "8381f58a", 2);
   check_block_num( result, db );
}

BOOST_AUTO_TEST_CASE( coinbase_test )
{
   dev::bytes result;

   auto check_coinbase = [&](const dev::bytes output, database& db )
   { 
      auto temp = output;
      temp.erase( temp.begin(), temp.begin() + 12 );

      auto out_id = vms::base::address_to_id( fc::raw::unpack<dev::h160>( temp ) ).second;
      auto block = db.fetch_block_by_number( db.head_block_num() );

      auto& index = db.get_index_type< witness_index >().indices().get< by_id >();
      auto itr = index.find( block->witness );

      BOOST_CHECK( out_id == itr->witness_account.instance );
   };

   create_block_prop_contract( *this );
   result = execute_block_prop_contract( *this, "a6ae0aac", 1);
   check_coinbase( result, db );

   generate_blocks(10);
   result = execute_block_prop_contract( *this, "a6ae0aac", 2);
   check_coinbase( result, db );
}

BOOST_AUTO_TEST_CASE( block_hash_test )
{
   dev::bytes result;

   auto check_block_hash = [&](const dev::bytes output, database& db, uint32_t num )
   { 
      auto temp = output;
      temp.erase( temp.begin(), temp.begin() + 12 );

      auto id = dev::toHex( temp );
      auto block = db.fetch_block_by_number( num );
      BOOST_CHECK( fc::ripemd160(id) == block->id() );

   };

   create_block_prop_contract( *this );
   result = execute_block_prop_contract( *this, "84af0cae0000000000000000000000000000000000000000000000000000000000000002", 1 );
   check_block_hash( result, db, 2 );

   generate_blocks(15);
   result = execute_block_prop_contract( *this, "84af0cae000000000000000000000000000000000000000000000000000000000000000a", 2 );
   check_block_hash( result, db, 10 );

   generate_blocks(300);
   result = execute_block_prop_contract( *this, "84af0cae000000000000000000000000000000000000000000000000000000000000006a", 3 );
   check_block_hash( result, db, 106 );

   result = execute_block_prop_contract( *this, "84af0cae000000000000000000000000000000000000000000000000000000000000000a", 4 );
   BOOST_CHECK( dev::h256( result ) == dev::h256() );

   result = execute_block_prop_contract( *this, "84af0cae000000000000000000000000000000000000000000000000000000000000030a", 5 );
   BOOST_CHECK( dev::h256( result ) == dev::h256() );
}

BOOST_AUTO_TEST_SUITE_END()