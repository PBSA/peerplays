#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

#include <evm_result.hpp>

BOOST_FIXTURE_TEST_SUITE( certain_assets_for_contract_tests, database_fixture )

/*
   pragma solidity >=0.4.22 <0.6.0;
   contract solidity {
      constructor() payable public {}
      function tr(address payable addr, uint sum, uint idAsset) public {
         addr.transferasset(sum, idAsset);
      }
      function() external payable {}
   }
*/
auto contract_transfer_code = "6080604052610114806100136000396000f3fe6080604052600436106039576000357c010000000000000000000000000000000000000000000000000000000090048063955c806d14603b575b005b348015604657600080fd5b50609a60048036036060811015605b57600080fd5b81019080803573ffffffffffffffffffffffffffffffffffffffff1690602001909291908035906020019092919080359060200190929190505050609c565b005b808373ffffffffffffffffffffffffffffffffffffffff166108fc849081150290604051600060405180830381858888bc935050505015801560e2573d6000803e3d6000fd5b5050505056fea165627a7a72305820c24b404a89129b78b68e9b4071ee733046a1f05fa6b4743abaf39e53dc3ad27e0029";

/*
   pragma solidity >=0.4.22 <0.6.0;
   contract solidityCall {
      constructor() payable public {}
      function se( address payable callAddr, address payable addr, uint sum, uint idAsset ) payable public {
         (bool success,) = callAddr.call( abi.encodeWithSignature( "tr(address,uint256,uint256)",addr,sum,idAsset ) );
         require(success);
      }
      function() external payable {}
   }
*/
auto contract_call_code = "6080604052610273806100136000396000f3fe60806040526004361061003b576000357c010000000000000000000000000000000000000000000000000000000090048063e354bcee1461003d575b005b6100b36004803603608081101561005357600080fd5b81019080803573ffffffffffffffffffffffffffffffffffffffff169060200190929190803573ffffffffffffffffffffffffffffffffffffffff16906020019092919080359060200190929190803590602001909291905050506100b5565b005b60008473ffffffffffffffffffffffffffffffffffffffff16848484604051602401808473ffffffffffffffffffffffffffffffffffffffff1673ffffffffffffffffffffffffffffffffffffffff16815260200183815260200182815260200193505050506040516020818303038152906040527f955c806d000000000000000000000000000000000000000000000000000000007bffffffffffffffffffffffffffffffffffffffffffffffffffffffff19166020820180517bffffffffffffffffffffffffffffffffffffffffffffffffffffffff83818316178352505050506040518082805190602001908083835b602083106101cb57805182526020820191506020810190506020830392506101a8565b6001836020036101000a0380198251168184511680821785525050505050509050019150506000604051808303816000865af19150503d806000811461022d576040519150601f19603f3d011682016040523d82523d6000602084013e610232565b606091505b505090508061024057600080fd5b505050505056fea165627a7a7230582076596bab6327318a6d52173ade6bead9c1fb2d5d8c510f9a288d2593598ccc1c0029";

auto call_tr = "955c806d";

auto call_se = "e354bcee";

/*
   pragma solidity >=0.4.22 <0.6.0;

   contract solidity3 {
      constructor() payable public {}
      function() external payable {}
   }

   contract solidity2 {

      solidity3 sol3;

      constructor() payable public { sol3 = new solidity3(); }
      function() external payable {}
   }

   contract solidity {

      solidity2 sol2;

      constructor() payable public { sol2 = new solidity2(); }
      function() external payable {}
   }
*/

auto contract_create = "608060405260405161001090610071565b604051809103906000f08015801561002c573d6000803e3d6000fd5b506000806101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff16021790555061007d565b60fc806100bd83390190565b60328061008b6000396000f3fe608060405200fea165627a7a7230582051d43d8fc56d085c3143d29c2e9cc1a7faa12631bf718f59b6be5b366201b36c00296080604052604051600e90606d565b604051809103906000f0801580156029573d6000803e3d6000fd5b506000806101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff1602179055506079565b6043806100b983390190565b6032806100876000396000f3fe608060405200fea165627a7a723058208e5c3f1e14b8d7ec81555c948892cf4cff57628723ee0992c82bd7c9dabc69ba0029608060405260328060116000396000f3fe608060405200fea165627a7a72305820e253f691c783d44f3b823380ee66794b53814a20d14e8c89fd069add28027c6e0029";

/*
   pragma solidity >=0.4.22 <0.6.0;

   contract solidity2 {
      function() external payable {}
   }

   contract solidity {

      solidity2 sol2;

      function create() payable public { sol2 = new solidity2(); }
      function() external payable {}
   }
*/

auto contract_create_2 = "608060405234801561001057600080fd5b50610138806100206000396000f3fe60806040526004361061003b576000357c010000000000000000000000000000000000000000000000000000000090048063efc81a8c1461003d575b005b610045610047565b005b604051610053906100b1565b604051809103906000f08015801561006f573d6000803e3d6000fd5b506000806101000a81548173ffffffffffffffffffffffffffffffffffffffff021916908373ffffffffffffffffffffffffffffffffffffffff160217905550565b604f806100be8339019056fe6080604052348015600f57600080fd5b50603280601d6000396000f3fe608060405200fea165627a7a7230582060937dffd9985f937f17cd6255ba9332a9fc86073eac4decfe84005e5c124cb10029a165627a7a72305820fa6f976f560fe8893e3e4b318b51ca551f95f1f52f99d8ea7fc505a482431ca40029";

/*
   pragma solidity >=0.4.22 <0.6.0;

   contract solidityCall {
      constructor() payable public {}
      function se(address payable callAddr) payable public {
         (bool success,) = callAddr.call(abi.encodeWithSignature("create()"));
         require(success);
      }
      function() external payable {}
   }
*/

auto contract_call_code_2 = "60806040526101f5806100136000396000f3fe60806040526004361061003b576000357c0100000000000000000000000000000000000000000000000000000000900480636d74f0661461003d575b005b61007f6004803603602081101561005357600080fd5b81019080803573ffffffffffffffffffffffffffffffffffffffff169060200190929190505050610081565b005b60008173ffffffffffffffffffffffffffffffffffffffff166040516024016040516020818303038152906040527fefc81a8c000000000000000000000000000000000000000000000000000000007bffffffffffffffffffffffffffffffffffffffffffffffffffffffff19166020820180517bffffffffffffffffffffffffffffffffffffffffffffffffffffffff83818316178352505050506040518082805190602001908083835b60208310610150578051825260208201915060208101905060208303925061012d565b6001836020036101000a0380198251168184511680821785525050505050509050019150506000604051808303816000865af19150503d80600081146101b2576040519150601f19603f3d011682016040523d82523d6000602084013e6101b7565b606091505b50509050806101c557600080fd5b505056fea165627a7a723058207f40f44b3628ab4f3d562d5f2078dd2e3d5ae50354bdd1fc56653f1c3100e91c0029";

auto call_se_2 = "6d74f066";

vms::evm::evm_result create_contract( database& db, std::string code, uint64_t value, uint64_t id_asset, std::set<uint64_t> allowed_assets )
{
   transaction_evaluation_state context(&db);
   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type( 5 );
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), allowed_assets, asset_id_type( id_asset ), value, asset_id_type(), 1, 500000, code } );
   operation_result res_contract;
   BOOST_CHECK_NO_THROW( res_contract = db.apply_operation( context, contract_op ) );
   return fc::raw::unpack<vms::evm::evm_result>( *db.db_res.get_results( std::string( res_contract.get<object_id_type>() ) ) );
}

vms::evm::evm_result call_contract( database& db, uint64_t receiver, std::string code, uint64_t value, uint64_t id_asset )
{
   transaction_evaluation_state context(&db);
   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type( 5 );
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, contract_id_type( receiver ), {}, asset_id_type(id_asset), value, asset_id_type(), 1, 500000, code } );
   operation_result res_contract;
   BOOST_CHECK_NO_THROW( res_contract = db.apply_operation( context, contract_op ) );
   return fc::raw::unpack<vms::evm::evm_result>( *db.db_res.get_results( std::string( res_contract.get<object_id_type>() ) ) );
}

BOOST_AUTO_TEST_CASE( create_contract_transfer_CORE_asset_test )
{
   transaction_evaluation_state context(&db);
   transfer(account_id_type(0),account_id_type(5),asset(1000000000, asset_id_type()));

   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type(5);

   db._evaluating_from_block = true;

   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), std::set<uint64_t>(), asset_id_type(), 131313, asset_id_type(), 1, 500000, contract_transfer_code } );
   operation_result res;
   BOOST_CHECK_NO_THROW( res = db.apply_operation( context, contract_op ) );
   auto result = fc::raw::unpack<vms::evm::evm_result>( *db.db_res.get_results( std::string( res.get<object_id_type>() ) ) );
   auto balance = db.get_balance( contract_id_type( vms::evm::address_to_id( result.exec_result.newAddress ).second ), asset_id_type(0) );
   BOOST_CHECK( balance == asset( 131313, asset_id_type( 0 ) ) );

   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), {1}, asset_id_type(), 131313, asset_id_type(), 1, 500000, contract_transfer_code } );
   BOOST_CHECK_THROW( db.apply_operation( context, contract_op ), fc::exception );

   db._evaluating_from_block = false;
}

BOOST_AUTO_TEST_CASE( create_contract_transfer_different_asset_test )
{
   transaction_evaluation_state context(&db);
   asset_object temp_asset = create_user_issued_asset( "ETB" );
   issue_uia( account_id_type( 5 ), temp_asset.amount( 10000000 ) );
   transfer( account_id_type( 0 ), account_id_type( 5 ), asset( 10000000, asset_id_type() ) );

   contract_operation contract_op;
   contract_op.vm_type = vm_types::EVM;
   contract_op.registrar = account_id_type( 5 );

   db._evaluating_from_block = true;

   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), {1}, asset_id_type(), 131313, asset_id_type(), 1, 500000, contract_transfer_code } );
   BOOST_CHECK_THROW( db.apply_operation( context, contract_op ), fc::exception );

   auto id_new_asset = static_cast<uint64_t>( temp_asset.get_id().instance );
   contract_op.data = fc::raw::unsigned_pack( eth_op{ contract_op.registrar, optional<contract_id_type>(), { id_new_asset }, asset_id_type( id_new_asset ), 131313, asset_id_type(), 1, 500000, contract_transfer_code } );
   BOOST_CHECK_NO_THROW( db.apply_operation( context, contract_op ) );

   db._evaluating_from_block = false;
}

BOOST_AUTO_TEST_CASE( transfer_funds_from_contract_test )
{
   asset_object temp_asset = create_user_issued_asset( "ETB" );
   issue_uia( account_id_type( 5 ), temp_asset.amount( 10000000 ) );
   transfer( account_id_type( 0 ), account_id_type( 5 ), asset( 10000000, asset_id_type() ) );
   auto id_new_asset = static_cast<uint64_t>( temp_asset.get_id().instance );
   db._evaluating_from_block = true;

   auto result_create_1 = create_contract( db, contract_transfer_code, 131313, id_new_asset, std::set<uint64_t>() );
   auto id_first_acc = vms::evm::address_to_id( result_create_1.exec_result.newAddress ).second;
   auto result_call_1 = call_contract( db, id_first_acc, "00", 131313, 0 );
   auto result_create_2 = create_contract( db, contract_transfer_code, 0, 0, { id_new_asset } );

   std::string call = call_tr + std::string( "000000000000000000000000" ) + result_create_2.exec_result.newAddress.hex() + dev::h256(dev::u256(13)).hex() + dev::h256(dev::u256(id_new_asset)).hex();
   auto result_call_2 = call_contract( db, id_first_acc, call, 0, 0 );

   auto balance = db.get_balance( contract_id_type( vms::evm::address_to_id( result_create_2.exec_result.newAddress ).second ), asset_id_type( id_new_asset ) );
   BOOST_CHECK( balance.amount.value == 13 );

   call = call_tr + std::string( "000000000000000000000000" ) + result_create_2.exec_result.newAddress.hex() + dev::h256(dev::u256(13)).hex() + dev::h256(dev::u256(0)).hex();
   auto result_call_3 = call_contract( db, id_first_acc, call, 0, 0 );
   BOOST_CHECK( result_call_3.exec_result.excepted == dev::eth::TransactionException::ProhibitedAssetType );

   balance = db.get_balance( contract_id_type( vms::evm::address_to_id( result_create_2.exec_result.newAddress ).second ), asset_id_type( id_new_asset ) );
   BOOST_CHECK( balance.amount.value == 13 );

   db._evaluating_from_block = false;
}

BOOST_AUTO_TEST_CASE( transfer_funds_from_deep_contract_test )
{
   asset_object temp_asset = create_user_issued_asset( "ETB" );
   issue_uia( account_id_type( 5 ), temp_asset.amount( 10000000 ) );
   transfer( account_id_type( 0 ), account_id_type( 5 ), asset( 10000000, asset_id_type() ) );
   auto id_new_asset = static_cast<uint64_t>( temp_asset.get_id().instance );
   db._evaluating_from_block = true;

   auto result_create_1 = create_contract( db, contract_transfer_code, 131313, id_new_asset, std::set<uint64_t>() );
   auto id_first_acc = vms::evm::address_to_id( result_create_1.exec_result.newAddress ).second;
   auto result_call_1 = call_contract( db, id_first_acc, "00", 131313, 0 );
   auto result_create_2 = create_contract( db, contract_transfer_code, 0, 0, { id_new_asset } );

   auto result_create_3 = create_contract( db, contract_call_code, 0, 0, std::set<uint64_t>() );
   auto id_acc_3 = vms::evm::address_to_id( result_create_3.exec_result.newAddress ).second;

   auto call = call_se + std::string( "000000000000000000000000" ) + result_create_1.exec_result.newAddress.hex() +
          std::string( "000000000000000000000000" ) + result_create_2.exec_result.newAddress.hex() +
          dev::h256(dev::u256(13)).hex() + dev::h256(dev::u256(id_new_asset)).hex();

   auto result_call_3 = call_contract( db, id_acc_3, call, 0, 0 );
   auto balance = db.get_balance( contract_id_type( vms::evm::address_to_id( result_create_2.exec_result.newAddress ).second ), asset_id_type( id_new_asset ) );
   BOOST_CHECK( balance.amount.value == 13 );

   call = call_se + std::string( "000000000000000000000000" ) + result_create_1.exec_result.newAddress.hex() +
          std::string( "000000000000000000000000" ) + result_create_2.exec_result.newAddress.hex() +
          dev::h256(dev::u256(13)).hex() + dev::h256(dev::u256(0)).hex();
   auto result_call_4 = call_contract( db, id_acc_3, call, 0, 0 );
   BOOST_CHECK( result_call_4.exec_result.excepted == dev::eth::TransactionException::RevertInstruction );
   balance = db.get_balance( contract_id_type( vms::evm::address_to_id( result_create_2.exec_result.newAddress ).second ), asset_id_type( id_new_asset ) );
   BOOST_CHECK( balance.amount.value == 13 );

   db._evaluating_from_block = false;
}

BOOST_AUTO_TEST_CASE( inheritance_the_allowed_assets_property_test )
{
   asset_object temp_asset = create_user_issued_asset( "ETB" );
   issue_uia( account_id_type( 5 ), temp_asset.amount( 10000000 ) );
   transfer( account_id_type( 0 ), account_id_type( 5 ), asset( 10000000, asset_id_type() ) );
   auto id_new_asset = static_cast<uint64_t>( temp_asset.get_id().instance );
   db._evaluating_from_block = true;

   std::set<uint64_t> assets_id = {1,2,3};
   const auto& contract_idx = db.get_index_type<contract_index>().indices().get<by_id>();
   BOOST_CHECK( contract_idx.size() == 0 );

   auto result_create_1 = create_contract( db, contract_transfer_code, 131313, id_new_asset, assets_id );

   BOOST_CHECK( contract_idx.size() == 1 );
   BOOST_CHECK( contract_idx.begin()->allowed_assets == assets_id );

   db._evaluating_from_block = false;
}

BOOST_AUTO_TEST_CASE( inheritance_the_allow_assets_parent_property_test )
{
   transfer( account_id_type( 0 ), account_id_type( 5 ), asset( 10000000, asset_id_type() ) );
   db._evaluating_from_block = true;

   std::set<uint64_t> assets_id = {1,2,3};
   const auto& contract_idx = db.get_index_type<contract_index>().indices().get<by_id>();
   BOOST_CHECK( contract_idx.size() == 0 );

   auto result_create_1 = create_contract( db, contract_create, 0, 0, assets_id );

   BOOST_CHECK( contract_idx.size() == 3 );
   for( const auto& itr : contract_idx ) {
      BOOST_CHECK( itr.allowed_assets == assets_id );
   }

   db._evaluating_from_block = false;
}

BOOST_AUTO_TEST_CASE( inheritance_the_allow_assets_parent_property_deep_test )
{
   transfer( account_id_type( 0 ), account_id_type( 5 ), asset( 10000000, asset_id_type() ) );
   db._evaluating_from_block = true;

   std::set<uint64_t> assets_id = {1,2,3};
   const auto& contract_idx = db.get_index_type<contract_index>().indices().get<by_id>();
   BOOST_CHECK( contract_idx.size() == 0 );

   auto result_create_1 = create_contract( db, contract_create_2, 0, 0, std::set<uint64_t>() );
   auto result_create_2 = create_contract( db, contract_call_code_2, 0, 0, assets_id );

   BOOST_CHECK( contract_idx.size() == 2 );

   auto call = call_se_2 + std::string( "000000000000000000000000" ) + result_create_1.exec_result.newAddress.hex();
   auto result_call_1 = call_contract( db, vms::evm::address_to_id( result_create_2.exec_result.newAddress ).second, call, 0, 0 );

   BOOST_CHECK( contract_idx.size() == 3 );
   auto itr = contract_idx.begin();
   BOOST_CHECK( itr->allowed_assets == std::set<uint64_t>() );
   BOOST_CHECK( (++itr)->allowed_assets == assets_id );
   BOOST_CHECK( (++itr)->allowed_assets == assets_id );

   db._evaluating_from_block = false;
}

BOOST_AUTO_TEST_SUITE_END()
