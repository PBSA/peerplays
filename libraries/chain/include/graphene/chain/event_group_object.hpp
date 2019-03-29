/*
 * Copyright (c) 2018 Peerplays Blockchain Standards Association, and contributors.
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

#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {

class database;

struct by_sport_id;

class event_group_object : public graphene::db::abstract_object< event_group_object >
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = event_group_object_type;

      internationalized_string_type name;
      sport_id_type sport_id;
    
      void cancel_events(database& db) const;
};

typedef multi_index_container<
   event_group_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >, 
      ordered_unique< tag<by_sport_id>, composite_key<event_group_object,
                                                      member< event_group_object, sport_id_type, &event_group_object::sport_id >,
                                                      member<object, object_id_type, &object::id> > > >
   > event_group_object_multi_index_type;

typedef generic_index<event_group_object, event_group_object_multi_index_type> event_group_object_index;
} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::event_group_object, (graphene::db::object), (name)(sport_id) )
