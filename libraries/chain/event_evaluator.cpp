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
#include <graphene/chain/event_evaluator.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/event_group_object.hpp>
#include <graphene/chain/competitor_object.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>

namespace graphene { namespace chain {

void_result event_create_evaluator::do_evaluate(const event_create_operation& op)
{ try {
   FC_ASSERT(trx_state->_is_proposed_trx);

   database& d = db();

   // the event_group_id in the operation can be a relative id.  If it is,
   // resolve it and verify that it is truly an event_group
   object_id_type resolved_event_group_id = op.event_group_id;
   if (is_relative(op.event_group_id))
      resolved_event_group_id = get_relative_id(op.event_group_id);

   FC_ASSERT(resolved_event_group_id.space() == event_group_id_type::space_id && 
             resolved_event_group_id.type() == event_group_id_type::type_id, 
             "event_group_id must refer to a event_group_id_type");
   event_group_id = resolved_event_group_id;
   const event_group_object& event_group = event_group_id(d);

   // Likewise for each competitor in the list
   for (const object_id_type& raw_competitor_id : op.competitors)
   {
      object_id_type resolved_competitor_id = raw_competitor_id;
      if (is_relative(raw_competitor_id))
         resolved_competitor_id = get_relative_id(raw_competitor_id);

      FC_ASSERT(resolved_competitor_id.space() == competitor_id_type::space_id && 
                resolved_competitor_id.type() == competitor_id_type::type_id, 
                "competitor must refer to a competitor_id_type");
      competitor_id_type competitor_id = resolved_competitor_id;
      const competitor_object& competitor = competitor_id(d);
      FC_ASSERT(competitor.sport_id == event_group.sport_id, 
                "competitor must compete in the same sport as the event they're competing in");
      competitors.push_back(competitor_id);
   }

   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type event_create_evaluator::do_apply(const event_create_operation& op)
{ try {
   const event_object& new_event =
     db().create<event_object>( [&]( event_object& event_obj ) {
         event_obj.name = op.name;
         event_obj.season = op.season;
         event_obj.start_time = op.start_time;
         event_obj.event_group_id = event_group_id;
         event_obj.competitors = competitors;
     });
   return new_event.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // graphene::chain
