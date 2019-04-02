#include <boost/test/unit_test.hpp>
#include "../common/database_fixture.hpp"

#include <graphene/chain/contract_object.hpp>
#include <evm_result.hpp>

using namespace graphene::chain::test;

/*
   contract secondContract {
      
      function() payable {
         assert(false);
      }
   }

   contract TestRevertAssert {
      
      secondContract _secondContract;
      
      function TestRevertAssert() {
         _secondContract = new secondContract();
      }
      
      function createWithRevert() {
         _secondContract = new secondContract();
         revert();
      }
      
      function throwAssert() {
         assert(false);
      }
      
      function throwRevert() {
         revert();
      }
      
      function sendWithRevert() payable {
         revert();
      }
      
      function sendWithAssert() payable {
         assert(false);
      }

      function callSendContractWithTransfer() payable {
         _secondContract.transfer(msg.value);
      }

      function sendFundsToSecondContract() payable {
         _secondContract.send(msg.value);
      }
   }
*/

std::string contractCode = "608060405234801561001057600080fd5b5061001961007a565b604051809103906000f080158015610035573d6000803e3d6000fd5b506000806101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff160217905550610089565b60405160588061036e83390190565b6102d6806100986000396000f300608060405260043610610083576000357c0100000000000000000000000000000000000000000000000000000000900463ffffffff1680631e5c0503146100885780634b750976146100925780634d2d612d146100a95780638da60226146100c0578063e7731596146100ca578063f08f6077146100d4578063fb5469db146100eb575b600080fd5b6100906100f5565b005b34801561009e57600080fd5b506100a7610101565b005b3480156100b557600080fd5b506100be61010d565b005b6100c8610112565b005b6100d261016b565b005b3480156100e057600080fd5b506100e9610170565b005b6100f36101d9565b005b600015156100ff57fe5b565b6000151561010b57fe5b565b600080fd5b6000809054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166108fc349081150290604051600060405180830381858888f1935050505050565b600080fd5b610178610243565b604051809103906000f080158015610194573d6000803e3d6000fd5b506000806101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff160217905550600080fd5b6000809054906101000a900473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff166108fc349081150290604051600060405180830381858888f19350505050158015610240573d6000803e3d6000fd5b50565b6040516058806102538339019056006080604052348015600f57600080fd5b50603b80601d6000396000f300608060405260001515600d57fe5b0000a165627a7a7230582047d5269d9537803d42ee8e79f6104d7bb28b835e51e6b8c9ca17a6c3a5a8c0420029a165627a7a72305820cbf8058a6b3af5641be856ed67ea45e9fe18191e8c7b9f4c5f44188a1924687700296080604052348015600f57600080fd5b50603b80601d6000396000f300608060405260001515600d57fe5b0000a165627a7a7230582047d5269d9537803d42ee8e79f6104d7bb28b835e51e6b8c9ca17a6c3a5a8c0420029";

/*
pragma solidity >=0.4.22 <0.6.0;

   contract testcontract {
      constructor() payable public {}
      function tr(address payable addr, uint sum, uint idAsset) public {
         addr.transferasset(sum, idAsset);
      }
      function() external payable {}
   }
*/

std::string testcontractCode = "6080604052610114806100136000396000f3fe6080604052600436106039576000357c010000000000000000000000000000000000000000000000000000000090048063955c806d14603b575b005b348015604657600080fd5b50609a60048036036060811015605b57600080fd5b81019080803573ffffffffffffffffffffffffffffffffffffffff1690602001909291908035906020019092919080359060200190929190505050609c565b005b808373ffffffffffffffffffffffffffffffffffffffff166108fc849081150290604051600060405180830381858888bc935050505015801560e2573d6000803e3d6000fd5b5050505056fea165627a7a723058203a80deeaa54224fcd7aceb94125cf87c17b73dd8599a61a1449c1a2054e39ba40029";

/*
pragma solidity >=0.4.22 <0.6.0;
   contract testcontract2 {
      constructor() payable public {}
      function() external payable { assert(false); }
   }
*/

std::string testcontract2Code = "608060405260398060116000396000f3fe60806040526000600b57fe5b00fea165627a7a72305820fd1e920b392f0a75c08b7c4d8dec2fbe7e0147ba0a408da9a8abba16db9a32de0029";

void create_contract( database_fixture& df, std::string code )
{
   df.generate_block();
   df.transfer( account_id_type(0), account_id_type(5), asset( 1000000000, asset_id_type() ) );
   signed_transaction trx;
   set_expiration( df.db, trx );

   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type(5);
   contract_op.fee = asset(0, asset_id_type());
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(0), 0, asset_id_type(0), 1, 2000000, code });
   trx.operations.push_back( contract_op );

   PUSH_TX( df.db, trx, ~0 );
   trx.clear();
   df.generate_block();
}

vms::evm::evm_result call_contract( database_fixture& df, std::string code, uint64_t value, int result_id, uint64_t id_asset = 0 ) 
{
   signed_transaction trx;
   set_expiration( df.db, trx );

   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type(5);
   contract_op.fee = asset(0, asset_id_type());
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type(0), std::set<uint64_t>(), asset_id_type(id_asset), value, asset_id_type(0), 1, 2000000, code });
   trx.operations.push_back( contract_op );

   PUSH_TX( df.db, trx, ~0 );
   trx.clear();

   df.generate_block();

   auto raw_res = df.db.db_res.get_results( std::string( (object_id_type)result_contract_id_type(result_id) ) ); 

   auto res = fc::raw::unpack< vms::evm::evm_result >( *raw_res );
   return res;
}

BOOST_FIXTURE_TEST_SUITE( rollback_state_tests, database_fixture )

BOOST_AUTO_TEST_CASE( charge_all_gas_when_vm_exception_occure )
{
   create_contract( *this, contractCode );
   auto start_balance = get_balance( account_id_type(5) , asset_id_type() );
   auto gasUsed = 2000000;
   auto result = call_contract( *this, "4b750976", 0, 1 );

   BOOST_CHECK( result.exec_result.excepted == dev::eth::TransactionException::BadInstruction );
   BOOST_CHECK( result.exec_result.gasUsed == dev::u256(gasUsed) );
   BOOST_CHECK( get_balance( account_id_type(5) , asset_id_type() ) + gasUsed == start_balance );
}

BOOST_AUTO_TEST_CASE( charge_part_of_gas_when_revert_occure )
{
   create_contract( *this, contractCode );
   auto start_balance = get_balance( account_id_type(5) , asset_id_type() );
   auto gasUsed = 21446;
   auto result = call_contract( *this, "4d2d612d", 0, 1 );

   BOOST_CHECK( result.exec_result.excepted == dev::eth::TransactionException::RevertInstruction );
   BOOST_CHECK( result.exec_result.gasUsed == dev::u256(gasUsed) );
   BOOST_CHECK( get_balance( account_id_type(5) , asset_id_type() ) + gasUsed == start_balance );
}

BOOST_AUTO_TEST_CASE( return_back_value_when_vm_exception_occure )
{
   create_contract( *this, contractCode );
   auto start_balance = get_balance( account_id_type(5) , asset_id_type() );
   auto gasUsed = 2000000;
   auto value = 10000000;
   auto result = call_contract( *this, "1e5c0503", value, 1 );

   BOOST_CHECK( get_balance( account_id_type(5) , asset_id_type() ) + gasUsed == start_balance );
   BOOST_CHECK( db.get_balance( contract_id_type(0), asset_id_type() ) == asset(0, asset_id_type()) );
}

BOOST_AUTO_TEST_CASE( return_back_value_when_revert_occure )
{
   create_contract( *this, contractCode );
   auto start_balance = get_balance( account_id_type(5) , asset_id_type() );
   auto gasUsed = 21466;
   auto value = 10000000;
   auto result = call_contract( *this, "e7731596", value, 1 );

   BOOST_CHECK( get_balance( account_id_type(5) , asset_id_type() ) + gasUsed == start_balance );
   BOOST_CHECK( db.get_balance( contract_id_type(0), asset_id_type() ) == asset(0, asset_id_type()) );
}

BOOST_AUTO_TEST_CASE( return_back_value_when_transfer_to_second_contract )
{
   create_contract( *this, contractCode );
   auto start_balance = get_balance( account_id_type(5) , asset_id_type() );
   auto gasUsed = 31556;
   auto value = 10000000;
   auto result = call_contract( *this, "fb5469db", value, 1 );

   BOOST_CHECK( result.exec_result.excepted == dev::eth::TransactionException::RevertInstruction );
   BOOST_CHECK( result.exec_result.gasUsed == dev::u256(gasUsed) );
   BOOST_CHECK( get_balance( account_id_type(5) , asset_id_type() ) + gasUsed == start_balance );
   BOOST_CHECK( db.get_balance( contract_id_type(0), asset_id_type() ) == asset(0, asset_id_type()) );
   BOOST_CHECK( db.get_balance( contract_id_type(1), asset_id_type() ) == asset(0, asset_id_type()) );
}

BOOST_AUTO_TEST_CASE( not_return_back_value_when_send_to_second_contract )
{
   create_contract( *this, contractCode );
   auto start_balance = get_balance( account_id_type(5) , asset_id_type() );
   auto gasUsed = 31463;
   auto value = 10000000;
   auto result = call_contract( *this, "8da60226", value, 1 );

   BOOST_CHECK( result.exec_result.excepted == dev::eth::TransactionException::None );
   BOOST_CHECK( result.exec_result.gasUsed == dev::u256(gasUsed) );
   BOOST_CHECK( get_balance( account_id_type(5) , asset_id_type() ) + gasUsed + value == start_balance );
   BOOST_CHECK( db.get_balance( contract_id_type(0), asset_id_type() ) == asset(value, asset_id_type()) );
   BOOST_CHECK( db.get_balance( contract_id_type(1), asset_id_type() ) == asset(0, asset_id_type()) );
}

BOOST_AUTO_TEST_CASE( rollback_create_contract_object_when_revert_occure )
{
   const auto& contract_idx = db.get_index_type<contract_index>().indices().get<by_id>();
   BOOST_CHECK( contract_idx.size() == 0 );
   create_contract( *this, contractCode );
   BOOST_CHECK( contract_idx.size() == 2 );
   auto result = call_contract( *this, "8da60226", 0, 1 );

   BOOST_CHECK( contract_idx.size() == 2 );
}

BOOST_AUTO_TEST_CASE( rollback_transfer_stack_when_revert_occure )
{
   create_contract( *this, contractCode );
   auto size = get_operation_history( contract_id_type( 0 ) ).size();
   BOOST_CHECK( size == 1 );

   auto result = call_contract( *this, "8da60226", 10000000, 1 );
   size = get_operation_history( contract_id_type( 0 ) ).size();
   BOOST_CHECK( size == 1 );
}

BOOST_AUTO_TEST_CASE( rollback_diff_assets_transfer )
{
   asset_object temp_asset = create_user_issued_asset( "ETB" );
   issue_uia( account_id_type( 5 ), temp_asset.amount( 10000000 ) );
   auto id_new_asset = static_cast<uint64_t>( temp_asset.get_id().instance );

   create_contract( *this, testcontractCode );
   create_contract( *this, testcontract2Code );
   call_contract( *this, "00", 100000, 2, id_new_asset );

   auto raw_res = db.db_res.get_results( std::string( (object_id_type)result_contract_id_type(1) ) ); 
   auto res = fc::raw::unpack< vms::evm::evm_result >( *raw_res );

   std::string call = "955c806d" + std::string( "000000000000000000000000" ) + res.exec_result.newAddress.hex() + dev::h256(dev::u256(10000)).hex() + dev::h256(dev::u256(1)).hex();
   auto call_res = call_contract( *this, call, 0, 3 );

   BOOST_CHECK( call_res.exec_result.excepted == dev::eth::TransactionException::RevertInstruction );
   BOOST_CHECK( db.get_balance( contract_id_type(0), asset_id_type(id_new_asset) ) == asset(100000, asset_id_type(id_new_asset)) );
}

BOOST_AUTO_TEST_SUITE_END()
