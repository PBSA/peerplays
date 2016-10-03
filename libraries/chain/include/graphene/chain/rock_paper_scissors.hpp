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

#include <string>

#include <fc/crypto/sha256.hpp>
#include <fc/reflect/reflect.hpp>
#include <fc/optional.hpp>
#include <fc/static_variant.hpp>
#include <fc/array.hpp>

namespace graphene { namespace chain {
   enum class rock_paper_scissors_throw
   {
      rock,
      paper,
      scissors
   };
   struct rock_paper_scissors_move
   {
      uint64_t nonce1;
      uint64_t nonce2;
      rock_paper_scissors_throw move;
   };
   struct rock_paper_scissors_commit
   {
      uint64_t nonce1;
      fc::sha256 move_hash;
   };
   struct rock_paper_scissors_reveal
   {
      uint64_t nonce2;
      rock_paper_scissors_throw move;
   };

   struct rock_paper_scissors_game_details
   {
      fc::array<fc::optional<rock_paper_scissors_commit>, 2> commit_moves;
      fc::array<fc::optional<rock_paper_scissors_reveal>, 2> reveal_moves;
   };

   typedef fc::static_variant<rock_paper_scissors_game_details> game_specific_details;

} }

FC_REFLECT_ENUM( graphene::chain::rock_paper_scissors_throw,
                 (rock)
                 (paper)
                 (scissors))

FC_REFLECT( graphene::chain::rock_paper_scissors_move,
            (nonce1)
            (nonce2)
            (move) )

FC_REFLECT( graphene::chain::rock_paper_scissors_commit,
            (nonce1)
            (move_hash) )

FC_REFLECT( graphene::chain::rock_paper_scissors_reveal,
            (nonce2)(move) )

FC_REFLECT( graphene::chain::rock_paper_scissors_game_details,
            (commit_moves)(reveal_moves) )
