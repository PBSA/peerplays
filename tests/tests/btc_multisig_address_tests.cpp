#include <boost/test/unit_test.hpp>
#include <fc/crypto/digest.hpp>
#include <sidechain/btc_multisig_address.hpp>

using namespace sidechain;

fc::ecc::public_key_data create_public_key_data( const std::vector<char>& public_key )
{
   FC_ASSERT( public_key.size() == 33 );
   fc::ecc::public_key_data key;
   for(size_t i = 0; i < 33; i++) {
      key.at(i) = public_key[i];
   }
   return key;
}

BOOST_AUTO_TEST_SUITE( btc_multisig_address_tests )

BOOST_AUTO_TEST_CASE( create_multisig_address_test ) {

   std::vector<char> public_key1 = parse_hex( "03db643710666b862e0a97f7edbe8ef40ec2c4a29ef995c431c21ca85e35000010" );
   std::vector<char> public_key2 = parse_hex( "0320000d982c156a6f09df8c7674abddc2bb326533268ed03572916221b4417983" );
   std::vector<char> public_key3 = parse_hex( "033619e682149aef0c3e2dee3dc5107dd78cb2c14bf0bd25b59056259fbb37ec3f" );

   std::vector<char> address = parse_hex( "a91460cb986f0926e7c4ca1984ca9f56767da2af031e87" );
   std::vector<char> redeem_script = parse_hex( "522103db643710666b862e0a97f7edbe8ef40ec2c4a29ef995c431c21ca85e35000010210320000d982c156a6f09df8c7674abddc2bb326533268ed03572916221b441798321033619e682149aef0c3e2dee3dc5107dd78cb2c14bf0bd25b59056259fbb37ec3f53ae" );

   fc::ecc::public_key_data key1 = create_public_key_data( public_key1 );
   fc::ecc::public_key_data key2 = create_public_key_data( public_key2 );
   fc::ecc::public_key_data key3 = create_public_key_data( public_key3 );

   sidechain::btc_multisig_segwit_address cma(2, { { account_id_type(1), public_key_type(key1) }, { account_id_type(2), public_key_type(key2) }, { account_id_type(3), public_key_type(key3) } });

   BOOST_CHECK( address == cma.address );
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
   BOOST_CHECK( address.get_base58_address() == "2NGU4ogScHEHEpReUzi9RB2ha58KAFnkFyk" );
}

BOOST_AUTO_TEST_SUITE_END()
