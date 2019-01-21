#include <boost/test/unit_test.hpp>
#include <sidechain/utils.hpp>
#include <sidechain/bitcoin_transaction.hpp>
#include <sidechain/serialize.hpp>

using namespace sidechain;

BOOST_AUTO_TEST_SUITE( bitcoin_transaction_tests )

BOOST_AUTO_TEST_CASE( serialize_bitcoin_transaction_test )
{
    out_point prevout;
    prevout.hash = fc::sha256( "89df2e16bdc1fd00dffc72f24ec4da53ebb3ce1b08f55e7a9f874527b8714b9a" );
    prevout.n = 0;

    tx_in in;
    in.prevout = prevout;
    in.scriptSig = parse_hex( "473044022025f722981037f23949df7ac020e9895f6b4d35f98d04dadc43a8897d756b02f202203d4df0a81dac49be676657357f083d06f57b2d6b1199511ebdbd3bf82feff24101" );
    in.nSequence = 4294967294;

    tx_out out1;
    out1.value = 3500000000;
    out1.scriptPubKey = parse_hex( "76a914d377a5fd22419f8180f6d0d12215daffdd15b80088ac" );

    tx_out out2;
    out2.value = 1499996160;
    out2.scriptPubKey = parse_hex( "76a914e71562730a2d7b2c2c7f2f137d6ddf80e8ee024288ac" );

    bitcoin_transaction tx;
    tx.nVersion = 2;
    tx.vin = { in };
    tx.vout = { out1, out2 };
    tx.nLockTime = 101;

    const auto serialized = pack( tx );

    const auto expected = parse_hex( "02000000019a4b71b82745879f7a5ef5081bceb3eb53dac44ef272fcdf00fdc1bd162edf890000000048473044022025f722981037f23949df7ac020e9895f6b4d35f98d04dadc43a8897d756b02f202203d4df0a81dac49be676657357f083d06f57b2d6b1199511ebdbd3bf82feff24101feffffff0200c39dd0000000001976a914d377a5fd22419f8180f6d0d12215daffdd15b80088ac00206859000000001976a914e71562730a2d7b2c2c7f2f137d6ddf80e8ee024288ac65000000" );

    BOOST_CHECK_EQUAL_COLLECTIONS( serialized.cbegin(), serialized.cend(), expected.cbegin(), expected.cend() );
}

BOOST_AUTO_TEST_CASE( deserialize_bitcoin_transaction_test )
{
    bitcoin_transaction tx = unpack(parse_hex( "02000000019a4b71b82745879f7a5ef5081bceb3eb53dac44ef272fcdf00fdc1bd162edf890000000048473044022025f722981037f23949df7ac020e9895f6b4d35f98d04dadc43a8897d756b02f202203d4df0a81dac49be676657357f083d06f57b2d6b1199511ebdbd3bf82feff24101feffffff0200c39dd0000000001976a914d377a5fd22419f8180f6d0d12215daffdd15b80088ac00206859000000001976a914e71562730a2d7b2c2c7f2f137d6ddf80e8ee024288ac65000000" ) );

    BOOST_CHECK_EQUAL( tx.nVersion, 2 );
    BOOST_CHECK_EQUAL( tx.nLockTime, 101 );

    BOOST_REQUIRE_EQUAL( tx.vin.size(), 1 );
    BOOST_CHECK_EQUAL( tx.vin[0].prevout.hash.str(), "89df2e16bdc1fd00dffc72f24ec4da53ebb3ce1b08f55e7a9f874527b8714b9a" );
    BOOST_CHECK_EQUAL( tx.vin[0].prevout.n, 0 );

    BOOST_CHECK( fc::to_hex( tx.vin[0].scriptSig) == "473044022025f722981037f23949df7ac020e9895f6b4d35f98d04dadc43a8897d756b02f202203d4df0a81dac49be676657357f083d06f57b2d6b1199511ebdbd3bf82feff24101" );
    BOOST_CHECK_EQUAL( tx.vin[0].nSequence, 4294967294 );
    
    BOOST_REQUIRE_EQUAL( tx.vout.size(), 2 );
    BOOST_CHECK_EQUAL( tx.vout[0].value, 3500000000 );
    BOOST_CHECK( fc::to_hex( tx.vout[0].scriptPubKey ) == "76a914d377a5fd22419f8180f6d0d12215daffdd15b80088ac" );
    BOOST_CHECK_EQUAL(tx.vout[1].value, 1499996160);
    BOOST_CHECK( fc::to_hex( tx.vout[1].scriptPubKey ) == "76a914e71562730a2d7b2c2c7f2f137d6ddf80e8ee024288ac" );
}

BOOST_AUTO_TEST_CASE( btc_tx_methods_test ) {
    const auto tx = unpack( parse_hex( "0100000000010115e180dc28a2327e687facc33f10f2a20da717e5548406f7ae8b4c811072f856040000002322002001d5d92effa6ffba3efa379f9830d0f75618b13393827152d26e4309000e88b1ffffffff0188b3f505000000001976a9141d7cd6c75c2e86f4cbf98eaed221b30bd9a0b92888ac02473044022038421164c6468c63dc7bf724aa9d48d8e5abe3935564d38182addf733ad4cd81022076362326b22dd7bfaf211d5b17220723659e4fe3359740ced5762d0e497b7dcc012321038262a6c6cec93c2d3ecd6c6072efea86d02ff8e3328bbd0242b20af3425990acac00000000" ) );

    BOOST_CHECK( tx.get_hash().str() == "a5947589e2762107ff650958ba0e3a3cf341f53281d15593530bf9762c4edab1" );
    BOOST_CHECK( tx.get_txid().str() == "954f43dbb30ad8024981c07d1f5eb6c9fd461e2cf1760dd1283f052af746fc88" );
    BOOST_CHECK( tx.get_vsize() == 148 );
}

BOOST_AUTO_TEST_CASE( bitcoin_transaction_builder_test )
{
   // All tests are only to verefy the compilation of transactions, the transactions are not real(not valid)
   {   // P2PKH to P2PKH
       bitcoin_transaction_builder tb;
       tb.set_version( 2 );
       tb.add_in( payment_type::P2PKH, fc::sha256( "5d42b45d5a3ddcf2421b208885871121551acf6ea5cc1c1b4e666537ab6fcbef" ), 0, bytes() );
       tb.add_out( payment_type::P2PKH, 4999990000, "mkAn3ASzVBTLMbaLf2YTfcotdQ8hSbKD14" );

       const auto tx = tb.get_transaction();
       BOOST_CHECK( fc::to_hex(pack( tx ) ) == "0200000001efcb6fab3765664e1b1ccca56ecf1a552111878588201b42f2dc3d5a5db4425d0000000000ffffffff01f0ca052a010000001976a9143307bf6f98832e53a48b144d65c6a95700a93ffb88ac00000000");
   }
   {   // coinbase to P2PK
       const auto pubkey = fc::raw::unpack<fc::ecc::public_key_data>( parse_hex( "02028322f70f9bf4a014fb6422f555b05d605229460259c157b3fe34b7695f2d00" ) );
       out_point prevout;
       prevout.hash = fc::sha256( "0000000000000000000000000000000000000000000000000000000000000000" );
       prevout.n = 0xffffffff;

       tx_in txin;
       txin.prevout = prevout;

       bitcoin_transaction_builder tb;
       tb.set_version( 2 );
       tb.add_in( payment_type::P2SH, txin, parse_hex( "022a020101" ) );
       tb.add_out( payment_type::P2PK, 625000000, pubkey);
       tb.add_out( payment_type::NULLDATA, 0, parse_hex( "21030e7061b9fb18571cf2441b2a7ee2419933ddaa423bc178672cd11e87911616d1ac" ) );

       const auto tx = tb.get_transaction();
       BOOST_CHECK( fc::to_hex( pack( tx ) ) == "02000000010000000000000000000000000000000000000000000000000000000000000000ffffffff05022a020101ffffffff0240be402500000000232102028322f70f9bf4a014fb6422f555b05d605229460259c157b3fe34b7695f2d00ac0000000000000000256a2321030e7061b9fb18571cf2441b2a7ee2419933ddaa423bc178672cd11e87911616d1ac00000000" );
   }
   {   // P2SH to P2SH
       bitcoin_transaction_builder tb;
       tb.set_version( 2 );
       tb.add_in( payment_type::P2SH, fc::sha256( "40eee3ae1760e3a8532263678cdf64569e6ad06abc133af64f735e52562bccc8" ), 0, bytes() );
       tb.add_out( payment_type::P2SH, 0xffffffff, "3P14159f73E4gFr7JterCCQh9QjiTjiZrG" );

       const auto tx = tb.get_transaction();
       BOOST_CHECK( fc::to_hex( pack( tx ) ) == "0200000001c8cc2b56525e734ff63a13bc6ad06a9e5664df8c67632253a8e36017aee3ee400000000000ffffffff01ffffffff0000000017a914e9c3dd0c07aac76179ebc76a6c78d4d67c6c160a8700000000" );
   }
   {   // P2PK to NULLDATA
       bitcoin_transaction_builder tb;
       tb.set_version( 2 );
       tb.add_in( payment_type::P2PK, fc::sha256( "fa897a4a2b8bc507db6cf4425e81ca7ebde89a369e07d608ac7f7c311cb13b4f" ), 0, bytes() );
       tb.add_out( payment_type::NULLDATA, 0, parse_hex( "ffffffff" ) );

       const auto tx = tb.get_transaction();
       BOOST_CHECK( fc::to_hex( pack( tx ) ) == "02000000014f3bb11c317c7fac08d6079e369ae8bd7eca815e42f46cdb07c58b2b4a7a89fa0000000000ffffffff010000000000000000066a04ffffffff00000000" );
   }
   {   // P2PK+P2PKH to P2PKH,P2PKH       
       bitcoin_transaction_builder tb;
       tb.set_version(2);
       tb.add_in( payment_type::P2PK, fc::sha256( "13d29149e08b6ca63263f3dddd303b32f5aab646ebc6b7db84756d80a227f6d9" ), 0, bytes() );
       tb.add_in( payment_type::P2PKH, fc::sha256( "a8e7f661925cdd2c0e37fc93c03540c113aa6bcea02b35de09377127f76d0da3" ), 0, bytes() );
       tb.add_out( payment_type::P2PKH, 4999990000, "mzg9RZ1p29uNXu4uTWoMdMERdVXZpunJhW" );
       tb.add_out( payment_type::P2PKH, 4999990000, "n2SPW6abRxUnnTSSHp73VGahbPW4WT9GaK" );
       
       const auto tx = tb.get_transaction();        
       BOOST_CHECK( fc::to_hex( pack( tx ) ) == "0200000002d9f627a2806d7584dbb7c6eb46b6aaf5323b30ddddf36332a66c8be04991d2130000000000ffffffffa30d6df727713709de352ba0ce6baa13c14035c093fc370e2cdd5c9261f6e7a80000000000ffffffff02f0ca052a010000001976a914d2276c7ed7af07f697175cc2cbcbbf32da81caba88acf0ca052a010000001976a914e57d9a9af070998bedce991c4d8e39f9c51eb93a88ac00000000" );
   }
   {   // P2WPKH to P2WPKH
       bitcoin_transaction_builder tb;
       tb.set_version(2);
       tb.add_in( payment_type::P2WPKH, fc::sha256( "56f87210814c8baef7068454e517a70da2f2103fc3ac7f687e32a228dc80e115" ), 1, bytes() );
       tb.add_out( payment_type::P2WPKH, 99988480, "mkAn3ASzVBTLMbaLf2YTfcotdQ8hSbKD14" );

       const auto tx = tb.get_transaction();
       BOOST_CHECK( fc::to_hex( pack( tx ) ) == "020000000115e180dc28a2327e687facc33f10f2a20da717e5548406f7ae8b4c811072f8560100000000ffffffff0100b4f505000000001600143307bf6f98832e53a48b144d65c6a95700a93ffb00000000" );
   }
   {   // P2WSH to P2WSH
       bitcoin_transaction_builder tb;
       tb.set_version(2);
       tb.add_in( payment_type::P2WSH, fc::sha256( "56f87210814c8baef7068454e517a70da2f2103fc3ac7f687e32a228dc80e115"), 2, bytes() );
       tb.add_out( payment_type::P2WSH, 99988360, "p2xtZoXeX5X8BP8JfFhQK2nD3emtjch7UeFm" );

       const auto tx = tb.get_transaction();
       BOOST_CHECK( fc::to_hex( pack( tx ) ) == "020000000115e180dc28a2327e687facc33f10f2a20da717e5548406f7ae8b4c811072f8560200000000ffffffff0188b3f505000000001600140000010966776006953d5567439e5e39f86a0d2700000000" );
   }
   {   // P2SH(WPKH) to P2SH(WPKH)
       bitcoin_transaction_builder tb;
       tb.set_version( 2 );
       tb.add_in( payment_type::P2SH_WPKH, fc::sha256( "56f87210814c8baef7068454e517a70da2f2103fc3ac7f687e32a228dc80e115" ), 3, parse_hex( "ab68025513c3dbd2f7b92a94e0581f5d50f654e7" ) );
       tb.add_out( payment_type::P2SH_WPKH, 99987100, "3Mwz6cg8Fz81B7ukexK8u8EVAW2yymgWNd" );

       const auto tx = tb.get_transaction();
       BOOST_CHECK( fc::to_hex( pack( tx ) ) == "020000000115e180dc28a2327e687facc33f10f2a20da717e5548406f7ae8b4c811072f8560300000014ab68025513c3dbd2f7b92a94e0581f5d50f654e7ffffffff019caef5050000000017a914de373b053abb48ec078cf5f41b42aedac0103e278700000000" );
   }
   {   // P2SH(WSH) to P2SH(WSH)
       bitcoin_transaction_builder tb;
       tb.set_version(2);
       const auto redeem_script = parse_hex( "21038262a6c6cec93c2d3ecd6c6072efea86d02ff8e3328bbd0242b20af3425990acac");
       tb.add_in( payment_type::P2SH_WSH, fc::sha256( "fca01bd539623013f6f945dc6173c395394621ffaa53a9eb6da6e9a2e7c9400e" ), 0, redeem_script );
       tb.add_out( payment_type::P2SH_WSH, 99987100, "3Mwz6cg8Fz81B7ukexK8u8EVAW2yymgWNd" );

       const auto tx = tb.get_transaction();
       BOOST_CHECK( fc::to_hex( pack( tx ) ) == "02000000010e40c9e7a2e9a66deba953aaff21463995c37361dc45f9f613306239d51ba0fc000000002321038262a6c6cec93c2d3ecd6c6072efea86d02ff8e3328bbd0242b20af3425990acacffffffff019caef5050000000017a914de373b053abb48ec078cf5f41b42aedac0103e278700000000" );
   }
}

BOOST_AUTO_TEST_SUITE_END()
