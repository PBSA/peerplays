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

namespace graphene { namespace chain {

class database;

class sport_object : public graphene::db::abstract_object< sport_object >
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = sport_object_type;

      internationalized_string_type name;
      
      /// manager account can modify the sportobject without the permission
      /// of the witness_account, also he can modify all objects beneath (event_group etc.)
      account_id_type manager = account_id_type(1);
};

typedef multi_index_container<
   sport_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > > > > sport_object_multi_index_type;

typedef generic_index<sport_object, sport_object_multi_index_type> sport_object_index;
} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::sport_object, (graphene::db::object), (name) (manager) )
