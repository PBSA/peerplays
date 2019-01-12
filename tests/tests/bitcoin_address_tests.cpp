#include <boost/test/unit_test.hpp>
#include <sidechain/bitcoin_address.hpp>

using namespace sidechain;

BOOST_AUTO_TEST_SUITE( bitcoin_address_tests )

fc::ecc::public_key_data create_public_key_data( const std::vector<char>& public_key )
{
   FC_ASSERT( public_key.size() == 33 );
   fc::ecc::public_key_data key;
   for(size_t i = 0; i < 33; i++) {
      key.at(i) = public_key[i];
   }
   return key;
}

BOOST_AUTO_TEST_CASE( addresses_type_test )
{
   // public_key
   std::string compressed( "03df51984d6b8b8b1cc693e239491f77a36c9e9dfe4a486e9972a18e03610a0d22" );
   BOOST_CHECK( bitcoin_address( compressed ).get_type() == payment_type::P2PK );

   std::string uncompressed( "04fe53c78e36b86aae8082484a4007b706d5678cabb92d178fc95020d4d8dc41ef44cfbb8dfa7a593c7910a5b6f94d079061a7766cbeed73e24ee4f654f1e51904" );
   BOOST_CHECK( bitcoin_address( uncompressed ).get_type() == payment_type::NULLDATA );


   // segwit_address
   std::string p2wpkh_mainnet( "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4" );
   BOOST_CHECK( bitcoin_address( p2wpkh_mainnet ).get_type() == payment_type::P2WPKH );

   std::string p2wpkh_testnet( "tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx" );
   BOOST_CHECK( bitcoin_address( p2wpkh_testnet ).get_type() == payment_type::P2WPKH );

   std::string p2wsh( "bc1qc7slrfxkknqcq2jevvvkdgvrt8080852dfjewde450xdlk4ugp7szw5tk9" );
   BOOST_CHECK( bitcoin_address( p2wsh ).get_type() == payment_type::P2WSH );


   // base58
   std::string p2pkh_mainnet( "17VZNX1SN5NtKa8UQFxwQbFeFc3iqRYhem" );
   BOOST_CHECK( bitcoin_address( p2pkh_mainnet ).get_type() == payment_type::P2PKH );

   std::string p2pkh_testnet( "mipcBbFg9gMiCh81Kj8tqqdgoZub1ZJRfn" );
   BOOST_CHECK( bitcoin_address( p2pkh_testnet ).get_type() == payment_type::P2PKH );

   std::string p2sh_mainnet( "3EktnHQD7RiAE6uzMj2ZifT9YgRrkSgzQX" );
   BOOST_CHECK( bitcoin_address( p2sh_mainnet ).get_type() == payment_type::P2SH );

   std::string p2sh_testnet( "2MzQwSSnBHWHqSAqtTVQ6v47XtaisrJa1Vc" );
   BOOST_CHECK( bitcoin_address( p2sh_testnet ).get_type() == payment_type::P2SH );
}

BOOST_AUTO_TEST_CASE( addresses_raw_test )
{
   // public_key
   std::string compressed( "03df51984d6b8b8b1cc693e239491f77a36c9e9dfe4a486e9972a18e03610a0d22" );
   bytes standard_compressed( parse_hex( compressed ) );
   BOOST_CHECK( bitcoin_address( compressed ).get_raw_address() == standard_compressed );

   std::string uncompressed( "04fe53c78e36b86aae8082484a4007b706d5678cabb92d178fc95020d4d8dc41ef44cfbb8dfa7a593c7910a5b6f94d079061a7766cbeed73e24ee4f654f1e51904" );
   BOOST_CHECK( bitcoin_address( uncompressed ).get_raw_address() == bytes() );


   // segwit_address
   std::string p2wpkh_mainnet( "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4" );
   bytes standard_p2wpkh_mainnet( parse_hex( "751e76e8199196d454941c45d1b3a323f1433bd6" ) );
   BOOST_CHECK( bitcoin_address( p2wpkh_mainnet ).get_raw_address() == standard_p2wpkh_mainnet );

   std::string p2wpkh_testnet( "tb1qw508d6qejxtdg4y5r3zarvary0c5xw7kxpjzsx" );
   bytes standard_p2wpkh_testnet( parse_hex( "751e76e8199196d454941c45d1b3a323f1433bd6" ) );
   BOOST_CHECK( bitcoin_address( p2wpkh_testnet ).get_raw_address() == standard_p2wpkh_testnet );

   std::string p2wsh( "bc1qc7slrfxkknqcq2jevvvkdgvrt8080852dfjewde450xdlk4ugp7szw5tk9" );
   bytes standard_p2wsh( parse_hex( "c7a1f1a4d6b4c1802a59631966a18359de779e8a6a65973735a3ccdfdabc407d" ) );
   BOOST_CHECK( bitcoin_address( p2wsh ).get_raw_address() == standard_p2wsh );


   // base58
   std::string p2pkh_mainnet( "17VZNX1SN5NtKa8UQFxwQbFeFc3iqRYhem" );
   bytes standard_p2pkh_mainnet( parse_hex( "47376c6f537d62177a2c41c4ca9b45829ab99083" ) );
   BOOST_CHECK( bitcoin_address( p2pkh_mainnet ).get_raw_address() == standard_p2pkh_mainnet );

   std::string p2pkh_testnet( "mipcBbFg9gMiCh81Kj8tqqdgoZub1ZJRfn" );
   bytes standard_p2pkh_testnet( parse_hex( "243f1394f44554f4ce3fd68649c19adc483ce924" ) );
   BOOST_CHECK( bitcoin_address( p2pkh_testnet ).get_raw_address() == standard_p2pkh_testnet );

   std::string p2sh_mainnet( "3EktnHQD7RiAE6uzMj2ZifT9YgRrkSgzQX" );
   bytes standard_p2sh_mainnet( parse_hex( "8f55563b9a19f321c211e9b9f38cdf686ea07845" ) );
   BOOST_CHECK( bitcoin_address( p2sh_mainnet ).get_raw_address() == standard_p2sh_mainnet );

   std::string p2sh_testnet( "2MzQwSSnBHWHqSAqtTVQ6v47XtaisrJa1Vc" );
   bytes standard_p2sh_testnet( parse_hex( "4e9f39ca4688ff102128ea4ccda34105324305b0" ) );
   BOOST_CHECK( bitcoin_address( p2sh_testnet ).get_raw_address() == standard_p2sh_testnet );
}

BOOST_AUTO_TEST_CASE( create_multisig_address_test ) {

   std::vector<char> public_key1 = parse_hex( "03db643710666b862e0a97f7edbe8ef40ec2c4a29ef995c431c21ca85e35000010" );
   std::vector<char> public_key2 = parse_hex( "0320000d982c156a6f09df8c7674abddc2bb326533268ed03572916221b4417983" );
   std::vector<char> public_key3 = parse_hex( "033619e682149aef0c3e2dee3dc5107dd78cb2c14bf0bd25b59056259fbb37ec3f" );

   std::vector<char> address = parse_hex( "a91460cb986f0926e7c4ca1984ca9f56767da2af031e87" );
   std::vector<char> redeem_script = parse_hex( "522103db643710666b862e0a97f7edbe8ef40ec2c4a29ef995c431c21ca85e35000010210320000d982c156a6f09df8c7674abddc2bb326533268ed03572916221b441798321033619e682149aef0c3e2dee3dc5107dd78cb2c14bf0bd25b59056259fbb37ec3f53ae" );

   fc::ecc::public_key_data key1 = create_public_key_data( public_key1 );
   fc::ecc::public_key_data key2 = create_public_key_data( public_key2 );
   fc::ecc::public_key_data key3 = create_public_key_data( public_key3 );

   sidechain::btc_multisig_address cma(2, { { account_id_type(1), public_key_type(key1) }, { account_id_type(2), public_key_type(key2) }, { account_id_type(3), public_key_type(key3) } });

   BOOST_CHECK( address == cma.raw_address );
   BOOST_CHECK( redeem_script == cma.redeem_script );
}

BOOST_AUTO_TEST_CASE( create_segwit_address_test ) {
   // https://0bin.net/paste/nfnSf0HcBqBUGDto#7zJMRUhGEBkyh-eASQPEwKfNHgQ4D5KrUJRsk8MTPSa
   std::vector<char> public_key1 = parse_hex( "03b3623117e988b76aaabe3d63f56a4fc88b228a71e64c4cc551d1204822fe85cb" );
   std::vector<char> public_key2 = parse_hex( "03dd823066e096f72ed617a41d3ca56717db335b1ea47a1b4c5c9dbdd0963acba6" );
   std::vector<char> public_key3 = parse_hex( "033d7c89bd9da29fa8d44db7906a9778b53121f72191184a9fee785c39180e4be1" );

   std::vector<char> witness_script = parse_hex("0020b6744de4f6ec63cc92f7c220cdefeeb1b1bed2b66c8e5706d80ec247d37e65a1");

   fc::ecc::public_key_data key1 = create_public_key_data( public_key1 );
   fc::ecc::public_key_data key2 = create_public_key_data( public_key2 );
   fc::ecc::public_key_data key3 = create_public_key_data( public_key3 );

   sidechain::btc_multisig_segwit_address address(2, { { account_id_type(1), public_key_type(key1) }, { account_id_type(2), public_key_type(key2) }, { account_id_type(3), public_key_type(key3) } });
   BOOST_CHECK( address.get_witness_script() == witness_script );
   BOOST_CHECK( address.get_address() == "2NGU4ogScHEHEpReUzi9RB2ha58KAFnkFyk" );
}

BOOST_AUTO_TEST_SUITE_END()
