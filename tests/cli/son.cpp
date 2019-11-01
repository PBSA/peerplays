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
#include <fc/string.hpp>

#include <boost/test/unit_test.hpp>

class son_test_helper
{
    cli_fixture& fixture_;

public:
    son_test_helper(cli_fixture& fixture):
        fixture_(fixture)
    {
        fixture_.init_nathan();
    }

    void create_son(const std::string& account_name, const std::string& son_url,
                    bool generate_maintenance = true)
    {
        graphene::wallet::brain_key_info bki;
        signed_transaction create_tx;
        signed_transaction transfer_tx;
        signed_transaction upgrade_tx;
        account_object son_account;
        son_object son_obj;

        // create son account
        bki = fixture_.con.wallet_api_ptr->suggest_brain_key();
        BOOST_CHECK(!bki.brain_priv_key.empty());
        create_tx = fixture_.con.wallet_api_ptr->create_account_with_brain_key(
              bki.brain_priv_key, account_name, "nathan", "nathan", true
        );
        // save the private key for this new account in the wallet file
        BOOST_CHECK(fixture_.con.wallet_api_ptr->import_key(account_name, bki.wif_priv_key));
        fixture_.con.wallet_api_ptr->save_wallet_file(fixture_.con.wallet_filename);

        // attempt to give son account some CORE tokens
        BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to son account");
        transfer_tx = fixture_.con.wallet_api_ptr->transfer(
              "nathan", account_name, "65000", "1.3.0", "Here are some CORE token for your new account", true
        );

        BOOST_CHECK(fixture_.generate_block());

        // upgrade son account
        BOOST_TEST_MESSAGE("Upgrading son account to LTM");
        upgrade_tx = fixture_.con.wallet_api_ptr->upgrade_account(account_name, true);
        son_account = fixture_.con.wallet_api_ptr->get_account(account_name);

        // verify that the upgrade was successful
        BOOST_CHECK(son_account.is_lifetime_member());

        BOOST_CHECK(fixture_.generate_block());

        // create deposit vesting
        fixture_.con.wallet_api_ptr->create_vesting_balance(account_name,
                                                            "50", "1.3.0", vesting_balance_type::son, true);
        BOOST_CHECK(fixture_.generate_block());

        // create pay_vb vesting
        fixture_.con.wallet_api_ptr->create_vesting_balance(account_name, "1", "1.3.0", vesting_balance_type::normal, true);
        BOOST_CHECK(fixture_.generate_block());

        // check deposits are here
        auto deposits = fixture_.con.wallet_api_ptr->get_vesting_balances(account_name);
        BOOST_CHECK(deposits.size() == 2);

        create_tx = fixture_.con.wallet_api_ptr->create_son(account_name, son_url,
                                                            deposits[0].id, deposits[1].id,
                                                            true);

        if (generate_maintenance)
            BOOST_CHECK(fixture_.generate_maintenance_block());
    }

};

///////////////////////
// SON CLI
///////////////////////
BOOST_FIXTURE_TEST_SUITE(son_cli, cli_fixture)

BOOST_AUTO_TEST_CASE( create_sons )
{
   BOOST_TEST_MESSAGE("SON cli wallet tests begin");
   try
   {
      son_test_helper sth(*this);
      sth.create_son("son1account", "http://son1");
      sth.create_son("son2account", "http://son2");

      auto son1_obj = con.wallet_api_ptr->get_son("son1account");
      BOOST_CHECK(son1_obj.son_account == con.wallet_api_ptr->get_account_id("son1account"));
      BOOST_CHECK_EQUAL(son1_obj.url, "http://son1");

      auto son2_obj = con.wallet_api_ptr->get_son("son2account");
      BOOST_CHECK(son2_obj.son_account == con.wallet_api_ptr->get_account_id("son2account"));
      BOOST_CHECK_EQUAL(son2_obj.url, "http://son2");

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON cli wallet tests end");
}

BOOST_AUTO_TEST_CASE( cli_update_son )
{
   try
   {
      BOOST_TEST_MESSAGE("Cli get_son and update_son Test");

      son_test_helper sth(*this);
      sth.create_son("sonmember", "http://sonmember");

      auto sonmember_acct = con.wallet_api_ptr->get_account("sonmember");

      // get_son
      auto son_data = con.wallet_api_ptr->get_son("sonmember");
      BOOST_CHECK(son_data.url == "http://sonmember");
      BOOST_CHECK(son_data.son_account == sonmember_acct.get_id());

      // update SON
      con.wallet_api_ptr->update_son("sonmember", "http://sonmember_updated", "", true);
      son_data = con.wallet_api_ptr->get_son("sonmember");
      BOOST_CHECK(son_data.url == "http://sonmember_updated");

      // update SON signing key
      con.wallet_api_ptr->update_son("sonmember", "http://sonmember_updated2", "TEST6Yaq5ZNTTkMM2kBBzV5jktr8ETsniCC3bnVD7eFmegRrLXfGGG", true);
      son_data = con.wallet_api_ptr->get_son("sonmember");
      BOOST_CHECK(son_data.url == "http://sonmember_updated2");
      BOOST_CHECK(std::string(son_data.signing_key) == "TEST6Yaq5ZNTTkMM2kBBzV5jktr8ETsniCC3bnVD7eFmegRrLXfGGG");

   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE( son_voting )
{
   BOOST_TEST_MESSAGE("SON Vote cli wallet tests begin");
   try
   {
      son_test_helper sth(*this);
      sth.create_son("son1account", "http://son1");
      sth.create_son("son2account", "http://son2");

      BOOST_TEST_MESSAGE("Voting for SONs");

      son_object son1_obj;
      son_object son2_obj;
      signed_transaction vote_son1_tx;
      signed_transaction vote_son2_tx;
      int son1_start_votes, son1_end_votes;
      int son2_start_votes, son2_end_votes;

      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_start_votes = son1_obj.total_votes;
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_start_votes = son2_obj.total_votes;

      // Vote for a son1account
      BOOST_TEST_MESSAGE("Voting for son1account");
      vote_son1_tx = con.wallet_api_ptr->vote_for_son("nathan", "son1account", true, true);
      BOOST_CHECK(generate_maintenance_block());

      // Verify that the vote is there
      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_end_votes = son1_obj.total_votes;
      BOOST_CHECK(son1_end_votes > son1_start_votes);

      // Vote for a son2account
      BOOST_TEST_MESSAGE("Voting for son2account");
      vote_son2_tx = con.wallet_api_ptr->vote_for_son("nathan", "son2account", true, true);
      BOOST_CHECK(generate_maintenance_block());

      // Verify that the vote is there
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_end_votes = son2_obj.total_votes;
      BOOST_CHECK(son2_end_votes > son2_start_votes);

      // Withdraw vote for a son1account
      BOOST_TEST_MESSAGE("Withdraw vote for a son1account");
      vote_son1_tx = con.wallet_api_ptr->vote_for_son("nathan", "son1account", false, true);
      BOOST_CHECK(generate_maintenance_block());

      // Verify that the vote is removed
      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_end_votes = son1_obj.total_votes;
      BOOST_CHECK(son1_end_votes == son1_start_votes);

      // Withdraw vote for a son2account
      BOOST_TEST_MESSAGE("Withdraw vote for a son2account");
      vote_son2_tx = con.wallet_api_ptr->vote_for_son("nathan", "son2account", false, true);
      BOOST_CHECK(generate_maintenance_block());

      // Verify that the vote is removed
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_end_votes = son2_obj.total_votes;
      BOOST_CHECK(son2_end_votes == son2_start_votes);

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON Vote cli wallet tests end");
}

BOOST_AUTO_TEST_CASE( delete_son )
{
   BOOST_TEST_MESSAGE("SON delete cli wallet tests begin");
   try
   {
       son_test_helper sth(*this);
       sth.create_son("son1account", "http://son1");
       sth.create_son("son2account", "http://son2");

       BOOST_TEST_MESSAGE("Deleting SONs");
       signed_transaction delete_tx;
       auto _db = app1->chain_database();
       BOOST_CHECK(_db->get_index_type<son_index>().indices().size() == 2);
       delete_tx = con.wallet_api_ptr->delete_son("son1account", "true");
       delete_tx = con.wallet_api_ptr->delete_son("son2account", "true");
       BOOST_CHECK(generate_maintenance_block());
       BOOST_CHECK(_db->get_index_type<son_index>().indices().size() == 0);
   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON delete cli wallet tests end");
}


BOOST_FIXTURE_TEST_CASE( select_top_fifteen_sons, cli_fixture )
{
   BOOST_TEST_MESSAGE("SON cli wallet tests begin");
   try
   {
      son_test_helper sth(*this);

      graphene::wallet::brain_key_info bki;
      signed_transaction create_tx;
      signed_transaction transfer_tx;
      signed_transaction upgrade_tx;
      signed_transaction vote_tx;
      account_object acc_before_upgrade, acc_after_upgrade;
      son_object son_obj;
      global_property_object gpo;

      gpo = con.wallet_api_ptr->get_global_properties();
      unsigned int son_number = gpo.parameters.maximum_son_count;

      // create son accounts
      for(unsigned int i = 0; i < son_number + 1; i++)
      {
          sth.create_son("sonaccount" + fc::to_pretty_string(i),
                         "http://son" + fc::to_pretty_string(i), false);
      }
      BOOST_CHECK(generate_maintenance_block());

      BOOST_TEST_MESSAGE("Voting for SONs");
      for(unsigned int i = 0; i < son_number + 1; i++)
      {
          std::string name = "sonaccount" + fc::to_pretty_string(i);
          vote_tx = con.wallet_api_ptr->vote_for_son(name, name, true, true);
      }
      BOOST_CHECK(generate_maintenance_block());

      for(unsigned int i = 0; i < son_number; i++)
      {
          std::string name1 = "sonaccount" + fc::to_pretty_string(i);
          std::string name2 = "sonaccount" + fc::to_pretty_string(i + 1);
          vote_tx = con.wallet_api_ptr->vote_for_son(name1, name2, true, true);
      }
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo: " << gpo.active_sons.size());
      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo: " << gpo.active_sons.size());

      for(unsigned int i = 0; i < son_number - 1; i++)
      {
          std::string name1 = "sonaccount" + fc::to_pretty_string(i + 2);
          std::string name2 = "sonaccount" + fc::to_pretty_string(i);
          vote_tx = con.wallet_api_ptr->vote_for_son(name1, name2, true, true);
      }
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo: " << gpo.active_sons.size());
      BOOST_CHECK(generate_maintenance_block());
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo: " << gpo.active_sons.size());

      for(unsigned int i = 0; i < son_number - 2; i++)
      {
          std::string name1 = "sonaccount" + fc::to_pretty_string(i + 3);
          std::string name2 = "sonaccount" + fc::to_pretty_string(i);
          vote_tx = con.wallet_api_ptr->vote_for_son(name1, name2, true, true);
      }
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo: " << gpo.active_sons.size());
      BOOST_CHECK(generate_maintenance_block());

      BOOST_CHECK(gpo.active_sons.size() == 15);

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON cli wallet tests end");
}

BOOST_AUTO_TEST_CASE( list_son )
{
   BOOST_TEST_MESSAGE("List SONs cli wallet tests begin");
   try
   {
      son_test_helper sth(*this);
      sth.create_son("son1account", "http://son1");
      sth.create_son("son2account", "http://son2");

      auto res = con.wallet_api_ptr->list_sons("", 100);
      BOOST_REQUIRE(res.size() == 2);
      BOOST_CHECK(res.find("son1account") != res.end());
      BOOST_CHECK(res.find("son2account") != res.end());

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("List SONs cli wallet tests end");
}

BOOST_AUTO_TEST_CASE( update_son_votes_test )
{
    BOOST_TEST_MESSAGE("SON update_son_votes cli wallet tests begin");
    try
    {
       son_test_helper sth(*this);
       sth.create_son("son1account", "http://son1");
       sth.create_son("son2account", "http://son2");

       BOOST_TEST_MESSAGE("Vote for 2 accounts with update_son_votes");

       son_object son1_obj;
       son_object son2_obj;
       int son1_start_votes, son1_end_votes;
       int son2_start_votes, son2_end_votes;

       // Get votes at start
       son1_obj = con.wallet_api_ptr->get_son("son1account");
       son1_start_votes = son1_obj.total_votes;
       son2_obj = con.wallet_api_ptr->get_son("son2account");
       son2_start_votes = son2_obj.total_votes;

       std::vector<std::string> accepted;
       std::vector<std::string> rejected;
       signed_transaction update_votes_tx;

       // Vote for both SONs
       accepted.clear();
       rejected.clear();
       accepted.push_back("son1account");
       accepted.push_back("son2account");
       update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted,
                                                              rejected, 15, true);
       BOOST_CHECK(generate_block());
       BOOST_CHECK(generate_maintenance_block());

       // Verify the votes
       son1_obj = con.wallet_api_ptr->get_son("son1account");
       son1_end_votes = son1_obj.total_votes;
       BOOST_CHECK(son1_end_votes > son1_start_votes);
       son1_start_votes = son1_end_votes;
       son2_obj = con.wallet_api_ptr->get_son("son2account");
       son2_end_votes = son2_obj.total_votes;
       BOOST_CHECK(son2_end_votes > son2_start_votes);
       son2_start_votes = son2_end_votes;


       // Withdraw vote for SON 1
       accepted.clear();
       rejected.clear();
       rejected.push_back("son1account");
       update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted,
                                                              rejected, 15, true);
       BOOST_CHECK(generate_maintenance_block());

       // Verify the votes
       son1_obj = con.wallet_api_ptr->get_son("son1account");
       son1_end_votes = son1_obj.total_votes;
       BOOST_CHECK(son1_end_votes < son1_start_votes);
       son1_start_votes = son1_end_votes;
       son2_obj = con.wallet_api_ptr->get_son("son2account");
       // voice distribution changed, SON2 now has all voices
       son2_end_votes = son2_obj.total_votes;
       BOOST_CHECK(son2_end_votes > son2_start_votes);
       son2_start_votes = son2_end_votes;

       // Try to reject incorrect SON
       accepted.clear();
       rejected.clear();
       rejected.push_back("son1accnt");
       BOOST_CHECK_THROW(update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted,
                                                              rejected, 15, true), fc::exception);
       BOOST_CHECK(generate_block());

       // Verify the votes
       son1_obj = con.wallet_api_ptr->get_son("son1account");
       son1_end_votes = son1_obj.total_votes;
       BOOST_CHECK(son1_end_votes == son1_start_votes);
       son1_start_votes = son1_end_votes;
       son2_obj = con.wallet_api_ptr->get_son("son2account");
       son2_end_votes = son2_obj.total_votes;
       BOOST_CHECK(son2_end_votes == son2_start_votes);
       son2_start_votes = son2_end_votes;

       // Reject SON2
       accepted.clear();
       rejected.clear();
       rejected.push_back("son2account");
       update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted,
                                                              rejected, 15, true);
       BOOST_CHECK(generate_maintenance_block());

       // Verify the votes
       son1_obj = con.wallet_api_ptr->get_son("son1account");
       son1_end_votes = son1_obj.total_votes;
       BOOST_CHECK(son1_end_votes == son1_start_votes);
       son1_start_votes = son1_end_votes;
       son2_obj = con.wallet_api_ptr->get_son("son2account");
       son2_end_votes = son2_obj.total_votes;
       BOOST_CHECK(son2_end_votes < son2_start_votes);
       son2_start_votes = son2_end_votes;

       // Try to accept and reject the same SON
       accepted.clear();
       rejected.clear();
       rejected.push_back("son1accnt");
       accepted.push_back("son1accnt");
       BOOST_REQUIRE_THROW(update_votes_tx = con.wallet_api_ptr->update_son_votes("nathan", accepted,
                                                              rejected, 15, true), fc::exception);
       BOOST_CHECK(generate_maintenance_block());

       // Verify the votes
       son1_obj = con.wallet_api_ptr->get_son("son1account");
       son1_end_votes = son1_obj.total_votes;
       BOOST_CHECK(son1_end_votes == son1_start_votes);
       son1_start_votes = son1_end_votes;
       son2_obj = con.wallet_api_ptr->get_son("son2account");
       son2_end_votes = son2_obj.total_votes;
       BOOST_CHECK(son2_end_votes == son2_start_votes);
       son2_start_votes = son2_end_votes;

    } catch( fc::exception& e ) {
       BOOST_TEST_MESSAGE("SON cli wallet tests exception");
       edump((e.to_detail_string()));
       throw;
    }
    BOOST_TEST_MESSAGE("SON update_son_votes cli wallet tests end");
}

BOOST_AUTO_TEST_CASE( related_functions )
{
   BOOST_TEST_MESSAGE("SON-related functions cli wallet tests begin");
   try
   {
      global_property_object gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons.size() == 0);

      son_test_helper sth(*this);
      sth.create_son("son1account", "http://son1");
      sth.create_son("son2account", "http://son2");

      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_CHECK(gpo.active_sons.size() == 2);

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON-related functions cli wallet tests end");
}


BOOST_AUTO_TEST_SUITE_END()



