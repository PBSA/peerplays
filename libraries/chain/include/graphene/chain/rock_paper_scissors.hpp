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

#include <fc_pp/crypto/sha256.hpp>
#include <fc_pp/reflect/reflect.hpp>
#include <fc_pp/optional.hpp>
#include <fc_pp/static_variant.hpp>
#include <fc_pp/array.hpp>

namespace graphene { namespace chain {
   struct rock_paper_scissors_game_details
   {
      // note: I wanted to declare these as fixed arrays, but they don't serialize properly
      //fc_pp::array<fc_pp::optional<rock_paper_scissors_throw_commit>, 2> commit_moves;
      //fc_pp::array<fc_pp::optional<rock_paper_scissors_throw_reveal>, 2> reveal_moves;
      std::vector<fc_pp::optional<rock_paper_scissors_throw_commit> > commit_moves;
      std::vector<fc_pp::optional<rock_paper_scissors_throw_reveal> > reveal_moves;
      rock_paper_scissors_game_details() :
         commit_moves(2),
         reveal_moves(2)
      {
      }
   };

   typedef fc_pp::static_variant<rock_paper_scissors_game_details> game_specific_details;
} }

FC_REFLECT( graphene::chain::rock_paper_scissors_game_details,
            (commit_moves)(reveal_moves) )
FC_REFLECT_TYPENAME( graphene::chain::game_specific_details )

