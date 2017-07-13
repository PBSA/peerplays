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
//#include <fc/crypto/openssl.hpp>
//#include <openssl/rand.h>

#include <graphene/chain/tournament_object.hpp>
#include <graphene/chain/match_object.hpp>
#include <graphene/chain/game_object.hpp>
#include "../common/database_fixture.hpp"
#include <graphene/utilities/tempdir.hpp>

#include <signal.h>

using namespace graphene::chain;

bool test_standard_rand = false;
bool all_tests = false;
bool game_is_over = false;

void sig_handler(int signo)
{
    std::cout << "." << std::endl;
    game_is_over = true;
}

BOOST_AUTO_TEST_SUITE(tournament_tests)

bool one_test(database_fixture& df, int test_nr = 0, int tsamples = 0, int psamples = 0, std::string options = "")
{
    game_is_over = false;

    std::string command = "dieharder -g 200 -d " + std::to_string(test_nr) ;
    if (tsamples)
        command += " -t " + std::to_string(tsamples);
    if (psamples)
        command += " -p " + std::to_string(psamples);
    if (!options.empty())
        command += " " + options;

    FILE *io = popen(command.c_str(), "w");
    BOOST_CHECK(io);
    if(!io)
        return false;

   int r;
   void *binary_data;
   size_t binary_data_length = sizeof(r);
   int m = 0; // 0 not generate blocks, > 0 generate block every m generated numbers 100 - 1000 are reasonable
   int i = 0;

   while ( !game_is_over && !feof(io) && !ferror(io) )
   {
        if (test_standard_rand) {
                r = rand();
        } else {
            if (i) {
               --i;
                df.generate_block();
            } else  {
               i = m;
            }
            r = df.db.get_random_bits((uint64_t)INT_MAX+1);
        }

        binary_data = (void *) &r;
        size_t l =
        fwrite(binary_data, 1, binary_data_length, io);
        if (l != binary_data_length) break;
        //fflush(io);
   }

    pclose(io);
    return true;
}

BOOST_FIXTURE_TEST_CASE( basic, database_fixture )
{
    try
    {
           std::string o(" dieharder ");
                       o.append(all_tests ? "all" : "selected").
                         append(" tests of ").
                         append(test_standard_rand ? "rand" : "get_random_bits");
           BOOST_TEST_MESSAGE("Hello" + o);

           std::vector<int> selected = {0, 1, 3, 5, 6, 15};
#if 1
           // trying to randomize starting point
           int r = std::rand() % 100;
           for(int i = 0; i < r ; ++i)
               db.get_random_bits(INT_MAX);
#endif
           for (int i = 0; i < 18; ++i)
           {
               if (!all_tests && std::find(selected.begin(), selected.end(), i) == selected.end())
                   continue;
               BOOST_TEST_MESSAGE("#" + std::to_string(i));
               if (!one_test(*this, i))
                    break;
           }
           BOOST_TEST_MESSAGE("Bye" + o);
    }
    catch (fc::exception& e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_SUITE_END()

//#define BOOST_TEST_MODULE "C++ Unit Tests for Graphene Blockchain Database"
#include <cstdlib>
#include <iostream>
#include <boost/test/included/unit_test.hpp>

boost::unit_test::test_suite* init_unit_test_suite(int argc, char* argv[]) {
    for (int i=1; i<argc; i++)
    {
       const std::string arg = argv[i];
       std::cout << "#" << i << " " <<  arg << std::endl;
       if(arg == "-R")
          test_standard_rand = true;
       else if(arg == "-A")
          all_tests = true;
    }
    std::srand(time(NULL));
    std::cout << "Random number generator seeded to " << time(NULL) << std::endl;
    if (signal(SIGPIPE, sig_handler) == SIG_ERR)
        std::cout << "Can't catch SIGPIPE signal!" << std::endl;
    return nullptr;
}
