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

#pragma once

#include <cassert>
#include <cstdint>
#include <string>

#include <fc/container/flat.hpp>
#include <fc/reflect/reflect.hpp>

namespace graphene { namespace chain {

   struct rock_paper_scissors_game_options
   {
      /// If true and a user fails to commit their move before the time_per_commit_move expires, 
      /// the blockchain will randomly choose a move for the user
      bool insurance_enabled;
      /// The number of seconds users are given to commit their next move, counted from the beginning
      /// of the hand (during the game, a hand begins immediately on the block containing the 
      /// second player's reveal or where the time_per_reveal move has expired)
      uint32_t time_per_commit_move;

      /// The number of seconds users are given to reveal their move, counted from the time of the
      /// block containing the second commit or the where the time_per_commit_move expired
      uint32_t time_per_reveal_move;
   };

   struct rock_paper_scissors_game_details
   {


   };
   typedef fc::static_variant<rock_paper_scissors_game_options> game_specific_details;

} }

FC_REFLECT( graphene::chain::rock_paper_scissors_game_options, (insurance_enabled)(time_per_commit_move)(time_per_reveal_move) )


