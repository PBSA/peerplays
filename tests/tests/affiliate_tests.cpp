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
#include <graphene/chain/protocol/account.hpp>
#include <graphene/chain/protocol/protocol.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/witness_scheduler_rng.hpp>
#include <graphene/chain/exceptions.hpp>

#include <graphene/db/simple_index.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/crypto/hash_ctr_rng.hpp>
#include "../common/database_fixture.hpp"

#include <algorithm>
#include <random>

using namespace graphene::chain;
using namespace graphene::db;

BOOST_FIXTURE_TEST_SUITE( affiliate_tests, database_fixture )

/**
 * Verify that names are RFC-1035 compliant https://tools.ietf.org/html/rfc1035
 * https://github.com/cryptonomex/graphene/issues/15
 */
BOOST_AUTO_TEST_CASE( account_test )
{
   ACTORS( (alice)(ann)(audrey) );
   fund( alice );

   const fc::ecc::private_key paula_private_key = generate_private_key( "paula" );

   account_create_operation aco = make_account( "paula", paula_private_key.get_public_key() );
   aco.extensions.value.affiliate_distributions = affiliate_reward_distributions();
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[alice_id] = GRAPHENE_100_PERCENT;
   test::set_expiration( db, trx );
   trx.clear();
   trx.operations.push_back( aco );
   // not allowed before hardfork
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   proposal_create_operation pco;
   pco.fee_paying_account = alice_id;
   aco.registrar = ann_id;
   pco.proposed_ops.emplace_back( aco );
   aco.registrar = account_id_type();
   pco.expiration_time = db.head_block_time() + fc::days(1);
   trx.clear();
   trx.operations.push_back( pco );
   // not allowed before hardfork
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   generate_blocks( HARDFORK_999_TIME );
   generate_block();

   // Proposal is now allowed
   pco.expiration_time = db.head_block_time() + fc::days(1);
   trx.clear();
   trx.operations.push_back( pco );
   test::set_expiration( db, trx );
   db.push_transaction(trx, ~0);

   // Must contain at least one app-tag
   aco.extensions.value.affiliate_distributions->_dists.clear();
   trx.clear();
   trx.operations.push_back( aco );
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   // Distribution for app-tag must be non-empty
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist.clear();
   trx.clear();
   trx.operations.push_back( aco );
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   // If more than one app-tag given, neither can be empty
   aco.extensions.value.affiliate_distributions->_dists[rps]._dist[alice_id] = GRAPHENE_100_PERCENT;
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist.clear();
   trx.clear();
   trx.operations.push_back( aco );
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   // Sum of percentage must be 100%
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[ann_id] = 1;
   trx.clear();
   trx.operations.push_back( aco );
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   // Individual percentages cannot exceed 100%
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[ann_id] = -1;
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[audrey_id] = 1 + GRAPHENE_100_PERCENT;
   trx.clear();
   trx.operations.push_back( aco );
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   // Sum of percentage must be 100%
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[ann_id] = GRAPHENE_100_PERCENT - 10;
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[audrey_id] = 9;
   trx.clear();
   trx.operations.push_back( aco );
   GRAPHENE_REQUIRE_THROW( db.push_transaction(trx, ~0), fc::assert_exception );

   // Ok
   aco.extensions.value.affiliate_distributions->_dists[bookie]._dist[audrey_id] = 10;
   trx.clear();
   trx.operations.push_back( aco );
   db.push_transaction(trx, ~0);

   const auto& paula = get_account( aco.name );
   BOOST_CHECK( paula.affiliate_distributions.valid() );
   BOOST_CHECK_EQUAL( 2, paula.affiliate_distributions->_dists.size() );

   const auto& app_rps = paula.affiliate_distributions->_dists.find( rps );
   BOOST_CHECK( app_rps != paula.affiliate_distributions->_dists.end() );
   BOOST_CHECK_EQUAL( 1, app_rps->second._dist.size() );
   const auto& share_alice = app_rps->second._dist.find( alice_id );
   BOOST_CHECK( share_alice != app_rps->second._dist.end() );
   BOOST_CHECK_EQUAL( GRAPHENE_100_PERCENT, share_alice->second );

   const auto& app_bookie = paula.affiliate_distributions->_dists.find( bookie );
   BOOST_CHECK( app_bookie != paula.affiliate_distributions->_dists.end() );
   BOOST_CHECK_EQUAL( 2, app_bookie->second._dist.size() );
   const auto& share_ann = app_bookie->second._dist.find( ann_id );
   BOOST_CHECK( share_ann != app_bookie->second._dist.end() );
   BOOST_CHECK_EQUAL( GRAPHENE_100_PERCENT - 10, share_ann->second );
   const auto& share_audrey = app_bookie->second._dist.find( audrey_id );
   BOOST_CHECK( share_audrey != app_bookie->second._dist.end() );
   BOOST_CHECK_EQUAL( 10, share_audrey->second );
}

BOOST_AUTO_TEST_SUITE_END()
