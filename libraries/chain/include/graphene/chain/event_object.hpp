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
#include <graphene/chain/protocol/event.hpp>
#include <sstream>

#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {
   class event_object;
} }

namespace fc { 
   void to_variant(const graphene::chain::event_object& event_obj, fc::variant& v);
   void from_variant(const fc::variant& v, graphene::chain::event_object& event_obj);
} //end namespace fc

namespace graphene { namespace chain {

class database;

class event_object : public graphene::db::abstract_object< event_object >
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = event_object_type;

      event_object();
      event_object(const event_object& rhs);
      ~event_object();
      event_object& operator=(const event_object& rhs);

      internationalized_string_type name;
      
      internationalized_string_type season;

      optional<time_point_sec> start_time;

      event_group_id_type event_group_id;

      bool at_least_one_betting_market_group_settled;

      event_status get_status() const;
      vector<string> scores;

      // serialization functions:
      // for serializing to raw, go through a temporary sstream object to avoid
      // having to implement serialization in the header file
      template<typename Stream>
      friend Stream& operator<<( Stream& s, const event_object& event_obj );

      template<typename Stream>
      friend Stream& operator>>( Stream& s, event_object& event_obj );

      friend void ::fc::to_variant(const graphene::chain::event_object& event_obj, fc::variant& v);
      friend void ::fc::from_variant(const fc::variant& v, graphene::chain::event_object& event_obj);

      void pack_impl(std::ostream& stream) const;
      void unpack_impl(std::istream& stream);

      void on_upcoming_event(database& db);
      void on_in_progress_event(database& db);
      void on_frozen_event(database& db);
      void on_finished_event(database& db);
      void on_canceled_event(database& db);
      void on_settled_event(database& db);
      void on_betting_market_group_resolved(database& db, betting_market_group_id_type resolved_group, bool was_canceled);
      void on_betting_market_group_closed(database& db, betting_market_group_id_type closed_group);
      void dispatch_new_status(database& db, event_status new_status);
   private:
      class impl;
      std::unique_ptr<impl> my;
};

struct by_event_group_id;
struct by_event_status;
typedef multi_index_container<
   event_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_event_group_id>, composite_key<event_object,
                                                            member< event_object, event_group_id_type, &event_object::event_group_id >,
                                                            member<object, object_id_type, &object::id> > >,
      ordered_unique< tag<by_event_status>, composite_key<event_object,
                                                          const_mem_fun< event_object, event_status, &event_object::get_status >,
                                                          member<object, object_id_type, &object::id> > > > > event_object_multi_index_type;

typedef generic_index<event_object, event_object_multi_index_type> event_object_index;

   template<typename Stream>
   inline Stream& operator<<( Stream& s, const event_object& event_obj )
   { 
      fc_elog(fc::logger::get("event"), "In event_obj to_raw");
      // pack all fields exposed in the header in the usual way
      // instead of calling the derived pack, just serialize the one field in the base class
      //   fc::raw::pack<Stream, const graphene::db::abstract_object<event_object> >(s, event_obj);
      fc::raw::pack(s, event_obj.id);
      fc::raw::pack(s, event_obj.name);
      fc::raw::pack(s, event_obj.season);
      fc::raw::pack(s, event_obj.start_time);
      fc::raw::pack(s, event_obj.event_group_id);
      fc::raw::pack(s, event_obj.at_least_one_betting_market_group_settled);
      fc::raw::pack(s, event_obj.scores);

      // fc::raw::pack the contents hidden in the impl class
      std::ostringstream stream;
      event_obj.pack_impl(stream);
      std::string stringified_stream(stream.str());
      fc::raw::pack(s, stream.str());

      return s;
   }
   template<typename Stream>
   inline Stream& operator>>( Stream& s, event_object& event_obj )
   { 
      fc_elog(fc::logger::get("event"), "In event_obj from_raw");
      // unpack all fields exposed in the header in the usual way
      //fc::raw::unpack<Stream, graphene::db::abstract_object<event_object> >(s, event_obj);
      fc::raw::unpack(s, event_obj.id);
      fc::raw::unpack(s, event_obj.name);
      fc::raw::unpack(s, event_obj.season);
      fc::raw::unpack(s, event_obj.start_time);
      fc::raw::unpack(s, event_obj.event_group_id);
      fc::raw::unpack(s, event_obj.at_least_one_betting_market_group_settled);
      fc::raw::unpack(s, event_obj.scores);

      // fc::raw::unpack the contents hidden in the impl class
      std::string stringified_stream;
      fc::raw::unpack(s, stringified_stream);
      std::istringstream stream(stringified_stream);
      event_obj.unpack_impl(stream);
      
      return s;
   }
} } // graphene::chain
FC_REFLECT(graphene::chain::event_object, (name))

