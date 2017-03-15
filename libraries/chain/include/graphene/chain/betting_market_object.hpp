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

#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {

class database;

class betting_market_group_object : public graphene::db::abstract_object< betting_market_group_object >
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = betting_market_group_object_type;

      event_id_type event_id;

      betting_market_options_type options;
};

class betting_market_object : public graphene::db::abstract_object< betting_market_object >
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = betting_market_object_type;

      betting_market_group_id_type group_id;

      internationalized_string_type payout_condition;

      asset_id_type asset_id;
};

typedef multi_index_container<
   betting_market_group_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > > > > betting_market_group_object_multi_index_type;
typedef generic_index<betting_market_group_object, betting_market_group_object_multi_index_type> betting_market_group_object_index;

typedef multi_index_container<
   betting_market_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > > > > betting_market_object_multi_index_type;

typedef generic_index<betting_market_object, betting_market_object_multi_index_type> betting_market_object_index;
} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::betting_market_group_object, (graphene::db::object), (event_id)(options) )
FC_REFLECT_DERIVED( graphene::chain::betting_market_object, (graphene::db::object), (group_id)(payout_condition)(asset_id) )
