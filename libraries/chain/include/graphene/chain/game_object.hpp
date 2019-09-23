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
#include <graphene/chain/match_object.hpp>
#include <graphene/chain/rock_paper_scissors.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <graphene/db/flat_index.hpp>
#include <graphene/db/generic_index.hpp>
#include <fc/crypto/hex.hpp>
#include <sstream>

namespace graphene { namespace chain {
   class game_object;
} }

namespace fc { 
   void to_variant(const graphene::chain::game_object& game_obj, fc::variant& v, uint32_t max_depth = 1);
   void from_variant(const fc::variant& v, graphene::chain::game_object& game_obj, uint32_t max_depth = 1);
} //end namespace fc

namespace graphene { namespace chain {
   class database;
   using namespace graphene::db;

   enum class game_state
   {
      game_in_progress,
      expecting_commit_moves,
      expecting_reveal_moves,
      game_complete
   };

   class game_object : public graphene::db::abstract_object<game_object>
   {
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = game_object_type;

      match_id_type match_id;

      vector<account_id_type> players;

      flat_set<account_id_type> winners;

      game_specific_details game_details;

      fc::optional<time_point_sec> next_timeout;
      
      game_state get_state() const;

      game_object();
      game_object(const game_object& rhs);
      ~game_object();
      game_object& operator=(const game_object& rhs);

      void evaluate_move_operation(const database& db, const game_move_operation& op) const;
      void make_automatic_moves(database& db);
      void determine_winner(database& db);

      void on_move(database& db, const game_move_operation& op);
      void on_timeout(database& db);
      void start_game(database& db, const std::vector<account_id_type>& players);
      
      // serialization functions:
      // for serializing to raw, go through a temporary sstream object to avoid
      // having to implement serialization in the header file
      template<typename Stream>
      friend Stream& operator<<( Stream& s, const game_object& game_obj );

      template<typename Stream>
      friend Stream& operator>>( Stream& s, game_object& game_obj );

      friend void ::fc::to_variant(const graphene::chain::game_object& game_obj, fc::variant& v, uint32_t max_depth);
      friend void ::fc::from_variant(const fc::variant& v, graphene::chain::game_object& game_obj, uint32_t max_depth);

      void pack_impl(std::ostream& stream) const;
      void unpack_impl(std::istream& stream);

      class impl;
      std::unique_ptr<impl> my;
   };

   struct by_next_timeout {};
   typedef multi_index_container<
      game_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_unique< tag<by_next_timeout>, 
            composite_key<game_object, 
               member<game_object, optional<time_point_sec>, &game_object::next_timeout>,
               member<object, object_id_type, &object::id> > > >
   > game_object_multi_index_type;
   typedef generic_index<game_object, game_object_multi_index_type> game_index;

   template<typename Stream>
   inline Stream& operator<<( Stream& s, const game_object& game_obj )
   { 
      // pack all fields exposed in the header in the usual way
      // instead of calling the derived pack, just serialize the one field in the base class
      //   fc::raw::pack<Stream, const graphene::db::abstract_object<game_object> >(s, game_obj);
      fc::raw::pack(s, game_obj.id);
      fc::raw::pack(s, game_obj.match_id);
      fc::raw::pack(s, game_obj.players);
      fc::raw::pack(s, game_obj.winners);
      fc::raw::pack(s, game_obj.game_details);
      fc::raw::pack(s, game_obj.next_timeout);

      // fc::raw::pack the contents hidden in the impl class
      std::ostringstream stream;
      game_obj.pack_impl(stream);
      std::string stringified_stream(stream.str());
      fc::raw::pack(s, stream.str());

      return s;
   }

   template<typename Stream>
   inline Stream& operator>>( Stream& s, game_object& game_obj )
   { 
      // unpack all fields exposed in the header in the usual way
      //fc::raw::unpack<Stream, graphene::db::abstract_object<game_object> >(s, game_obj);
      fc::raw::unpack(s, game_obj.id);
      fc::raw::unpack(s, game_obj.match_id);
      fc::raw::unpack(s, game_obj.players);
      fc::raw::unpack(s, game_obj.winners);
      fc::raw::unpack(s, game_obj.game_details);
      fc::raw::unpack(s, game_obj.next_timeout);

      // fc::raw::unpack the contents hidden in the impl class
      std::string stringified_stream;
      fc::raw::unpack(s, stringified_stream);
      std::istringstream stream(stringified_stream);
      game_obj.unpack_impl(stream);
      
      return s;
   }

} }

FC_REFLECT_ENUM(graphene::chain::game_state,
                (game_in_progress)
                (expecting_commit_moves)
                (expecting_reveal_moves)
                (game_complete))

//FC_REFLECT_TYPENAME(graphene::chain::game_object) // manually serialized
FC_REFLECT(graphene::chain::game_object, (players))


