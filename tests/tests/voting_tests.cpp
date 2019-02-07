/*
 * Copyright (c) 2018 oxarbitrage, and contributors.
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

#include <graphene/app/database_api.hpp>
#include <graphene/chain/exceptions.hpp>

#include <iostream>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;


BOOST_FIXTURE_TEST_SUITE(voting_tests, database_fixture)

BOOST_AUTO_TEST_CASE(last_voting_date)
{
   try
   {
      ACTORS((alice));

      transfer(committee_account, alice_id, asset(100));

      // we are going to vote for this witness
      auto witness1 = witness_id_type(1)(db);

      auto stats_obj = alice_id(db).statistics(db);
      BOOST_CHECK_EQUAL(stats_obj.last_vote_time.sec_since_epoch(), 0);

      // alice votes
      graphene::chain::account_update_operation op;
      op.account = alice_id;
      op.new_options = alice.options;
      op.new_options->votes.insert(witness1.vote_id);
      trx.operations.push_back(op);
      sign(trx, alice_private_key);
      PUSH_TX( db, trx, ~0 );

      auto now = db.head_block_time().sec_since_epoch();

      // last_vote_time is updated for alice
      stats_obj = alice_id(db).statistics(db);
      BOOST_CHECK_EQUAL(stats_obj.last_vote_time.sec_since_epoch(), now);

   } FC_LOG_AND_RETHROW()
}
BOOST_AUTO_TEST_CASE(last_voting_date_proxy)
{
   try
   {
      ACTORS((alice)(proxy)(bob));

      transfer(committee_account, alice_id, asset(100));
      transfer(committee_account, bob_id, asset(200));
      transfer(committee_account, proxy_id, asset(300));

      generate_block();

      // witness to vote for
      auto witness1 = witness_id_type(1)(db);

      // round1: alice changes proxy, this is voting activity
      {
         graphene::chain::account_update_operation op;
         op.account = alice_id;
         op.new_options = alice_id(db).options;
         op.new_options->voting_account = proxy_id;
         trx.operations.push_back(op);
         sign(trx, alice_private_key);
         PUSH_TX( db, trx, ~0 );
      }
      // alice last_vote_time is updated
      auto alice_stats_obj = alice_id(db).statistics(db);
      auto round1 = db.head_block_time().sec_since_epoch();
      BOOST_CHECK_EQUAL(alice_stats_obj.last_vote_time.sec_since_epoch(), round1);

      generate_block();

      // round 2: alice update account but no proxy or voting changes are done
      {
         graphene::chain::account_update_operation op;
         op.account = alice_id;
         op.new_options = alice_id(db).options;
         trx.operations.push_back(op);
         sign(trx, alice_private_key);
         set_expiration( db, trx );
         PUSH_TX( db, trx, ~0 );
      }
      // last_vote_time is not updated
      auto round2 = db.head_block_time().sec_since_epoch();
      alice_stats_obj = alice_id(db).statistics(db);
      BOOST_CHECK_EQUAL(alice_stats_obj.last_vote_time.sec_since_epoch(), round1);

      generate_block();

      // round 3: bob votes
      {
         graphene::chain::account_update_operation op;
         op.account = bob_id;
         op.new_options = bob_id(db).options;
         op.new_options->votes.insert(witness1.vote_id);
         trx.operations.push_back(op);
         sign(trx, bob_private_key);
         set_expiration( db, trx );
         PUSH_TX(db, trx, ~0);
      }

      // last_vote_time for bob is updated as he voted
      auto round3 = db.head_block_time().sec_since_epoch();
      auto bob_stats_obj = bob_id(db).statistics(db);
      BOOST_CHECK_EQUAL(bob_stats_obj.last_vote_time.sec_since_epoch(), round3);

      generate_block();

      // round 4: proxy votes
      {
         graphene::chain::account_update_operation op;
         op.account = proxy_id;
         op.new_options = proxy_id(db).options;
         op.new_options->votes.insert(witness1.vote_id);
         trx.operations.push_back(op);
         sign(trx, proxy_private_key);
         PUSH_TX(db, trx, ~0);
      }

      // proxy just voted so the last_vote_time is updated
      auto round4 = db.head_block_time().sec_since_epoch();
      auto proxy_stats_obj = proxy_id(db).statistics(db);
      BOOST_CHECK_EQUAL(proxy_stats_obj.last_vote_time.sec_since_epoch(), round4);

      // alice haves proxy, proxy votes but last_vote_time is not updated for alice
      alice_stats_obj = alice_id(db).statistics(db);
      BOOST_CHECK_EQUAL(alice_stats_obj.last_vote_time.sec_since_epoch(), round1);

      // bob haves nothing to do with proxy so last_vote_time is not updated
      bob_stats_obj = bob_id(db).statistics(db);
      BOOST_CHECK_EQUAL(bob_stats_obj.last_vote_time.sec_since_epoch(), round3);

   } FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()
