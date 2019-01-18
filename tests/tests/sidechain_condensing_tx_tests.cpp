#include <boost/test/unit_test.hpp>
#include <sidechain/sidechain_condensing_tx.hpp>
#include <sidechain/bitcoin_script.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/crypto/digest.hpp>

#include <iostream>
#include <algorithm>

BOOST_AUTO_TEST_SUITE( sidechain_condensing_tx_tests )

uint64_t size_fee = 100;
uint64_t pw_vout_amount = 113;
double witness_percentage = 0.01;

void create_info_vins_and_info_vouts( std::vector<info_for_vin>& info_vins, std::vector<info_for_vout>& info_vouts )
{
   for( size_t i = 0; i < 10; i++ ) {
      info_for_vin vin;
      vin.out.hash_tx = "1111111111111111111111111111111111111111111111111111111111111111";
      vin.out.n_vout = static_cast<uint32_t>( i );
      vin.out.amount = static_cast<uint64_t>( i + 1000 );
      vin.address = std::to_string( i );
      vin.script = { 0x0d };
      info_vins.push_back( vin );

      info_for_vout vout;
      vout.payer = account_id_type( i );
      vout.address = sidechain::bitcoin_address( "mipcBbFg9gMiCh81Kj8tqqdgoZub1ZJRfn" );
      vout.amount = static_cast<uint64_t>( i + 1000 );
      info_vouts.push_back( vout );
   }
}

info_for_vin create_pw_vin()
{
   info_for_vin vin_info;
   vin_info.out.hash_tx = "2222222222222222222222222222222222222222222222222222222222222222";
   vin_info.out.n_vout = static_cast<uint32_t>( 13 );
   vin_info.out.amount = static_cast<uint64_t>( 1113 );
   vin_info.address = std::to_string( 13 );
   vin_info.script = { 0x0d };

   return vin_info;
}

accounts_keys create_accounts_keys()
{
   fc::ecc::private_key priv_key = fc::ecc::private_key::regenerate( fc::digest( "key" ) );
   return { { account_id_type( 0 ), public_key_type( priv_key.get_public_key() ) },
            { account_id_type( 1 ), public_key_type( priv_key.get_public_key() ) },
            { account_id_type( 2 ), public_key_type( priv_key.get_public_key() ) } };
}

BOOST_AUTO_TEST_CASE( create_sidechain_condensing_tx_test )
{
   std::vector<info_for_vin> info_vins;
   std::vector<info_for_vout> info_vouts;

   create_info_vins_and_info_vouts( info_vins, info_vouts );

   sidechain_condensing_tx ct( info_vins, info_vouts );
   const auto& tx = ct.get_transaction();

   BOOST_CHECK( tx.vin.size() == 10 );
   BOOST_CHECK( tx.vout.size() == 10 );
   for( size_t i = 0; i < 10; i++ ) {
      BOOST_CHECK( tx.vin[i].prevout.hash == fc::sha256( "1111111111111111111111111111111111111111111111111111111111111111" ) );
      BOOST_CHECK( tx.vin[i].prevout.n == static_cast<uint32_t>( i ) );
      bytes scriptSig = { 0x22, 0x0d };
      BOOST_CHECK( tx.vin[i].scriptSig == scriptSig );
      BOOST_CHECK( tx.vin[i].scriptWitness == std::vector<bytes>() );

      BOOST_CHECK( tx.vout[i].value == static_cast<int64_t>( i + 1000 ) );
      const auto address_bytes = fc::from_base58( "mipcBbFg9gMiCh81Kj8tqqdgoZub1ZJRfn" );
      bytes raw_address( address_bytes.begin() + 1, address_bytes.begin() + 21 );
      bytes scriptPubKey = script_builder() << op::DUP << op::HASH160 << raw_address << op::EQUALVERIFY << op::CHECKSIG;
      BOOST_CHECK( tx.vout[i].scriptPubKey == scriptPubKey );
   }
}

BOOST_AUTO_TEST_CASE( creation_additional_in_out_sidechain_condensing_tx )
{
   std::vector<info_for_vin> info_vins;
   std::vector<info_for_vout> info_vouts;

   create_info_vins_and_info_vouts( info_vins, info_vouts );

   accounts_keys keys = create_accounts_keys();

   sidechain_condensing_tx ct( info_vins, info_vouts );
   ct.create_pw_vin( create_pw_vin() );
   ct.create_vouts_for_witness_fee( keys );
   ct.create_pw_vout( 1113, bytes{ 0x0d, 0x0d, 0x0d } );
   const auto& tx = ct.get_transaction();

   BOOST_CHECK( tx.vin.size() == 11 );
   BOOST_CHECK( tx.vout.size() == 14 );

   BOOST_CHECK( tx.vin[0].prevout.hash == fc::sha256( "2222222222222222222222222222222222222222222222222222222222222222" ) );
   BOOST_CHECK( tx.vin[0].prevout.n == static_cast<uint32_t>( 13 ) );
   bytes scriptSig = { 0x22, 0x0d };
   BOOST_CHECK( tx.vin[0].scriptSig == bytes() );

   BOOST_CHECK( tx.vout[0].value == static_cast<int64_t>( 1113 ) );
   bytes scriptPubKey{ 0x00, 0x01, 0x0d };
   BOOST_CHECK( tx.vout[0].scriptPubKey == scriptPubKey );
   for( size_t i = 1; i <= keys.size(); i++ ) {
      BOOST_CHECK( tx.vout[i].value == static_cast<int64_t>( 0 ) );
      bytes scriptPubKey{ 0x21 };
      const auto key_data = keys[account_id_type( i - 1 )].key_data;
      std::copy( key_data.begin(), key_data.end(), std::back_inserter( scriptPubKey ) );
      scriptPubKey.push_back( 0xac );
      BOOST_CHECK( tx.vout[i].scriptPubKey == scriptPubKey );
   }
}

BOOST_AUTO_TEST_CASE( subtract_fee_tests )
{
   std::vector<info_for_vin> info_vins;
   std::vector<info_for_vout> info_vouts;

   accounts_keys keys = create_accounts_keys();
   create_info_vins_and_info_vouts( info_vins, info_vouts );
   uint64_t size_fee_user = size_fee / ( info_vins.size() + info_vouts.size() );

   sidechain_condensing_tx ct( info_vins, info_vouts );
   ct.create_pw_vin( create_pw_vin() );
   ct.create_vouts_for_witness_fee( keys );
   ct.create_pw_vout( pw_vout_amount, bytes{ 0x0d, 0x0d, 0x0d } );
   ct.subtract_fee( size_fee, witness_percentage );
   const auto& tx = ct.get_transaction();

   std::vector<uint64_t> witnesses_fee;
   for( size_t i = 0; i < info_vouts.size(); i++ ) {
      witnesses_fee.push_back( ( info_vouts[i].amount - size_fee_user ) * witness_percentage );
   }

   uint64_t witnesses_fee_sum = std::accumulate( witnesses_fee.begin(), witnesses_fee.end(), 0 );
   uint64_t witness_fee = witnesses_fee_sum / keys.size();
   BOOST_CHECK( tx.vout[0].value == static_cast<int64_t>( pw_vout_amount ) );
   for( size_t i = 1; i <= keys.size(); i++ ) {
      BOOST_CHECK( tx.vout[i].value == static_cast<int64_t>( witness_fee ) );
   }

   size_t offset = 1 + keys.size();
   for( size_t i = offset; i < tx.vout.size(); i++ ) {
      BOOST_CHECK( tx.vout[i].value == static_cast<int64_t>( ( info_vouts[i - offset].amount - size_fee_user ) - witnesses_fee[i - offset] ) );
   }
}

BOOST_AUTO_TEST_CASE( subtract_fee_not_pw_vout_and_witness_vouts_tests )
{
   std::vector<info_for_vin> info_vins;
   std::vector<info_for_vout> info_vouts;

   create_info_vins_and_info_vouts( info_vins, info_vouts );
   uint64_t size_fee_user = size_fee / ( info_vins.size() + info_vouts.size() );

   sidechain_condensing_tx ct( info_vins, info_vouts );
   ct.create_pw_vin( create_pw_vin() );
   ct.subtract_fee( size_fee, witness_percentage );
   const auto& tx = ct.get_transaction();

   std::vector<uint64_t> witnesses_fee;
   for( size_t i = 0; i < info_vouts.size(); i++ ) {
      witnesses_fee.push_back( ( info_vouts[i].amount - size_fee_user ) * witness_percentage );
   }

   for( size_t i = 0; i < tx.vout.size(); i++ ) {
      BOOST_CHECK( tx.vout[i].value == static_cast<int64_t>( ( info_vouts[i].amount - size_fee_user ) - witnesses_fee[i] ) );
   }
}

BOOST_AUTO_TEST_CASE( subtract_fee_not_witness_vouts_tests )
{
   std::vector<info_for_vin> info_vins;
   std::vector<info_for_vout> info_vouts;

   create_info_vins_and_info_vouts( info_vins, info_vouts );
   uint64_t size_fee_user = size_fee / ( info_vins.size() + info_vouts.size() );

   sidechain_condensing_tx ct( info_vins, info_vouts );
   ct.create_pw_vin( create_pw_vin() );
   ct.create_pw_vout( pw_vout_amount, bytes{ 0x0d, 0x0d, 0x0d } );
   ct.subtract_fee( size_fee, witness_percentage );
   const auto& tx = ct.get_transaction();

   std::vector<uint64_t> witnesses_fee;
   for( size_t i = 0; i < info_vouts.size(); i++ ) {
      witnesses_fee.push_back( ( info_vouts[i].amount - size_fee_user ) * witness_percentage );
   }

   size_t offset = 1;
   for( size_t i = offset; i < tx.vout.size(); i++ ) {
      BOOST_CHECK( tx.vout[i].value == static_cast<int64_t>( ( info_vouts[i - offset].amount - size_fee_user ) - witnesses_fee[i - offset] ) );
   }
}

BOOST_AUTO_TEST_CASE( subtract_fee_not_pw_vout_tests )
{
   std::vector<info_for_vin> info_vins;
   std::vector<info_for_vout> info_vouts;

   create_info_vins_and_info_vouts( info_vins, info_vouts );
   uint64_t size_fee_user = size_fee / ( info_vins.size() + info_vouts.size() );
   accounts_keys keys = create_accounts_keys();

   sidechain_condensing_tx ct( info_vins, info_vouts );
   ct.create_pw_vin( create_pw_vin() );
   ct.create_vouts_for_witness_fee( keys );
   ct.subtract_fee( size_fee, witness_percentage );
   const auto& tx = ct.get_transaction();

   std::vector<uint64_t> witnesses_fee;
   for( size_t i = 0; i < info_vouts.size(); i++ ) {
      witnesses_fee.push_back( ( info_vouts[i].amount - size_fee_user ) * witness_percentage );
   }

   size_t offset = keys.size();
   for( size_t i = offset; i < tx.vout.size(); i++ ) {
      BOOST_CHECK( tx.vout[i].value == static_cast<int64_t>( ( info_vouts[i - offset].amount - size_fee_user ) - witnesses_fee[i - offset] ) );
   }
}

BOOST_AUTO_TEST_CASE( subtract_fee_not_vins_vout_tests )
{
   std::vector<info_for_vin> info_vins;
   std::vector<info_for_vout> info_vouts;

   create_info_vins_and_info_vouts( info_vins, info_vouts );
   info_vins.clear();
   uint64_t size_fee_user = size_fee / ( info_vins.size() + info_vouts.size() );
   accounts_keys keys = create_accounts_keys();

   sidechain_condensing_tx ct( info_vins, info_vouts );
   ct.create_pw_vin( create_pw_vin() );
   ct.create_vouts_for_witness_fee( keys );
   ct.create_pw_vout( pw_vout_amount, bytes{ 0x0d, 0x0d, 0x0d } );
   ct.subtract_fee( size_fee, witness_percentage );
   const auto& tx = ct.get_transaction();

   std::vector<uint64_t> witnesses_fee;
   for( size_t i = 0; i < info_vouts.size(); i++ ) {
      witnesses_fee.push_back( ( info_vouts[i].amount - size_fee_user ) * witness_percentage );
   }

   size_t offset = 1 + keys.size();
   for( size_t i = offset; i < tx.vout.size(); i++ ) {
      BOOST_CHECK( tx.vout[i].value == static_cast<int64_t>( ( info_vouts[i - offset].amount - size_fee_user ) - witnesses_fee[i - offset] ) );
   }
}

BOOST_AUTO_TEST_SUITE_END()
