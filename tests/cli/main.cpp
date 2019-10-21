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
#include <graphene/app/application.hpp>
#include <graphene/app/plugin.hpp>
#include <graphene/app/api.hpp>

#include <graphene/utilities/tempdir.hpp>
#include <graphene/bookie/bookie_plugin.hpp>
#include <graphene/witness/witness.hpp>
#include <graphene/account_history/account_history_plugin.hpp>
#include <graphene/egenesis/egenesis.hpp>
#include <graphene/wallet/wallet.hpp>
#include <graphene/chain/config.hpp>

#include <fc/thread/thread.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/crypto/base58.hpp>

#include <fc/crypto/aes.hpp>
#include <fc/smart_ref_impl.hpp>

#ifdef _WIN32
#ifndef _WIN32_WINNT
      #define _WIN32_WINNT 0x0501
   #endif
   #include <winsock2.h>
   #include <WS2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/ip.h>
#include <sys/types.h>
#endif
#include <thread>

#include <boost/filesystem/path.hpp>

#define BOOST_TEST_MODULE Test Application
#include <boost/test/included/unit_test.hpp>

/*****
 * Global Initialization for Windows
 * ( sets up Winsock stuf )
 */
#ifdef _WIN32
int sockInit(void)
{
   WSADATA wsa_data;
   return WSAStartup(MAKEWORD(1,1), &wsa_data);
}
int sockQuit(void)
{
   return WSACleanup();
}
#endif

/*********************
 * Helper Methods
 *********************/

#include "../common/genesis_file_util.hpp"

#define INVOKE(test) ((struct test*)this)->test_method();

//////
/// @brief attempt to find an available port on localhost
/// @returns an available port number, or -1 on error
/////
int get_available_port()
{
   struct sockaddr_in sin;
   int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
   if (socket_fd == -1)
      return -1;
   sin.sin_family = AF_INET;
   sin.sin_port = 0;
   sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
   if (::bind(socket_fd, (struct sockaddr*)&sin, sizeof(struct sockaddr_in)) == -1)
      return -1;
   socklen_t len = sizeof(sin);
   if (getsockname(socket_fd, (struct sockaddr *)&sin, &len) == -1)
      return -1;
#ifdef _WIN32
   closesocket(socket_fd);
#else
   close(socket_fd);
#endif
   return ntohs(sin.sin_port);
}

///////////
/// @brief Start the application
/// @param app_dir the temporary directory to use
/// @param server_port_number to be filled with the rpc endpoint port number
/// @returns the application object
//////////
std::shared_ptr<graphene::app::application> start_application(fc::temp_directory& app_dir, int& server_port_number) {
   std::shared_ptr<graphene::app::application> app1(new graphene::app::application{});

   app1->register_plugin<graphene::bookie::bookie_plugin>();
   app1->register_plugin<graphene::account_history::account_history_plugin>();
   app1->register_plugin<graphene::market_history::market_history_plugin>();
   app1->register_plugin<graphene::witness_plugin::witness_plugin>();
   app1->startup_plugins();
   boost::program_options::variables_map cfg;
#ifdef _WIN32
   sockInit();
#endif
   server_port_number = get_available_port();
   cfg.emplace(
         "rpc-endpoint",
         boost::program_options::variable_value(string("127.0.0.1:" + std::to_string(server_port_number)), false)
   );
   cfg.emplace("genesis-json", boost::program_options::variable_value(create_genesis_file(app_dir), false));
   cfg.emplace("seed-nodes", boost::program_options::variable_value(string("[]"), false));
   cfg.emplace("plugins", boost::program_options::variable_value(string("bookie account_history market_history"), false));

   app1->initialize(app_dir.path(), cfg);

   app1->initialize_plugins(cfg);
   app1->startup_plugins();

   app1->startup();
   fc::usleep(fc::milliseconds(500));
   return app1;
}

///////////
/// Send a block to the db
/// @param app the application
/// @param returned_block the signed block
/// @returns true on success
///////////
bool generate_block(std::shared_ptr<graphene::app::application> app, graphene::chain::signed_block& returned_block)
{
   try {
      fc::ecc::private_key committee_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
      auto db = app->chain_database();
      returned_block = db->generate_block( db->get_slot_time(1),
                                           db->get_scheduled_witness(1),
                                           committee_key,
                                           database::skip_nothing );
      return true;
   } catch (exception &e) {
      return false;
   }
}

bool generate_block(std::shared_ptr<graphene::app::application> app)
{
   graphene::chain::signed_block returned_block;
   return generate_block(app, returned_block);
}

///////////
/// @brief Skip intermediate blocks, and generate a maintenance block
/// @param app the application
/// @returns true on success
///////////
bool generate_maintenance_block(std::shared_ptr<graphene::app::application> app) {
   try {
      fc::ecc::private_key committee_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
      uint32_t skip = ~0;
      auto db = app->chain_database();
      auto maint_time = db->get_dynamic_global_properties().next_maintenance_time;
      auto slots_to_miss = db->get_slot_at_time(maint_time);
      db->generate_block(db->get_slot_time(slots_to_miss),
                         db->get_scheduled_witness(slots_to_miss),
                         committee_key,
                         skip);
      return true;
   } catch (exception& e)
   {
      return false;
   }
}

///////////
/// @brief a class to make connecting to the application server easier
///////////
class client_connection
{
public:
   /////////
   // constructor
   /////////
   client_connection(
         std::shared_ptr<graphene::app::application> app,
         const fc::temp_directory& data_dir,
         const int server_port_number
   )
   {
      wallet_data.chain_id = app->chain_database()->get_chain_id();
      wallet_data.ws_server = "ws://127.0.0.1:" + std::to_string(server_port_number);
      wallet_data.ws_user = "";
      wallet_data.ws_password = "";
      websocket_connection  = websocket_client.connect( wallet_data.ws_server );

      api_connection = std::make_shared<fc::rpc::websocket_api_connection>(websocket_connection, GRAPHENE_MAX_NESTED_OBJECTS);

      remote_login_api = api_connection->get_remote_api< graphene::app::login_api >(1);
      BOOST_CHECK(remote_login_api->login( wallet_data.ws_user, wallet_data.ws_password ) );

      wallet_api_ptr = std::make_shared<graphene::wallet::wallet_api>(wallet_data, remote_login_api);
      wallet_filename = data_dir.path().generic_string() + "/wallet.json";
      wallet_api_ptr->set_wallet_filename(wallet_filename);

      wallet_api = fc::api<graphene::wallet::wallet_api>(wallet_api_ptr);

      wallet_cli = std::make_shared<fc::rpc::cli>(GRAPHENE_MAX_NESTED_OBJECTS);
      for( auto& name_formatter : wallet_api_ptr->get_result_formatters() )
         wallet_cli->format_result( name_formatter.first, name_formatter.second );

      boost::signals2::scoped_connection closed_connection(websocket_connection->closed.connect([=]{
         cerr << "Server has disconnected us.\n";
         wallet_cli->stop();
      }));
      (void)(closed_connection);
   }
   ~client_connection()
   {
      // wait for everything to finish up
      fc::usleep(fc::milliseconds(500));
   }
public:
   fc::http::websocket_client websocket_client;
   graphene::wallet::wallet_data wallet_data;
   fc::http::websocket_connection_ptr websocket_connection;
   std::shared_ptr<fc::rpc::websocket_api_connection> api_connection;
   fc::api<login_api> remote_login_api;
   std::shared_ptr<graphene::wallet::wallet_api> wallet_api_ptr;
   fc::api<graphene::wallet::wallet_api> wallet_api;
   std::shared_ptr<fc::rpc::cli> wallet_cli;
   std::string wallet_filename;
};


///////////////////////////////
// Cli Wallet Fixture
///////////////////////////////

struct cli_fixture
{
   class dummy
   {
   public:
      ~dummy()
      {
         // wait for everything to finish up
         fc::usleep(fc::milliseconds(500));
      }
   };
   dummy dmy;
   int server_port_number;
   fc::temp_directory app_dir;
   std::shared_ptr<graphene::app::application> app1;
   client_connection con;
   std::vector<std::string> nathan_keys;

   cli_fixture() :
         server_port_number(0),
         app_dir( graphene::utilities::temp_directory_path() ),
         app1( start_application(app_dir, server_port_number) ),
         con( app1, app_dir, server_port_number ),
         nathan_keys( {"5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"} )
   {
      BOOST_TEST_MESSAGE("Setup cli_wallet::boost_fixture_test_case");

      using namespace graphene::chain;
      using namespace graphene::app;

      try
      {
         BOOST_TEST_MESSAGE("Setting wallet password");
         con.wallet_api_ptr->set_password("supersecret");
         con.wallet_api_ptr->unlock("supersecret");

         // import Nathan account
         BOOST_TEST_MESSAGE("Importing nathan key");
         BOOST_CHECK_EQUAL(nathan_keys[0], "5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3");
         BOOST_CHECK(con.wallet_api_ptr->import_key("nathan", nathan_keys[0]));
      } catch( fc::exception& e ) {
         edump((e.to_detail_string()));
         throw;
      }
   }

   ~cli_fixture()
   {
      BOOST_TEST_MESSAGE("Cleanup cli_wallet::boost_fixture_test_case");

      // wait for everything to finish up
      fc::usleep(fc::seconds(1));

      app1->shutdown();
#ifdef _WIN32
      sockQuit();
#endif
   }
};

///////////////////////////////
// Tests
///////////////////////////////

////////////////
// Start a server and connect using the same calls as the CLI
////////////////
BOOST_FIXTURE_TEST_CASE( cli_connect, cli_fixture )
{
   BOOST_TEST_MESSAGE("Testing wallet connection.");
}

BOOST_FIXTURE_TEST_CASE( upgrade_nathan_account, cli_fixture )
{
   try
   {
      BOOST_TEST_MESSAGE("Upgrade Nathan's account");

      account_object nathan_acct_before_upgrade, nathan_acct_after_upgrade;
      std::vector<signed_transaction> import_txs;
      signed_transaction upgrade_tx;

      BOOST_TEST_MESSAGE("Importing nathan's balance");
      import_txs = con.wallet_api_ptr->import_balance("nathan", nathan_keys, true);
      nathan_acct_before_upgrade = con.wallet_api_ptr->get_account("nathan");

      BOOST_CHECK(generate_block(app1));

      // upgrade nathan
      BOOST_TEST_MESSAGE("Upgrading Nathan to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("nathan", true);

      nathan_acct_after_upgrade = con.wallet_api_ptr->get_account("nathan");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (nathan_acct_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (nathan_acct_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(nathan_acct_after_upgrade.is_lifetime_member());
   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_FIXTURE_TEST_CASE( create_new_account, cli_fixture )
{
   try
   {
      INVOKE(upgrade_nathan_account);

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
BOOST_FIXTURE_TEST_CASE( cli_vote_for_2_witnesses, cli_fixture )
{
   try
   {
      BOOST_TEST_MESSAGE("Cli Vote Test for 2 Witnesses");

      INVOKE(upgrade_nathan_account); // just to fund nathan

      // get the details for init1
      witness_object init1_obj = con.wallet_api_ptr->get_witness("init1");
      int init1_start_votes = init1_obj.total_votes;
      // Vote for a witness
      signed_transaction vote_witness1_tx = con.wallet_api_ptr->vote_for_witness("nathan", "init1", true, true);

      // generate a block to get things started
      BOOST_CHECK(generate_block(app1));
      // wait for a maintenance interval
      BOOST_CHECK(generate_maintenance_block(app1));

      // Verify that the vote is there
      init1_obj = con.wallet_api_ptr->get_witness("init1");
      witness_object init2_obj = con.wallet_api_ptr->get_witness("init2");
      int init1_middle_votes = init1_obj.total_votes;
      BOOST_CHECK(init1_middle_votes > init1_start_votes);

      // Vote for a 2nd witness
      int init2_start_votes = init2_obj.total_votes;
      signed_transaction vote_witness2_tx = con.wallet_api_ptr->vote_for_witness("nathan", "init2", true, true);

      // send another block to trigger maintenance interval
      BOOST_CHECK(generate_maintenance_block(app1));

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
// SON CLI
///////////////////////
BOOST_FIXTURE_TEST_CASE( create_son, cli_fixture )
{
   BOOST_TEST_MESSAGE("SON cli wallet tests begin");
   try
   {
      INVOKE(upgrade_nathan_account);

      graphene::wallet::brain_key_info bki;
      signed_transaction create_tx;
      signed_transaction transfer_tx;
      signed_transaction upgrade_tx;
      signed_transaction delete_tx;
      account_object son1_before_upgrade, son1_after_upgrade;
      account_object son2_before_upgrade, son2_after_upgrade;
      son_object son1_obj;
      son_object son2_obj;

      // create son1account
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "son1account", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("son1account", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give son1account some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to son1account");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "son1account", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      son1_before_upgrade = con.wallet_api_ptr->get_account("son1account");
      BOOST_CHECK(generate_block(app1));

      // upgrade son1account
      BOOST_TEST_MESSAGE("Upgrading son1account to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("son1account", true);
      son1_after_upgrade = con.wallet_api_ptr->get_account("son1account");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (son1_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (son1_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(son1_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create son2account
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "son2account", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("son2account", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give son1account some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to son2account");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "son2account", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      son2_before_upgrade = con.wallet_api_ptr->get_account("son2account");
      BOOST_CHECK(generate_block(app1));

      // upgrade son1account
      BOOST_TEST_MESSAGE("Upgrading son2account to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("son2account", true);
      son2_after_upgrade = con.wallet_api_ptr->get_account("son2account");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (son2_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (son2_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(son2_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create 2 SONs
      BOOST_TEST_MESSAGE("Creating two SONs");
      create_tx = con.wallet_api_ptr->create_son("son1account", "http://son1", "true");
      create_tx = con.wallet_api_ptr->create_son("son2account", "http://son2", "true");
      BOOST_CHECK(generate_maintenance_block(app1));

      son1_obj = con.wallet_api_ptr->get_son("son1account");
      BOOST_CHECK(son1_obj.son_account == con.wallet_api_ptr->get_account_id("son1account"));
      BOOST_CHECK_EQUAL(son1_obj.url, "http://son1");

      son2_obj = con.wallet_api_ptr->get_son("son2account");
      BOOST_CHECK(son2_obj.son_account == con.wallet_api_ptr->get_account_id("son2account"));
      BOOST_CHECK_EQUAL(son2_obj.url, "http://son2");



      BOOST_TEST_MESSAGE("Voting for SONs");

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
      BOOST_CHECK(generate_maintenance_block(app1));

      // Verify that the vote is there
      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_end_votes = son1_obj.total_votes;
      BOOST_CHECK(son1_end_votes > son1_start_votes);

      // Vote for a son2account
      BOOST_TEST_MESSAGE("Voting for son2account");
      vote_son2_tx = con.wallet_api_ptr->vote_for_son("nathan", "son2account", true, true);
      BOOST_CHECK(generate_maintenance_block(app1));

      // Verify that the vote is there
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_end_votes = son2_obj.total_votes;
      BOOST_CHECK(son2_end_votes > son2_start_votes);

      // Withdraw vote for a son1account
      BOOST_TEST_MESSAGE("Withdraw vote for a son1account");
      vote_son1_tx = con.wallet_api_ptr->vote_for_son("nathan", "son1account", false, true);
      BOOST_CHECK(generate_maintenance_block(app1));

      // Verify that the vote is removed
      son1_obj = con.wallet_api_ptr->get_son("son1account");
      son1_end_votes = son1_obj.total_votes;
      BOOST_CHECK(son1_end_votes == son1_start_votes);

      // Withdraw vote for a son2account
      BOOST_TEST_MESSAGE("Withdraw vote for a son2account");
      vote_son2_tx = con.wallet_api_ptr->vote_for_son("nathan", "son2account", false, true);
      BOOST_CHECK(generate_maintenance_block(app1));

      // Verify that the vote is removed
      son2_obj = con.wallet_api_ptr->get_son("son2account");
      son2_end_votes = son2_obj.total_votes;
      BOOST_CHECK(son2_end_votes == son2_start_votes);



      BOOST_TEST_MESSAGE("Deleting SONs");
      auto _db = app1->chain_database();
      BOOST_CHECK(_db->get_index_type<son_index>().indices().size() == 2);
      delete_tx = con.wallet_api_ptr->delete_son("son1account", "true");
      delete_tx = con.wallet_api_ptr->delete_son("son2account", "true");
      BOOST_CHECK(generate_maintenance_block(app1));
      BOOST_CHECK(_db->get_index_type<son_index>().indices().size() == 0);

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON cli wallet tests end");
}

BOOST_FIXTURE_TEST_CASE( select_top_fifteen_sons, cli_fixture )
{
   BOOST_TEST_MESSAGE("SON cli wallet tests begin");
   try
   {
      INVOKE(upgrade_nathan_account);

      graphene::wallet::brain_key_info bki;
      signed_transaction create_tx;
      signed_transaction transfer_tx;
      signed_transaction upgrade_tx;
      signed_transaction vote_tx;
      account_object acc_before_upgrade, acc_after_upgrade;
      son_object son_obj;
      global_property_object gpo;

      // create sonaccount01
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount01", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount01", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount01 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount01");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount01", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount01");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount01
      BOOST_TEST_MESSAGE("Upgrading sonaccount01 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount01", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount01");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount02
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount02", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount02", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount02 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount02");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount02", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount02");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount02
      BOOST_TEST_MESSAGE("Upgrading sonaccount02 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount02", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount02");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount03
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount03", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount03", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount03 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount03");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount03", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount03");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount03
      BOOST_TEST_MESSAGE("Upgrading sonaccount03 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount03", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount03");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount04
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount04", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount04", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount04 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount04");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount04", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount04");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount04
      BOOST_TEST_MESSAGE("Upgrading sonaccount04 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount04", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount04");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount05
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount05", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount05", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount05 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount05");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount05", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount05");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount05
      BOOST_TEST_MESSAGE("Upgrading sonaccount05 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount05", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount05");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount06
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount06", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount06", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount06 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount06");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount06", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount06");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount06
      BOOST_TEST_MESSAGE("Upgrading sonaccount06 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount06", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount06");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount07
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount07", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount07", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount07 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount07");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount07", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount07");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount07
      BOOST_TEST_MESSAGE("Upgrading sonaccount07 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount07", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount07");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount08
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount08", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount08", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount08 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount08");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount08", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount08");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount08
      BOOST_TEST_MESSAGE("Upgrading sonaccount08 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount08", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount08");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount09
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount09", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount09", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount09 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount09");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount09", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount09");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount09
      BOOST_TEST_MESSAGE("Upgrading sonaccount09 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount09", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount09");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount10
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount10", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount10", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount10 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount10");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount10", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount10");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount10
      BOOST_TEST_MESSAGE("Upgrading sonaccount10 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount10", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount10");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount11
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount11", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount11", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount11 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount11");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount11", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount11");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount11
      BOOST_TEST_MESSAGE("Upgrading sonaccount11 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount11", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount11");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount12
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount12", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount12", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount12 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount12");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount12", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount12");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount12
      BOOST_TEST_MESSAGE("Upgrading sonaccount12 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount12", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount12");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount13
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount13", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount13", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount13 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount13");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount13", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount13");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount13
      BOOST_TEST_MESSAGE("Upgrading sonaccount13 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount13", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount13");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount14
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount14", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount14", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount14 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount14");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount14", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount14");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount14
      BOOST_TEST_MESSAGE("Upgrading sonaccount14 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount14", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount14");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount15
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount15", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount15", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount15 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount15");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount15", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount15");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount15
      BOOST_TEST_MESSAGE("Upgrading sonaccount15 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount15", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount15");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create sonaccount16
      bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      create_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonaccount16", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonaccount16", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonaccount16 some CORE tokens
      BOOST_TEST_MESSAGE("Transferring CORE tokens from Nathan to sonaccount16");
      transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonaccount16", "15000", "1.3.0", "Here are some CORE token for your new account", true
      );

      acc_before_upgrade = con.wallet_api_ptr->get_account("sonaccount16");
      BOOST_CHECK(generate_block(app1));

      // upgrade sonaccount16
      BOOST_TEST_MESSAGE("Upgrading sonaccount16 to LTM");
      upgrade_tx = con.wallet_api_ptr->upgrade_account("sonaccount16", true);
      acc_after_upgrade = con.wallet_api_ptr->get_account("sonaccount16");

      // verify that the upgrade was successful
      BOOST_CHECK_PREDICATE(
            std::not_equal_to<uint32_t>(),
            (acc_before_upgrade.membership_expiration_date.sec_since_epoch())
                  (acc_after_upgrade.membership_expiration_date.sec_since_epoch())
      );
      BOOST_CHECK(acc_after_upgrade.is_lifetime_member());

      BOOST_CHECK(generate_block(app1));



      // create 16 SONs
      BOOST_TEST_MESSAGE("Creating 16 SONs");
      create_tx = con.wallet_api_ptr->create_son("sonaccount01", "http://son01", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount02", "http://son02", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount03", "http://son03", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount04", "http://son04", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount05", "http://son05", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount06", "http://son06", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount07", "http://son07", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount08", "http://son08", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount09", "http://son09", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount10", "http://son10", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount11", "http://son11", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount12", "http://son12", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount13", "http://son13", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount14", "http://son14", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount15", "http://son15", "true");
      create_tx = con.wallet_api_ptr->create_son("sonaccount16", "http://son16", "true");
      BOOST_CHECK(generate_maintenance_block(app1));

      son_obj = con.wallet_api_ptr->get_son("sonaccount01");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount01"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son01");
      son_obj = con.wallet_api_ptr->get_son("sonaccount02");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount02"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son02");
      son_obj = con.wallet_api_ptr->get_son("sonaccount03");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount03"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son03");
      son_obj = con.wallet_api_ptr->get_son("sonaccount04");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount04"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son04");
      son_obj = con.wallet_api_ptr->get_son("sonaccount05");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount05"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son05");
      son_obj = con.wallet_api_ptr->get_son("sonaccount06");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount06"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son06");
      son_obj = con.wallet_api_ptr->get_son("sonaccount07");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount07"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son07");
      son_obj = con.wallet_api_ptr->get_son("sonaccount08");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount08"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son08");
      son_obj = con.wallet_api_ptr->get_son("sonaccount09");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount09"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son09");
      son_obj = con.wallet_api_ptr->get_son("sonaccount10");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount10"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son10");
      son_obj = con.wallet_api_ptr->get_son("sonaccount11");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount11"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son11");
      son_obj = con.wallet_api_ptr->get_son("sonaccount12");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount12"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son12");
      son_obj = con.wallet_api_ptr->get_son("sonaccount13");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount13"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son13");
      son_obj = con.wallet_api_ptr->get_son("sonaccount14");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount14"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son14");
      son_obj = con.wallet_api_ptr->get_son("sonaccount15");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount15"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son15");
      son_obj = con.wallet_api_ptr->get_son("sonaccount16");
      BOOST_CHECK(son_obj.son_account == con.wallet_api_ptr->get_account_id("sonaccount16"));
      BOOST_CHECK_EQUAL(son_obj.url, "http://son16");



      BOOST_TEST_MESSAGE("Voting for SONs");
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount01", "sonaccount01", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount02", "sonaccount02", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount03", "sonaccount03", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount04", "sonaccount04", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount05", "sonaccount05", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount06", "sonaccount06", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount07", "sonaccount07", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount08", "sonaccount08", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount09", "sonaccount09", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount10", "sonaccount10", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount11", "sonaccount11", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount12", "sonaccount12", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount13", "sonaccount13", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount14", "sonaccount14", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount15", "sonaccount15", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount16", "sonaccount16", true, true);
      BOOST_CHECK(generate_maintenance_block(app1));
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount01", "sonaccount02", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount02", "sonaccount03", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount03", "sonaccount04", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount04", "sonaccount05", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount05", "sonaccount06", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount06", "sonaccount07", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount07", "sonaccount08", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount08", "sonaccount09", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount09", "sonaccount10", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount10", "sonaccount11", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount11", "sonaccount12", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount12", "sonaccount13", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount13", "sonaccount14", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount14", "sonaccount15", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount15", "sonaccount16", true, true);
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo: " << gpo.active_sons.size());
      BOOST_CHECK(generate_maintenance_block(app1));
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo: " << gpo.active_sons.size());
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount03", "sonaccount01", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount04", "sonaccount02", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount05", "sonaccount03", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount06", "sonaccount04", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount07", "sonaccount05", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount08", "sonaccount06", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount09", "sonaccount07", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount10", "sonaccount08", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount11", "sonaccount09", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount12", "sonaccount10", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount13", "sonaccount11", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount14", "sonaccount12", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount15", "sonaccount13", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount16", "sonaccount14", true, true);
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo: " << gpo.active_sons.size());
      BOOST_CHECK(generate_maintenance_block(app1));
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo: " << gpo.active_sons.size());
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount04", "sonaccount01", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount05", "sonaccount02", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount06", "sonaccount03", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount07", "sonaccount04", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount08", "sonaccount05", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount09", "sonaccount06", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount10", "sonaccount07", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount11", "sonaccount08", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount12", "sonaccount09", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount13", "sonaccount10", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount14", "sonaccount11", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount15", "sonaccount12", true, true);
      vote_tx = con.wallet_api_ptr->vote_for_son("sonaccount16", "sonaccount13", true, true);
      gpo = con.wallet_api_ptr->get_global_properties();
      BOOST_TEST_MESSAGE("gpo: " << gpo.active_sons.size());
      BOOST_CHECK(generate_maintenance_block(app1));

      BOOST_CHECK(gpo.active_sons.size() == 15);

   } catch( fc::exception& e ) {
      BOOST_TEST_MESSAGE("SON cli wallet tests exception");
      edump((e.to_detail_string()));
      throw;
   }
   BOOST_TEST_MESSAGE("SON cli wallet tests end");
}

///////////////////////
// Check account history pagination
///////////////////////
BOOST_FIXTURE_TEST_CASE( account_history_pagination, cli_fixture )
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

      BOOST_CHECK(generate_block(app1));

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

BOOST_FIXTURE_TEST_CASE( cli_get_son, cli_fixture )
{
   try
   {
      BOOST_TEST_MESSAGE("Cli get_son Test");

      INVOKE(upgrade_nathan_account); // just to fund nathan

      // create a new account
      graphene::wallet::brain_key_info bki = con.wallet_api_ptr->suggest_brain_key();
      BOOST_CHECK(!bki.brain_priv_key.empty());
      signed_transaction create_acct_tx = con.wallet_api_ptr->create_account_with_brain_key(
            bki.brain_priv_key, "sonmember", "nathan", "nathan", true
      );
      // save the private key for this new account in the wallet file
      BOOST_CHECK(con.wallet_api_ptr->import_key("sonmember", bki.wif_priv_key));
      con.wallet_api_ptr->save_wallet_file(con.wallet_filename);

      // attempt to give sonmember some CORE
      BOOST_TEST_MESSAGE("Transferring CORE from Nathan to sonmember");
      signed_transaction transfer_tx = con.wallet_api_ptr->transfer(
            "nathan", "sonmember", "100000", "1.3.0", "Here are some CORE token for your new account", true
      );

      BOOST_CHECK(generate_block(app1));

      // upgrade sonmember account
      con.wallet_api_ptr->upgrade_account("sonmember", true);
      auto sonmember_acct = con.wallet_api_ptr->get_account("sonmember");
      BOOST_CHECK(sonmember_acct.is_lifetime_member());

      // create son
      con.wallet_api_ptr->create_son("sonmember", "http://sonmember", true);

      // get_son
      auto son_data = con.wallet_api_ptr->get_son("sonmember");
      BOOST_CHECK(son_data.url == "http://sonmember");
      BOOST_CHECK(son_data.son_account == sonmember_acct.get_id());

      // update SON
      con.wallet_api_ptr->update_son("sonmember", "http://sonmember_updated", "", true);
      son_data = con.wallet_api_ptr->get_son("sonmember");
      BOOST_CHECK(son_data.url == "http://sonmember_updated");

      // delete SON
      con.wallet_api_ptr->delete_son("sonmember", true);
      auto res = con.wallet_api_ptr->list_sons("", 100);
      BOOST_CHECK(res.find("sonmember") == res.end());

   } catch( fc::exception& e ) {
      edump((e.to_detail_string()));
      throw;
   }
}
