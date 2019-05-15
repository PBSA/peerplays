/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>


#include <fc_pp/crypto/digest.hpp>
#include <fc_pp/crypto/elliptic.hpp>
#include <fc_pp/reflect/variant.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;

BOOST_FIXTURE_TEST_SUITE( operation_unit_tests, database_fixture )

BOOST_AUTO_TEST_CASE( serialization_raw_test )
{
   try {
      make_account();
      transfer_operation op;
      op.from = account_id_type(1);
      op.to = account_id_type(2);
      op.amount = asset(100);
      trx.operations.push_back( op );
      auto packed = fc_pp::raw::pack( trx );
      signed_transaction unpacked = fc_pp::raw::unpack<signed_transaction>(packed);
      unpacked.validate();
      BOOST_CHECK( digest(trx) == digest(unpacked) );
   } catch (fc_pp::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}
BOOST_AUTO_TEST_CASE( serialization_json_test )
{
   try {
      make_account();
      transfer_operation op;
      op.from = account_id_type(1);
      op.to = account_id_type(2);
      op.amount = asset(100);
      trx.operations.push_back( op );
      fc_pp::variant packed(trx);
      signed_transaction unpacked = packed.as<signed_transaction>();
      unpacked.validate();
      BOOST_CHECK( digest(trx) == digest(unpacked) );
   } catch (fc_pp::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( json_tests )
{
   try {
   auto var = fc_pp::json::variants_from_string( "10.6 " );
   var = fc_pp::json::variants_from_string( "10.5" );
   } catch ( const fc_pp::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( extended_private_key_type_test )
{
   try
   {
     fc_pp::ecc::extended_private_key key = fc_pp::ecc::extended_private_key( fc_pp::ecc::private_key::generate(),
                                                                       fc_pp::sha256(),
                                                                       0, 0, 0 );
      extended_private_key_type type = extended_private_key_type( key );
      std::string packed = std::string( type );
      extended_private_key_type unpacked = extended_private_key_type( packed );
      BOOST_CHECK( type == unpacked );
   } catch ( const fc_pp::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( extended_public_key_type_test )
{
   try
   {
      fc_pp::ecc::extended_public_key key = fc_pp::ecc::extended_public_key( fc_pp::ecc::private_key::generate().get_public_key(),
                                                                       fc_pp::sha256(),
                                                                       0, 0, 0 );
      extended_public_key_type type = extended_public_key_type( key );
      std::string packed = std::string( type );
      extended_public_key_type unpacked = extended_public_key_type( packed );
      BOOST_CHECK( type == unpacked );
   } catch ( const fc_pp::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( extension_serialization_test )
{
   try
   {
      buyback_account_options bbo;
      bbo.asset_to_buy = asset_id_type(1000);
      bbo.asset_to_buy_issuer = account_id_type(2000);
      bbo.markets.emplace( asset_id_type() );
      bbo.markets.emplace( asset_id_type(777) );
      account_create_operation create_op = make_account( "rex" );
      create_op.registrar = account_id_type(1234);
      create_op.extensions.value.buyback_options = bbo;

      auto packed = fc_pp::raw::pack( create_op );
      account_create_operation unpacked = fc_pp::raw::unpack<account_create_operation>(packed);

      ilog( "original: ${x}", ("x", create_op) );
      ilog( "unpacked: ${x}", ("x", unpacked) );
   }
   catch ( const fc_pp::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
