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

class event_object : public graphene::db::abstract_object< event_object >
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = event_object_type;

      internationalized_string_type name;
      
      internationalized_string_type season;

      optional<time_point_sec> start_time;

      event_group_id_type event_group_id;

      vector<competitor_id_type> competitors;

      event_status status;
      vector<string> scores;
};

typedef multi_index_container<
   event_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > > > > event_object_multi_index_type;

typedef generic_index<event_object, event_object_multi_index_type> event_object_index;
} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::event_object, (graphene::db::object), (name)(season)(start_time)(event_group_id)(status)(competitors)(scores) )
