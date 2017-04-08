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
#include <graphene/chain/protocol/tournament.hpp>

namespace graphene { namespace chain {

void tournament_options::validate() const
{
   //FC_ASSERT( number_of_players >= 2 && (number_of_players & (number_of_players - 1)) == 0,
   //           "Number of players must be a power of two" );
}

share_type tournament_create_operation::calculate_fee(const fee_parameters_type& k)const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(*this), k.price_per_kbyte );
}

void  tournament_create_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
   options.validate();
}

share_type tournament_join_operation::calculate_fee(const fee_parameters_type& k)const
{
   return k.fee;
}

void  tournament_join_operation::validate()const
{
   FC_ASSERT( fee.amount >= 0 );
}

share_type tournament_leave_operation::calculate_fee(const fee_parameters_type& k)const
{
   return k.fee;
}

void  tournament_leave_operation::validate()const
{
   // todo FC_ASSERT( fee.amount >= 0 );
}


share_type game_move_operation::calculate_fee(const fee_parameters_type& k)const
{
   return k.fee;
}

void  game_move_operation::validate()const
{
}

} } // namespace graphene::chain
