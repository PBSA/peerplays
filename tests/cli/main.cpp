/*
 * Copyright (c) 2019 PBSA, and contributors.
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
#include "cli_fixture.hpp"

#include <fc/smart_ref_impl.hpp>

#define BOOST_TEST_MODULE Test Application
#include <boost/test/included/unit_test.hpp>

///////////////////////////////
// Tests
///////////////////////////////

BOOST_FIXTURE_TEST_SUITE(cli_common, cli_fixture)

BOOST_AUTO_TEST_CASE( cli_connect )
{
   BOOST_TEST_MESSAGE("Testing wallet connection.");
}

BOOST_AUTO_TEST_CASE( upgrade_nathan_account )
{
    init_nathan();
}

BOOST_AUTO_TEST_CASE( create_new_account )
{
   try
   {
      init_nathan();

      // create a new account
      graphene::wallet::brain_key_info bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      signed_transaction create_acct_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "jmjatlanta", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("jmjatlanta", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give jmjatlanta some CORE
      BOOST_TEST_MESSAGE("Transferring CORE from Nathan to jmjatlanta");
      signed_transaction transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "jmjatlanta", "10000", "1.3.0", "Here are some CORE token for your new account", true
      );
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

///////////////////////
// Start a server and connect using the same calls as the CLI
// Vote for two witnesses, and make sure they both stay there
// after a maintenance block
///////////////////////
BOOST_AUTO_TEST_CASE( cli_vote_for_2_witnesses )
{
   try
   {
      BOOST_TEST_MESSAGE("Cli Vote Test for 2 Witnesses");

      init_nathan();

      // get the details for init1
      witness_object init1_obj = con.wallet_api_ptr->get_witness("init1");
      int init1_start_votes = init1_obj.total_votes;
      // Vote for a witness
      signed_transaction vote_witness1_tx = con.wallet_api_ptr->vote_for_witness("nathan", "init1", true, true);

      // generate a block to get things started
      BOOST_CHECK(generate_block());
      // wait for a maintenance interval
      BOOST_CHECK(generate_maintenance_block());

      // Verify that the vote is there
      init1_obj = con.wallet_api_ptr->get_witness("init1");
      witness_object init2_obj = con.wallet_api_ptr->get_witness("init2");
      int init1_middle_votes = init1_obj.total_votes;
      BOOST_CHECK(init1_middle_votes > init1_start_votes);

      // Vote for a 2nd witness
      int init2_start_votes = init2_obj.total_votes;
      signed_transaction vote_witness2_tx = con.wallet_api_ptr->vote_for_witness("nathan", "init2", true, true);

      // send another block to trigger maintenance interval
      BOOST_CHECK(generate_maintenance_block());

      // Verify that both the first vote and the 2nd are there
      init2_obj = con.wallet_api_ptr->get_witness("init2");
      init1_obj = con.wallet_api_ptr->get_witness("init1");

      int init2_middle_votes = init2_obj.total_votes;
      BOOST_CHECK(init2_middle_votes > init2_start_votes);
      int init1_last_votes = init1_obj.total_votes;
      BOOST_CHECK(init1_last_votes > init1_start_votes);
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

///////////////////////
// Check account history pagination
///////////////////////
BOOST_AUTO_TEST_CASE( account_history_pagination )
{
   try
   {
      INVOKE(create_new_account);

      // attempt to give jmjatlanta some peerplay
      BOOST_TEST_MESSAGE("Transferring peerplay from Nathan to jmjatlanta");
      for(int i = 1; i <= 199; i++)
      {
         signed_transaction transfer_tx = con.wallet_api_ptr->transfer("nathan", "jmjatlanta", std::to_string(i),
                                                "1.3.0", "Here are some CORE token for your new account", true);
      }

      BOOST_CHECK(generate_block());

      // now get account history and make sure everything is there (and no duplicates)
      std::vector<graphene::wallet::operation_detail> history = con.wallet_api_ptr->get_account_history("jmjatlanta", 300);
      BOOST_CHECK_EQUAL(201u, history.size() );

      std::set<object_id_type> operation_ids;

      for(auto& op : history)
      {
         if( operation_ids.find(op.op.id) != operation_ids.end() )
         {
            BOOST_FAIL("Duplicate found");
         }
         operation_ids.insert(op.op.id);
      }
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
