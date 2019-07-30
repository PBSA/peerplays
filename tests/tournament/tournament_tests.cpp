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
#include <fc/crypto/openssl.hpp>
#include <openssl/rand.h>

#include <graphene/chain/tournament_object.hpp>
#include <graphene/chain/match_object.hpp>
#include <graphene/chain/game_object.hpp>
#include "../common/tournament_helper.hpp"
#include <graphene/utilities/tempdir.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

#include <boost/algorithm/string/replace.hpp>

using namespace graphene::chain;

// defined if "bye" matches fix available
#define BYE_MATCHES_FIXED

#define RAND_MAX_MIN(MAX, MIN) (std::rand() % ((MAX) - (MIN) + 1) + (MIN))

BOOST_AUTO_TEST_SUITE(tournament_tests)

BOOST_FIXTURE_TEST_CASE( Registration_deadline_has_already, database_fixture )
{
    try
    {       std::string reason("Registration deadline has already passed");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            tournament_id_type tournament_id;
            asset buy_in = asset(10000);
            try
            {
                tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 3, 1, 3,
                                                                     -100);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_REQUIRE(tournament_id == tournament_id_type());
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( registration_deadline_must_be, database_fixture )
{
    try
    {       std::string reason("Registration deadline must be before");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            tournament_id_type tournament_id;
            asset buy_in = asset(10000);
            try
            {
                tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 3, 1, 3,
                                                                     db.get_global_properties().parameters.maximum_registration_deadline + 1000);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_REQUIRE(tournament_id == tournament_id_type());
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( buyin_may_not_be_negative, database_fixture )
{
    try
    {       std::string reason("Tournament buy-in may not be negative");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(-1);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key,
                                                     buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( play_with_yourself, database_fixture )
{
    try
    {       std::string reason("re going to play with yourself, do it off-chain");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in,
                                                     1);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( tournaments_may_not_have_more_than, database_fixture )
{
    try
    {       std::string reason("Tournaments may not have more than");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in,
                                                     db.get_global_properties().parameters.maximum_players_in_tournament + 1);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( whitelist_must_allow_enough_players, database_fixture )
{
    try
    {       std::string reason("Whitelist must allow enough players to fill the tournament");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob)(carol));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            flat_set<account_id_type> whitelist{ alice_id, bob_id };
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 3,
                                                     30, 30, 3, 60, 3, 3, true, 3, 0, whitelist);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( whitelist_must_not_be_longer_than, database_fixture )
{
    try
    {       std::string reason("Whitelist must not be longer than");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            db.get_global_properties().parameters.maximum_tournament_whitelist_length;
            flat_set<account_id_type> whitelist;
            for(uint16_t i = 0; i < db.get_global_properties().parameters.maximum_tournament_whitelist_length+1; ++i)
            {
                std::string name = "account" + std::to_string(i);
                auto priv_key = generate_private_key(name);
                const auto& account = create_account(name, priv_key.get_public_key());
                whitelist.emplace(account.id);
            }

            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2,
                                                     30, 30, 3, 60, 3, 3, true, 3, 0, whitelist);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( cannot_specify_both_fixed_start_time_and_delay, database_fixture )
{
    try
    {       std::string reason("Cannot specify both a fixed start time and a delay");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");

            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            tournament_id_type tournament_id;
            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 30, 30, 3,
                                                                     3601,
                                                                     13, 3, true, 3,
                                                                     3600);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_REQUIRE(tournament_id == tournament_id_type());
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( cannot_start_before_registration, database_fixture )
{
    try
    {       std::string reason("Cannot start before registration deadline expires");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");

            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 30, 30, 3,
                                                                     3601,
                                                                     0, 3, true, 3,
                                                                     3600);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( start_time_is_too_far, database_fixture )
{
    try
    {       std::string reason("Start time is too far in the future");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");

            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 30, 30, 3, 60,
                                                                     0, 3, true, 3,
                                                                     db.get_global_properties().parameters.maximum_tournament_start_time_in_future + 1);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( start_delay_is_too_long, database_fixture )
{
    try
    {       std::string reason("Start delay is too long");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");

            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 30, 30, 3, 60,
                                                                     db.get_global_properties().parameters.maximum_tournament_start_delay + 1);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( must_specify_either_a_fixed, database_fixture )
{
    try
    {       std::string reason("Must specify either a fixed start time or a delay");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");

            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 30, 30, 3, 60,
                                                                     0, 3, true, 3,
                                                                     0);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( matches_may_not_require_more_than, database_fixture )
{
    try
    {       std::string reason("Matches may not require more than");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 3, 1,
                                                                     db.get_global_properties().parameters.maximum_tournament_number_of_wins + 1);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( delay_between_games_must_not_be_less, database_fixture )
{
    try
    {       std::string reason("Delay between games must not be less");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");

            db.modify(db.get_global_properties(), [](global_property_object& p) {
               p.parameters.min_round_delay = 1;
            });

            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 3, 1, 3, 3600, 3,
                                                                     0);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( delay_between_games_must_not_be_greater, database_fixture )
{
    try
    {       std::string reason("Delay between games must not be greater");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 3, 1, 3, 3600, 3,
                                                                     db.get_global_properties().parameters.max_round_delay + 1);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( time_to_commit_the_move_must_not_be_less, database_fixture )
{
    try
    {       std::string reason("Time to commit the next move must not be less");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");

            db.modify(db.get_global_properties(), [](global_property_object& p) {
               p.parameters.min_time_per_commit_move = 1;
            });

            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2,
                                                                     0);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( time_to_commit_the_move_must_not_be_greater, database_fixture )
{
    try
    {       std::string reason("Time to commit the next move must not be greater");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2,
                                                                     db.get_global_properties().parameters.max_time_per_commit_move + 1);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( time_to_reveal_the_move_must_not_be_less, database_fixture )
{
    try
    {       std::string reason("Time to reveal the move must not be less");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");

            db.modify(db.get_global_properties(), [](global_property_object& p) {
               p.parameters.min_time_per_reveal_move = 1;
            });

            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 30,
                                                                     0);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( time_to_reveal_the_move_must_not_be_greater, database_fixture )
{
    try
    {       std::string reason("Time to reveal the move must not be greater");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 30,
                                                                     db.get_global_properties().parameters.max_time_per_reveal_move + 1);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( supports_3_gestures_currently, database_fixture )
{
    try
    {       std::string reason("GUI Wallet only supports 3 gestures currently");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            try
            {
                tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 30, 30, 3, 60, 3, 3, true,
                                                                                        4);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

//  joining tournament

BOOST_FIXTURE_TEST_CASE( can_only_join, database_fixture )
{
    try
    {       std::string reason("Can only join a tournament during registration period");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            //transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 3, 1, 3, 1);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            //tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            sleep(2);
            generate_block();
            try
            {
                tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

#if 0
// it is unlikely that tournament_join_evaluator will reach the assertion
BOOST_FIXTURE_TEST_CASE( tournament_is_already, database_fixture )
{
    try
    {       std::string reason("Tournament is already full");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob)(carol));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));
            transfer(committee_account, carol_id,    asset(4000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
            try
            {
                tournament_helper.join_tournament(tournament_id, carol_id, carol_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("carol"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}
#endif

#if 0
// it is unlikely that tournament_join_evaluator will reach the assertion
BOOST_FIXTURE_TEST_CASE( registration_deadline_has_already_passed, database_fixture )
{
    try
    {       std::string reason("Registration deadline has already passed");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2, 3, 1, 3, 2);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            //tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            sleep(3);
            generate_block();
            try
            {
                tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}
#endif

BOOST_FIXTURE_TEST_CASE( player_is_not_on_the_whitelist, database_fixture )
{
    try
    {       std::string reason("Player is not on the whitelist for this tournament");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob)(carol));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));
            transfer(committee_account, carol_id,  asset(4000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            flat_set<account_id_type> whitelist{ alice_id, carol_id };
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2,
                                                                 30, 30, 3, 60, 3, 3, true, 3, 0, whitelist);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            try
            {
                tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( player_is_already_registered, database_fixture )
{
    try
    {       std::string reason("Player is already registered for this tournament");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 3);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
            try
            {
                tournament_helper.join_tournament(tournament_id, alice_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( buy_in_incorrect, database_fixture )
{
    try
    {       std::string reason("Buy-in is incorrect");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "', amount differs");
            ACTORS((nathan)(alice)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            asset buy_in_1 = asset(10001);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 3);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            try
            {
                tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in_1);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( buy_in_another_asset, database_fixture )
{
    try
    {       std::string reason("Buy-in is incorrect");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "', asset differs");

            ACTORS((nathan)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            const asset_id_type new_id = create_user_issued_asset("PEERBTC", nathan_id(db), 0).id;
            issue_uia(nathan_id, asset(GRAPHENE_MAX_SHARE_SUPPLY/3, new_id));

            transfer(nathan_id, bob_id, asset(10000000, new_id));
            asset buy_in = asset(10000);
            asset buy_in_1 = asset(10001, new_id);

            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in_1, 2);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            try
            {
                tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in_1);
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( asset_has_transfer_restricted, database_fixture )
{
    try
    {       std::string reason("Asset {asset} has transfer_restricted flag enabled");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");


            ACTORS((nathan)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            const asset_id_type new_id = create_user_issued_asset("NEW", nathan_id(db), transfer_restricted).id;
            issue_uia(nathan_id, asset(GRAPHENE_MAX_SHARE_SUPPLY/3, new_id));

            transfer(nathan_id, bob_id, asset(10000000, new_id));
            asset buy_in = asset(10000, new_id);

            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            try
            {
                tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( player_account_is_not_whitelisted, database_fixture )
{
    try
    {       std::string reason("player account {player} is not whitelisted for asset {asset}");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");

            ACTORS((nathan)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            const asset_id_type new_id = create_user_issued_asset( "NEW", nathan_id(db), white_list).id;
            //BOOST_CHECK(is_authorized_asset( db, nathan_id(db), new_id(db) ));

            asset_update_operation uop;
            uop.issuer = nathan_id;
            uop.asset_to_update = new_id;
            uop.new_options = new_id(db).options;
            uop.new_options.whitelist_authorities = {nathan_id, bob_id};
            trx.operations.push_back(uop);
            db.push_transaction( trx, ~0 );
            trx.operations.clear();
            BOOST_CHECK(new_id(db).options.whitelist_authorities.find(nathan_id) != new_id(db).options.whitelist_authorities.end());

            account_whitelist_operation wop;
            wop.authorizing_account = nathan_id;
            wop.account_to_list = nathan_id;
            wop.new_listing = account_whitelist_operation::white_listed;
            trx.operations.push_back(wop);
            db.push_transaction( trx, ~0 );
            trx.operations.clear();
            BOOST_CHECK(nathan_id(db).whitelisting_accounts.find(nathan_id) != nathan_id(db).whitelisting_accounts.end());

            issue_uia(nathan_id, asset(GRAPHENE_MAX_SHARE_SUPPLY/3, new_id));

            wop.account_to_list = bob_id;
            trx.operations.push_back(wop);
            db.push_transaction( trx, ~0 );
            trx.operations.clear();

            transfer(nathan_id, bob_id, asset(10000000, new_id));
            asset buy_in = asset(10000, new_id);

            wop.account_to_list = bob_id;
            wop.new_listing = account_whitelist_operation::black_listed;
            trx.operations.push_back(wop);
            db.push_transaction( trx, ~0 );
            trx.operations.clear();

            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            boost::replace_all(reason, "{player}", std::string(object_id_type(bob_id)));
            boost::replace_all(reason, "{asset}", std::string(object_id_type(new_id)));

            try
            {
                tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }

            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( payer_account_is_not_whitelisted, database_fixture )
{
    try
    {       std::string reason("payer account {payer} is not whitelisted for asset {asset}");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");

            ACTORS((nathan)(bob)(carol));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            const asset_id_type new_id = create_user_issued_asset( "NEW", nathan_id(db), white_list).id;
            //BOOST_CHECK(is_authorized_asset( db, nathan_id(db), new_id(db) ));

            asset_update_operation uop;
            uop.issuer = nathan_id;
            uop.asset_to_update = new_id;
            uop.new_options = new_id(db).options;
            uop.new_options.whitelist_authorities = {nathan_id, bob_id, carol_id};
            trx.operations.push_back(uop);
            db.push_transaction( trx, ~0 );
            trx.operations.clear();
            BOOST_CHECK(new_id(db).options.whitelist_authorities.find(nathan_id) != new_id(db).options.whitelist_authorities.end());

            account_whitelist_operation wop;
            wop.authorizing_account = nathan_id;
            wop.account_to_list = nathan_id;
            wop.new_listing = account_whitelist_operation::white_listed;
            trx.operations.push_back(wop);
            db.push_transaction( trx, ~0 );
            trx.operations.clear();
            BOOST_CHECK(nathan_id(db).whitelisting_accounts.find(nathan_id) != nathan_id(db).whitelisting_accounts.end());

            issue_uia(nathan_id, asset(GRAPHENE_MAX_SHARE_SUPPLY/3, new_id));

            wop.account_to_list = bob_id;
            trx.operations.push_back(wop);
            db.push_transaction( trx, ~0 );
            trx.operations.clear();

            wop.account_to_list = carol_id;
            trx.operations.push_back(wop);
            db.push_transaction( trx, ~0 );
            trx.operations.clear();

            transfer(nathan_id, bob_id, asset(10000000, new_id));
            transfer(nathan_id, carol_id, asset(10000000, new_id));
            asset buy_in = asset(10000, new_id);

            wop.account_to_list = carol_id;
            wop.new_listing = account_whitelist_operation::black_listed;
            trx.operations.push_back(wop);
            db.push_transaction( trx, ~0 );
            trx.operations.clear();

            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            boost::replace_all(reason, "{payer}", std::string(object_id_type(carol_id)));
            boost::replace_all(reason, "{asset}", std::string(object_id_type(new_id)));

            try
            {
                tournament_helper.join_tournament(tournament_id, bob_id, carol_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("carol"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }

            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( paying_account_has_insufficient_balance , database_fixture )
{
    try
    {       std::string reason("paying account '{payer}' has insufficient balance to pay buy-in");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 3);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            boost::replace_all(reason, "{payer}", "bob");

            try
            {
                tournament_helper.join_tournament(tournament_id, alice_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                //BOOST_TEST_MESSAGE(e.to_detail_string());
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

//  leaving tournament

BOOST_FIXTURE_TEST_CASE( player_is_not_registered, database_fixture )
{
    try
    {       std::string reason("Player is not registered for this tournament");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            try
            {
                tournament_helper.leave_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))));
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( only_player_or_payer, database_fixture )
{
    try
    {       std::string reason("Only player or payer can unregister the player from a tournament");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob)(carol));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));
            transfer(committee_account, carol_id,  asset(4000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            // player may unregister
            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.leave_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))));

            // payer may unregister
            tournament_helper.join_tournament(tournament_id, alice_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
            tournament_helper.leave_tournament(tournament_id, alice_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))));

            // no one else
            tournament_helper.join_tournament(tournament_id, bob_id, carol_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("carol"))), buy_in);
            try
            {
                tournament_helper.leave_tournament(tournament_id, bob_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))));
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_FIXTURE_TEST_CASE( can_only_leave, database_fixture )
{
    try
    {       std::string reason("Can only leave a tournament during registration period");
            BOOST_TEST_MESSAGE("Starting test '" + reason + "'");
            ACTORS((nathan)(alice)(bob));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));

            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            asset buy_in = asset(10000);
            tournament_id_type tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 2);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
            try
            {
                tournament_helper.leave_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))));
                FC_ASSERT(false, "no error has occured");
            }
            catch (fc::exception& e)
            {
                FC_ASSERT(e.to_detail_string().find(reason) != std::string::npos, "expected error hasn't occured");
            }
            BOOST_TEST_MESSAGE("Eof test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

/// Expecting success

// Test of basic functionality creating two tournamenst, joinig players,
// playing tournaments to completion, distributing prize.
// Testing of "bye" matches handling can be performed if "bye" matches fix is available.
// Numbers of players 2+1 4+1 8+1 ... seem to be most critical for handling of "bye" matches.
// Moves are generated automatically.
BOOST_FIXTURE_TEST_CASE( simple, database_fixture )
{
    try
    {
#ifdef BYE_MATCHES_FIXED
    #define TEST1_NR_OF_PLAYERS_NUMBER 3
    #define TEST2_NR_OF_PLAYERS_NUMBER 5
#else
     #define TEST1_NR_OF_PLAYERS_NUMBER 2
     #define TEST2_NR_OF_PLAYERS_NUMBER 4
#endif
            BOOST_TEST_MESSAGE("Hello simple tournament test");
            ACTORS((nathan)(alice)(bob)(carol)(dave)(ed)(frank)(george)(harry)(ike)(romek));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            BOOST_TEST_MESSAGE( "Giving folks some money" );
            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, romek_id,  asset(2000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));
            transfer(committee_account, carol_id,  asset(4000000));
            transfer(committee_account, dave_id,   asset(5000000));
#if TEST2_NR_OF_PLAYERS_NUMBER > 4
            transfer(committee_account, ed_id,     asset(6000000));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 5
            transfer(committee_account, frank_id,  asset(7000000));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 6
            transfer(committee_account, george_id, asset(8000000));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 7
            transfer(committee_account, harry_id,  asset(9000000));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 8
            transfer(committee_account, ike_id,  asset(1000000));
#endif

            BOOST_TEST_MESSAGE( "Preparing nathan" );
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            uint16_t tournaments_to_complete = 0;
            asset buy_in = asset(12000);
            tournament_id_type tournament_id;

            BOOST_TEST_MESSAGE( "Preparing a tournament, insurance disabled" );
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, TEST1_NR_OF_PLAYERS_NUMBER);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
#if TEST1_NR_OF_PLAYERS_NUMBER > 2
            tournament_helper.join_tournament(tournament_id, carol_id, carol_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("carol"))), buy_in);
#endif
            ++tournaments_to_complete;

            BOOST_TEST_MESSAGE( "Preparing another one, insurance enabled" );
            buy_in = asset(13000);
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, TEST2_NR_OF_PLAYERS_NUMBER,
                                                                 3, 1, 3, 3600, 3, 3, true);
            BOOST_REQUIRE(tournament_id == tournament_id_type(1));
            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            // romek joins but will leave
            tournament_helper.join_tournament(tournament_id, romek_id, romek_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("romek"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
            tournament_helper.join_tournament(tournament_id, carol_id, carol_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("carol"))), buy_in);
            // romek leaves
            tournament_helper.leave_tournament(tournament_id, romek_id, romek_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("romek"))));
            tournament_helper.join_tournament(tournament_id, dave_id, dave_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("dave"))), buy_in);
#if TEST2_NR_OF_PLAYERS_NUMBER > 4
            tournament_helper.join_tournament(tournament_id, ed_id, ed_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("ed"))), buy_in);
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 5
            tournament_helper.join_tournament(tournament_id, frank_id, frank_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("frank"))), buy_in);
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 6
            tournament_helper.join_tournament(tournament_id, george_id, george_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("george"))), buy_in);
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 7
            tournament_helper.join_tournament(tournament_id, harry_id, harry_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("harry"))), buy_in);
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 8
            tournament_helper.join_tournament(tournament_id, ike_id, ike_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("ike"))), buy_in);
#endif
            ++tournaments_to_complete;

            auto abc = [&] (string s)
            {
#if 0
                wlog(s);
                auto a = db.get_balance(alice_id, asset_id_type()); wdump(("# alice's balance") (a));
                auto b = db.get_balance(bob_id, asset_id_type()); wdump(("# bob's balance") (b));
                auto c = db.get_balance(carol_id, asset_id_type()); wdump(("# carol's balance") (c));
                auto d = db.get_balance(dave_id, asset_id_type()); wdump(("# dave's balance") (d));
#if TEST2_NR_OF_PLAYERS_NUMBER > 4
                auto e = db.get_balance(ed_id, asset_id_type()); wdump(("# ed's balance") (e));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 5
                auto f = db.get_balance(frank_id, asset_id_type()); wdump(("# frank's balance") (f));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 6
                auto g = db.get_balance(george_id, asset_id_type()); wdump(("# george's balance") (g));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 7
                auto h = db.get_balance(harry_id, asset_id_type()); wdump(("# harry's balance") (f));
#endif
#if TEST2_NR_OF_PLAYERS_NUMBER > 8
                auto i = db.get_balance(ike_id, asset_id_type()); wdump(("# ike's balance") (i));
#endif

                auto n = db.get_balance(nathan_id, asset_id_type()); wdump(("# nathan's balance") (n));
                auto r = db.get_balance(GRAPHENE_RAKE_FEE_ACCOUNT_ID, asset_id_type()); wdump(("# rake's balance") (r));
#endif
            };


            abc("@ tournament awaiting start");
            BOOST_TEST_MESSAGE( "Generating blocks, waiting for tournaments' completion");
            generate_block();
            abc("@ after first generated block");
            std::set<tournament_id_type> tournaments = tournament_helper.list_tournaments();
            std::map<account_id_type, std::map<asset_id_type, share_type>> players_balances = tournament_helper.list_players_balances();
            uint16_t rake_fee_percentage = db.get_global_properties().parameters.rake_fee_percentage;

            while(tournaments_to_complete > 0)
            {
                for(const auto& tournament_id: tournaments)
                {
                    const tournament_object& tournament = tournament_id(db);
                    if (tournament.get_state() == tournament_state::concluded) {
                        const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
                        const match_object& final_match = (tournament_details.matches[tournament_details.matches.size() - 1])(db);

                        assert(final_match.match_winners.size() == 1);
                        const account_id_type& winner_id = *final_match.match_winners.begin();

                        const account_object winner = winner_id(db);
                        BOOST_TEST_MESSAGE( "The winner is " + winner.name );

                        share_type rake_amount = (fc::uint128_t(tournament.prize_pool.value) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100).to_uint64();
                        optional<account_id_type> dividend_account = tournament_helper.get_asset_dividend_account(tournament.options.buy_in.asset_id);
                        if (dividend_account.valid())
                            players_balances[*dividend_account][tournament.options.buy_in.asset_id] += rake_amount;
                        players_balances[winner_id][tournament.options.buy_in.asset_id] += tournament.prize_pool - rake_amount;

                        tournaments.erase(tournament_id);
                        --tournaments_to_complete;
                        break;
                    }
                }
                generate_block();
                sleep(1);
            }

            abc("@ tournament concluded");
            // checking if prizes were distributed correctly
            BOOST_CHECK(tournaments.size() == 0);
            std::map<account_id_type, std::map<asset_id_type, share_type>> last_players_balances = tournament_helper.list_players_balances();
            for (auto a: last_players_balances)
            {
                BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s balance " + std::to_string((uint64_t)(a.second[asset_id_type()].value)) );
                BOOST_CHECK(a.second[asset_id_type()] == players_balances[a.first][asset_id_type()]);
            }
            BOOST_TEST_MESSAGE("Bye simple tournament test\n");

    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}


// Test of handling ties, creating two tournamenst, joinig players,
// All generated moves are identical.
BOOST_FIXTURE_TEST_CASE( ties, database_fixture )
{
    try
    {
#ifdef BYE_MATCHES_FIXED
    #define TEST1_NR_OF_PLAYERS_NUMBER 3
    #define TEST2_NR_OF_PLAYERS_NUMBER 5
#else
     #define TEST1_NR_OF_PLAYERS_NUMBER 2
     #define TEST2_NR_OF_PLAYERS_NUMBER 4
#endif
            BOOST_TEST_MESSAGE("Hello ties tournament test");
            ACTORS((nathan)(alice)(bob)(carol)(dave)(ed)(frank)(george)(harry)(ike));

            tournaments_helper tournament_helper(*this);
            fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

            BOOST_TEST_MESSAGE( "Giving folks some money" );
            transfer(committee_account, nathan_id, asset(1000000000));
            transfer(committee_account, alice_id,  asset(2000000));
            transfer(committee_account, bob_id,    asset(3000000));
            transfer(committee_account, carol_id,  asset(4000000));
            transfer(committee_account, dave_id,   asset(5000000));
            transfer(committee_account, ed_id,     asset(6000000));

            BOOST_TEST_MESSAGE( "Preparing nathan" );
            upgrade_to_lifetime_member(nathan);
            BOOST_CHECK(nathan.is_lifetime_member());

            uint16_t tournaments_to_complete = 0;
            asset buy_in = asset(12000);
            tournament_id_type tournament_id;

            BOOST_TEST_MESSAGE( "Preparing a tournament, insurance disabled" );
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, TEST1_NR_OF_PLAYERS_NUMBER, 30, 30);
            BOOST_REQUIRE(tournament_id == tournament_id_type());

            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
#if TEST1_NR_OF_PLAYERS_NUMBER > 2
            tournament_helper.join_tournament(tournament_id, carol_id, carol_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("carol"))), buy_in);
#endif
            ++tournaments_to_complete;

            BOOST_TEST_MESSAGE( "Preparing another one, insurance enabled" );
            buy_in = asset(13000);
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, TEST2_NR_OF_PLAYERS_NUMBER,
                                                                 30, 30, 3, 3600, 3, 3, true);
            BOOST_REQUIRE(tournament_id == tournament_id_type(1));
            tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
            tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
            tournament_helper.join_tournament(tournament_id, carol_id, carol_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("carol"))), buy_in);
            tournament_helper.join_tournament(tournament_id, dave_id, dave_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("dave"))), buy_in);
#if TEST2_NR_OF_PLAYERS_NUMBER > 4
            tournament_helper.join_tournament(tournament_id, ed_id, ed_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("ed"))), buy_in);
#endif
            ++tournaments_to_complete;


            std::set<tournament_id_type> tournaments = tournament_helper.list_tournaments();
            std::map<account_id_type, std::map<asset_id_type, share_type>> players_balances = tournament_helper.list_players_balances();
            uint16_t rake_fee_percentage = db.get_global_properties().parameters.rake_fee_percentage;


            BOOST_TEST_MESSAGE( "Generating blocks, waiting for tournaments' completion");
            while(tournaments_to_complete > 0)
            {
                generate_block();
                //tournament_helper.play_games(3, 4, 1);
                tournament_helper.play_games(0, 0, 1);
                for(const auto& tournament_id: tournaments)
                {
                    const tournament_object& tournament = tournament_id(db);
                    if (tournament.get_state() == tournament_state::concluded) {
                        const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
                        const match_object& final_match = (tournament_details.matches[tournament_details.matches.size() - 1])(db);

                        assert(final_match.match_winners.size() == 1);
                        const account_id_type& winner_id = *final_match.match_winners.begin();
                        BOOST_TEST_MESSAGE( "The winner of " + std::string(object_id_type(tournament_id))  + " is " + winner_id(db).name + " " +  std::string(object_id_type(winner_id)));
                        share_type rake_amount = (fc::uint128_t(tournament.prize_pool.value) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100).to_uint64();
                        optional<account_id_type> dividend_account = tournament_helper.get_asset_dividend_account(tournament.options.buy_in.asset_id);
                        if (dividend_account.valid())
                            players_balances[*dividend_account][tournament.options.buy_in.asset_id] += rake_amount;                    players_balances[winner_id][tournament.options.buy_in.asset_id] += tournament.prize_pool - rake_amount;

                        tournaments.erase(tournament_id);
                        --tournaments_to_complete;
                        break;
                    }
                }
                sleep(1);
            }

            // checking if prizes were distributed correctly
            BOOST_CHECK(tournaments.size() == 0);
            std::map<account_id_type, std::map<asset_id_type, share_type>> last_players_balances = tournament_helper.list_players_balances();
            for (auto a: last_players_balances)
            {
                BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s balance " + std::to_string((uint64_t)(a.second[asset_id_type()].value)) );
                BOOST_CHECK(a.second[asset_id_type()] == players_balances[a.first][asset_id_type()]);
            }
            BOOST_TEST_MESSAGE("Bye ties tournament test\n");

    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

// Test of canceled tournament
// Checking buyin refund.
BOOST_FIXTURE_TEST_CASE( canceled, database_fixture )
{
    try
    {
        BOOST_TEST_MESSAGE("Hello canceled tournament test");
        ACTORS((nathan)(alice)(bob));

        tournaments_helper tournament_helper(*this);
        fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));

        BOOST_TEST_MESSAGE( "Giving folks some money" );
        transfer(committee_account, nathan_id, asset(1000000000));
        transfer(committee_account, alice_id,  asset(2000000));
        transfer(committee_account, bob_id,    asset(3000000));

        BOOST_TEST_MESSAGE( "Preparing nathan" );
        upgrade_to_lifetime_member(nathan);

        uint16_t tournaments_to_complete = 0;
        asset buy_in = asset(12340);
        tournament_id_type tournament_id;

        BOOST_TEST_MESSAGE( "Preparing a tournament" );
        tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, 3, 30, 30, 3, 5);
        BOOST_REQUIRE(tournament_id == tournament_id_type());

        tournament_helper.join_tournament(tournament_id, alice_id, alice_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("alice"))), buy_in);
        tournament_helper.join_tournament(tournament_id, bob_id, bob_id, fc::ecc::private_key::regenerate(fc::sha256::hash(string("bob"))), buy_in);
        ++tournaments_to_complete;
        BOOST_TEST_MESSAGE( "Generating blocks, waiting for tournament's completion");
        std::set<tournament_id_type> tournaments = tournament_helper.list_tournaments();
        std::map<account_id_type, std::map<asset_id_type, share_type>> players_balances = tournament_helper.list_players_balances();

        while(tournaments_to_complete > 0)
        {
            for(const auto& tournament_id: tournaments)
            {
                const tournament_object& tournament = tournament_id(db);
                BOOST_REQUIRE(tournament.get_state() != tournament_state::concluded);

                if (tournament.get_state() == tournament_state::registration_period_expired) {


                    const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
                    for(auto payer : tournament_details.payers)
                    {
                        players_balances[payer.first][tournament.options.buy_in.asset_id] += payer.second;
                    }

                    tournaments.erase(tournament_id);
                    --tournaments_to_complete;
                    break;
                }
            }
            generate_block();
            sleep(1);
        }

        // checking if buyins were refunded correctly
        BOOST_CHECK(tournaments.size() == 0);
        std::map<account_id_type, std::map<asset_id_type, share_type>> last_players_balances = tournament_helper.list_players_balances();
        for (auto a: last_players_balances)
        {
            BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s balance " + std::to_string((uint64_t)(a.second[asset_id_type()].value)) );
            BOOST_CHECK(a.second[asset_id_type()] == players_balances[a.first][asset_id_type()]);
        }
        BOOST_TEST_MESSAGE("Bye canceled tournament test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

// Test of few concurrently played tournaments having the same constant number of players.
// Tournament/s having even number use the core asset.
// Tournament/s having odd number use another asset.
// Moves are generated randomly.
// Checking prizes distribution for both assets is performed.
BOOST_FIXTURE_TEST_CASE( assets, database_fixture )
{
    try
    {
        #define PLAYERS_NUMBER 8
        #define TOURNAMENTS_NUMBER 3
        #define DEF_SYMBOL "NEXT"

        BOOST_TEST_MESSAGE("Hello two assets tournament test");

        ACTORS((nathan));
        fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
        transfer(committee_account, nathan_id, asset(1000000000));
        upgrade_to_lifetime_member(nathan);
        BOOST_CHECK(nathan.is_lifetime_member());

        tournaments_helper tournament_helper(*this);
        // creating new asset
        asset_options aoptions;
        aoptions.max_market_fee = aoptions.max_supply = GRAPHENE_MAX_SHARE_SUPPLY;
        aoptions.flags = 0;
        aoptions.issuer_permissions = 79;
        aoptions.core_exchange_rate.base.amount = 1;
        aoptions.core_exchange_rate.base.asset_id = asset_id_type(0);
        aoptions.core_exchange_rate.quote.amount = 1;
        aoptions.core_exchange_rate.quote.asset_id = asset_id_type(1);
        tournament_helper.create_asset(nathan_id, DEF_SYMBOL, 5, aoptions, nathan_priv_key);

        issue_uia(nathan_id, asset(GRAPHENE_MAX_SHARE_SUPPLY/2, asset_id_type(1)));

        dividend_asset_options doptions;
        doptions.minimum_distribution_interval = 3*24*60*60;
        doptions.minimum_fee_percentage = 10*GRAPHENE_1_PERCENT;
        doptions.next_payout_time = db.head_block_time() + fc::hours(1);
        doptions.payout_interval = 7*24*60*60;
        tournament_helper.update_dividend_asset(asset_id_type(1), doptions, nathan_priv_key);

#if 0
        auto tas = get_asset(GRAPHENE_SYMBOL); wdump((tas));
        auto das = get_asset(DEF_SYMBOL); wdump((das));

        if (das.dividend_data_id.valid())
        {
            auto div = (*das.dividend_data_id)(db); wdump((div));
            auto dda = div.dividend_distribution_account(db); wdump((dda));
        }

        auto nac = nathan_id(db); wdump(("# nathan's account") (nac));

        auto nab0 = db.get_balance(nathan_id, asset_id_type(0)); wdump(("# nathans's balance 0") (nab0));
        auto nab1 = db.get_balance(nathan_id, asset_id_type(1)); wdump(("# nathans's balance 1") (nab1));
#endif

        // creating actors
        std::vector<std::tuple<std::string, account_id_type, fc::ecc::private_key>> actors;
        for(unsigned i = 0; i < PLAYERS_NUMBER; ++i)
        {
            std::string name = "account" + std::to_string(i);
            auto priv_key = generate_private_key(name);
            const auto& account = create_account(name, priv_key.get_public_key());
            actors.emplace_back(name, account.id, priv_key);
            transfer(committee_account, account.id,  asset((uint64_t)100000000 * PLAYERS_NUMBER + 10000000 * (i+1)));
            transfer(nathan_id, account.id, asset((uint64_t)200000000 * PLAYERS_NUMBER + 20000000 * (i+1), asset_id_type(1)));
        }

        // creating tournaments, registering players
        for(unsigned i = 0; i < TOURNAMENTS_NUMBER; ++i)
        {
            asset buy_in = asset(1000 * PLAYERS_NUMBER + 100 * i, asset_id_type(i%2));
            tournament_id_type tournament_id;
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, PLAYERS_NUMBER, 30, 30);

            for (unsigned j = 0; j < PLAYERS_NUMBER; ++j)
            {
                auto a = actors[j];
                tournament_helper.join_tournament(tournament_id, std::get<1>(a), std::get<1>(a), std::get<2>(a), buy_in);
            }
        }

        uint16_t tournaments_to_complete = TOURNAMENTS_NUMBER;
        std::set<tournament_id_type> tournaments = tournament_helper.list_tournaments();
        std::map<account_id_type, std::map<asset_id_type, share_type>> players_balances = tournament_helper.list_players_balances();
        uint16_t rake_fee_percentage = db.get_global_properties().parameters.rake_fee_percentage;

        BOOST_TEST_MESSAGE( "Generating blocks, waiting for tournaments' completion");
        while(tournaments_to_complete > 0)
        {
            generate_block();
            tournament_helper.play_games(3, 4);
            for(const auto& tournament_id: tournaments)
            {
                const tournament_object& tournament = tournament_id(db);
                if (tournament.get_state() == tournament_state::concluded) {
                    const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
                    const match_object& final_match = (tournament_details.matches[tournament_details.matches.size() - 1])(db);

                    assert(final_match.match_winners.size() == 1);
                    const account_id_type& winner_id = *final_match.match_winners.begin();
                    BOOST_TEST_MESSAGE( "The winner of " + std::string(object_id_type(tournament_id))  + " is " + winner_id(db).name + " " +  std::string(object_id_type(winner_id)));
                    share_type rake_amount = (fc::uint128_t(tournament.prize_pool.value) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100).to_uint64();
                    optional<account_id_type> dividend_account = tournament_helper.get_asset_dividend_account(tournament.options.buy_in.asset_id);
                    if (dividend_account.valid())
                        players_balances[*dividend_account][tournament.options.buy_in.asset_id] += rake_amount;                    players_balances[winner_id][tournament.options.buy_in.asset_id] += tournament.prize_pool - rake_amount;

                    tournaments.erase(tournament_id);
                    --tournaments_to_complete;
                    break;
                }
            }
            sleep(1);
        }
        BOOST_CHECK(tournaments.size() == 0);
        // checking if prizes were distributed correctly
        std::map<account_id_type, std::map<asset_id_type, share_type>> last_players_balances = tournament_helper.list_players_balances();
        for (auto a: last_players_balances)
        {
            BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s " + GRAPHENE_SYMBOL + " balance " + std::to_string((uint64_t) (a.second[asset_id_type()].value)));
            BOOST_CHECK(a.second[asset_id_type()] == players_balances[a.first][asset_id_type()]);
            BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s " + DEF_SYMBOL + " balance " + std::to_string((uint64_t) (a.second[asset_id_type(1)].value)));
            BOOST_CHECK(a.second[asset_id_type(1)] == players_balances[a.first][asset_id_type(1)]);
        }

        BOOST_TEST_MESSAGE("Bye two assets tournament test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

// Test of concurrently played tournaments having
// 2, 4, 8 ... 64 players randomly registered from global pool
// and randomized number of wins,
// generates random moves,
// checks prizes distribution and fees calculation.
// No "bye" matches.
BOOST_FIXTURE_TEST_CASE( basic, database_fixture )
{
    try
    {
        #define MIN_PLAYERS_NUMBER 2
        #define MAX_PLAYERS_NUMBER 64

        #define MIN_WINS_NUMBER 2
        #define MAX_WINS_NUMBER 5

        BOOST_TEST_MESSAGE("Hello basic tournament test");

        ACTORS((nathan));
        fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
        transfer(committee_account, nathan_id, asset(1000000000));
        upgrade_to_lifetime_member(nathan);
        BOOST_CHECK(nathan.is_lifetime_member());

        // creating random order of numbers of players joining tournaments
        std::vector<unsigned> players;
        for(unsigned i = MIN_PLAYERS_NUMBER; i <= MAX_PLAYERS_NUMBER; i*=2)
        {
            players.emplace_back(i);
        }
        for (unsigned i = players.size() - 1; i >= 1; --i)
        {
           if (std::rand() % 2 == 0) continue;
           unsigned j = std::rand() % i;
           std::swap(players[i], players[j]);
        }

        // creating a pool of actors
        std::vector<std::tuple<std::string, account_id_type, fc::ecc::private_key>> actors;
        for(unsigned i = 0; i < 3 * MAX_PLAYERS_NUMBER; ++i)
        {
            std::string name = "account" + std::to_string(i);
            auto priv_key = generate_private_key(name);
            const auto& account = create_account(name, priv_key.get_public_key());
            actors.emplace_back(name, account.id, priv_key);
            transfer(committee_account, account.id,  asset((uint64_t)1000000000 * players.size() + 100000000 * (i+1)));
        }
#if 0
        enable_fees();
        wdump((db.get_global_properties()));
#endif
        // creating tournaments, registering players
        tournaments_helper tournament_helper(*this);
        for (unsigned i = 0; i < players.size(); ++i)
        {
            auto number_of_players = players[i];
            auto number_of_wins = RAND_MAX_MIN(MAX_WINS_NUMBER,  MIN_WINS_NUMBER);
            BOOST_TEST_MESSAGE( "Preparing tournament with " + std::to_string(number_of_players)  + " players and " + std::to_string(number_of_wins) + " wins" );

            asset buy_in = asset(1000 * number_of_players + 100 * i);
            tournament_id_type tournament_id;
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, number_of_players, 30, 30, number_of_wins);

            for (unsigned j = 0; j < actors.size() && number_of_players > 0; ++j)
            {
                if (number_of_players < actors.size() - j && std::rand() % 2 == 0) continue;
                auto a = actors[j];
                --number_of_players;
                tournament_helper.join_tournament(tournament_id, std::get<1>(a), std::get<1>(a), std::get<2>(a), buy_in);
            }
        }

        uint16_t tournaments_to_complete = players.size();
        std::set<tournament_id_type> tournaments = tournament_helper.list_tournaments();
        std::map<account_id_type, std::map<asset_id_type, share_type>> players_initial_balances = tournament_helper.list_players_balances();
        tournament_helper.reset_players_fees();
        uint16_t rake_fee_percentage = db.get_global_properties().parameters.rake_fee_percentage;
#if 0
        wlog( "Prepared tournaments:");
        for(const tournament_id_type& tid: tournament_helper.list_tournaments())
        {
            const tournament_object tournament = tid(db);
            //const tournament_details_object details = tournament.tournament_details_id(db);
            wlog(" # ${i}, players count ${c}, wins number ${w}", ("i", tid.instance) ("c", tournament.registered_players) ("w", tournament.options.number_of_wins ));
        }

#endif
        BOOST_TEST_MESSAGE( "Generating blocks, waiting for tournaments' completion");
        tournament_helper.reset_players_fees();
        while(tournaments_to_complete > 0)
        {
            generate_block();
            enable_fees();
            tournament_helper.play_games();
            for(const auto& tournament_id: tournaments)
            {
                const tournament_object& tournament = tournament_id(db);
                if (tournament.get_state() == tournament_state::concluded) {
                    const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
                    const match_object& final_match = (tournament_details.matches[tournament_details.matches.size() - 1])(db);

                    assert(final_match.match_winners.size() == 1);
                    const account_id_type& winner_id = *final_match.match_winners.begin();
                    BOOST_TEST_MESSAGE( "The winner of " + std::string(object_id_type(tournament_id))  + " is " + winner_id(db).name + " " +  std::string(object_id_type(winner_id)));
                    share_type rake_amount = (fc::uint128_t(tournament.prize_pool.value) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100).to_uint64();
                    optional<account_id_type> dividend_account = tournament_helper.get_asset_dividend_account(tournament.options.buy_in.asset_id);
                    if (dividend_account.valid())
                        players_initial_balances[*dividend_account][tournament.options.buy_in.asset_id] += rake_amount;
                    players_initial_balances[winner_id][tournament.options.buy_in.asset_id] += tournament.prize_pool - rake_amount;

                    tournaments.erase(tournament_id);
                    --tournaments_to_complete;
                    break;
                }
            }
            sleep(1);
        }
        BOOST_CHECK(tournaments.size() == 0);

        // checking if prizes were distributed correctly and fees calculated properly
        std::map<account_id_type, std::map<asset_id_type, share_type>> current_players_balances = tournament_helper.list_players_balances();
        std::map<account_id_type, std::map<asset_id_type, share_type>> players_paid_fees = tournament_helper.get_players_fees();
        for (auto a: current_players_balances)
        {
            BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s balance " + std::to_string((uint64_t)(a.second[asset_id_type()].value)) );
            BOOST_CHECK(a.second[asset_id_type()] == players_initial_balances[a.first][asset_id_type()] + players_paid_fees[a.first][asset_id_type()]);
        }

        BOOST_TEST_MESSAGE("Bye basic tournament test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }

}

#ifdef BYE_MATCHES_FIXED
// Test of several concurrently played tournaments having
// randomized number of players registered from global pool,
// and randomized number of wins,
// generates random moves,
// checks prizes distribution.
// "bye" matches fix is required.
BOOST_FIXTURE_TEST_CASE( massive, database_fixture )
{
    try
    {
        #define MIN_TOURNAMENTS_NUMBER 7
        #define MAX_TOURNAMENTS_NUMBER 13

        #define MIN_PLAYERS_NUMBER 2
        #define MAX_PLAYERS_NUMBER 64

        #define MIN_WINS_NUMBER 2
        #define MAX_WINS_NUMBER 5

        BOOST_TEST_MESSAGE("Hello massive tournament test");

        ACTORS((nathan));
        fc::ecc::private_key nathan_priv_key = fc::ecc::private_key::regenerate(fc::sha256::hash(string("nathan")));
        transfer(committee_account, nathan_id, asset(1000000000));
        upgrade_to_lifetime_member(nathan);
        BOOST_CHECK(nathan.is_lifetime_member());

        // creating a pool of actors
        std::vector<std::tuple<std::string, account_id_type, fc::ecc::private_key>> actors;
        for(unsigned i = 0; i < 3 * MAX_PLAYERS_NUMBER; ++i)
        {
            std::string name = "account" + std::to_string(i);
            auto priv_key = generate_private_key(name);
            const auto& account = create_account(name, priv_key.get_public_key());
            actors.emplace_back(name, account.id, priv_key);
            transfer(committee_account, account.id,  asset((uint64_t)1000000 * MAX_TOURNAMENTS_NUMBER + 100000 * (i+1)));
        }

        // creating tournaments, registering players
        tournaments_helper tournament_helper(*this);
        unsigned number_of_tournaments = RAND_MAX_MIN(MAX_TOURNAMENTS_NUMBER, MIN_TOURNAMENTS_NUMBER);
        for(unsigned i = 0; i < number_of_tournaments; ++i)
        {
            unsigned number_of_players = RAND_MAX_MIN(MAX_PLAYERS_NUMBER, MIN_PLAYERS_NUMBER);
            unsigned number_of_wins = RAND_MAX_MIN(MAX_WINS_NUMBER, MIN_WINS_NUMBER);
            BOOST_TEST_MESSAGE( "Preparing tournament with " + std::to_string(number_of_players)  + " players and " + std::to_string(number_of_wins) + " wins" );

            asset buy_in = asset(1000 * number_of_players + 100 * i);
            tournament_id_type tournament_id;
            tournament_id = tournament_helper.create_tournament (nathan_id, nathan_priv_key, buy_in, number_of_players, 30, 30, number_of_wins);
            const tournament_object& tournament = tournament_id(db);

            for (unsigned j = 0; j < actors.size()-1 && number_of_players > 0; ++j)
            {
                if (number_of_players < actors.size() - j && std::rand() % 2 == 0) continue;
                auto a = actors[j];
                tournament_helper.join_tournament(tournament_id, std::get<1>(a), std::get<1>(a), std::get<2>(a), buy_in);
                if (j == i)
                {
                    BOOST_TEST_MESSAGE("Player " + std::get<0>(a) + " is leaving tournament " + std::to_string(i) +
                                        ", when tournament state is " + std::to_string((int)tournament.get_state()));
                    tournament_helper.leave_tournament(tournament_id, std::get<1>(a), std::get<1>(a), std::get<2>(a));
                    continue;
                }
                --number_of_players;
#if 0
                // ok if registration allowed till deadline
                if (!number_of_players)
                {
                    BOOST_TEST_MESSAGE("Player " + std::get<0>(a) + " is leaving tournament " + std::to_string(i) +
                                        ", when tournament state is " + std::to_string((int)tournament.get_state()));
                    tournament_helper.leave_tournament(tournament_id, std::get<1>(a), std::get<1>(a), std::get<2>(a));
                    ++j;
                    a = actors[j];
                    BOOST_TEST_MESSAGE("Player " + std::get<0>(a) + " is joinig tournament " + std::to_string(i) +
                                        ", when tournament state is " + std::to_string((int)tournament.get_state()));
                    tournament_helper.join_tournament(tournament_id, std::get<1>(a), std::get<1>(a), std::get<2>(a), buy_in);
                }
#endif
            }
            BOOST_TEST_MESSAGE("Tournament " +  std::to_string(i) + " is in state " + std::to_string((int)tournament.get_state()));
        }

        uint16_t tournaments_to_complete = number_of_tournaments;
        std::set<tournament_id_type> tournaments = tournament_helper.list_tournaments();
        std::map<account_id_type, std::map<asset_id_type, share_type>> players_balances = tournament_helper.list_players_balances();
        uint16_t rake_fee_percentage = db.get_global_properties().parameters.rake_fee_percentage;

        BOOST_TEST_MESSAGE( "Generating blocks, waiting for tournaments' completion");
        while(tournaments_to_complete > 0)
        {
            generate_block();
            tournament_helper.play_games();
            for(const auto& tournament_id: tournaments)
            {
                const tournament_object& tournament = tournament_id(db);
                if (tournament.get_state() == tournament_state::concluded) {
                    const tournament_details_object& tournament_details = tournament.tournament_details_id(db);
                    const match_object& final_match = (tournament_details.matches[tournament_details.matches.size() - 1])(db);

                    assert(final_match.match_winners.size() == 1);
                    const account_id_type& winner_id = *final_match.match_winners.begin();
                    BOOST_TEST_MESSAGE( "The winner of " + std::string(object_id_type(tournament_id))  + " is " + winner_id(db).name + " " +  std::string(object_id_type(winner_id)));
                    share_type rake_amount = (fc::uint128_t(tournament.prize_pool.value) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100).to_uint64();
                    optional<account_id_type> dividend_account = tournament_helper.get_asset_dividend_account(tournament.options.buy_in.asset_id);
                    if (dividend_account.valid())
                        players_balances[*dividend_account][tournament.options.buy_in.asset_id] += rake_amount;
                    players_balances[winner_id][tournament.options.buy_in.asset_id] += tournament.prize_pool - rake_amount;

                    tournaments.erase(tournament_id);
                    --tournaments_to_complete;
                    break;
                }
            }
            sleep(1);
        }
        BOOST_CHECK(tournaments.size() == 0);
#if 0
        wlog( "Performed tournaments:");
        for(const tournament_id_type& tid: tournament_helper.list_tournaments())
        {
            const tournament_object tournament = tid(db);
            //const tournament_details_object details = tournament.tournament_details_id(db);
            wlog(" # ${i}, players count ${c}, wins number ${w}", ("i", tid.instance) ("c", tournament.registered_players) ("w", tournament.options.number_of_wins ));
        }
#endif
        // checking if prizes were distributed correctly
        std::map<account_id_type, std::map<asset_id_type, share_type>> last_players_balances = tournament_helper.list_players_balances();
        for (auto a: last_players_balances)
        {
            BOOST_TEST_MESSAGE( "Checking " + a.first(db).name + "'s balance " + std::to_string((uint64_t)(a.second[asset_id_type()].value)) );
            BOOST_CHECK(a.second[asset_id_type()] == players_balances[a.first][asset_id_type()]);
        }

        BOOST_TEST_MESSAGE("Bye massive tournament test\n");
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}
#endif

BOOST_AUTO_TEST_SUITE_END()

//#define BOOST_TEST_MODULE "C++ Unit Tests for Graphene Blockchain Database"
#include <cstdlib>
#include <iostream>
#include <boost/test/included/unit_test.hpp>

boost::unit_test::test_suite* init_unit_test_suite(int argc, char* argv[]) {
    std::srand(time(NULL));
    std::cout << "Random number generator seeded to " << time(NULL) << std::endl;
    return nullptr;
}
