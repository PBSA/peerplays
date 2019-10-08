#pragma once
#include <graphene/chain/protocol/tournament.hpp>
#include <graphene/chain/rock_paper_scissors.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <graphene/db/flat_index.hpp>
#include <graphene/db/generic_index.hpp>
#include <fc/crypto/hex.hpp>
#include <sstream>

namespace graphene { namespace chain {
   class match_object;
} }

namespace fc { 
   void to_variant(const graphene::chain::match_object& match_obj, fc::variant& v, uint32_t max_depth = 1);
   void from_variant(const fc::variant& v, graphene::chain::match_object& match_obj, uint32_t max_depth = 1);
} //end namespace fc

namespace graphene { namespace chain {
   class database;
   using namespace graphene::db;

   enum class match_state
   {
      waiting_on_previous_matches,
      match_in_progress,
      match_complete
   };

   class match_object : public graphene::db::abstract_object<match_object>
   {
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = match_object_type;

      tournament_id_type tournament_id;

      /// The players in the match
      vector<account_id_type> players;

      /// The list of games in the match
      /// Unlike tournaments where the list of matches is known at the start,
      /// the list of games will start with one game and grow until we have played
      /// enough games to declare a winner for the match.
      vector<game_id_type> games;

      /// A list of the winners of each round of the game.  This information is 
      /// also stored in the game object, but is duplicated here to allow displaying
      /// information about a match without having to request all game objects
      vector<flat_set<account_id_type> > game_winners;

      /// A count of the number of wins for each player
      vector<uint32_t> number_of_wins;

      /// the total number of games that ended up in a tie/draw/stalemate
      uint32_t number_of_ties;

      // If the match is not yet complete, this will be empty
      // If the match is in the "match_complete" state, it will contain the
      // list of winners.
      // For Rock-paper-scissors, there will be one winner, unless there is
      // a stalemate (in that case, there are no winners)
      flat_set<account_id_type> match_winners;

      /// the time the match started
      time_point_sec start_time;

      /// If the match has ended, the time it ended
      optional<time_point_sec> end_time;

      match_object();
      match_object(const match_object& rhs);
      ~match_object();
      match_object& operator=(const match_object& rhs);
      
      match_state get_state() const;

      // serialization functions:
      // for serializing to raw, go through a temporary sstream object to avoid
      // having to implement serialization in the header file
      template<typename Stream>
      friend Stream& operator<<( Stream& s, const match_object& match_obj );

      template<typename Stream>
      friend Stream& operator>>( Stream& s, match_object& match_obj );

      friend void ::fc::to_variant(const graphene::chain::match_object& match_obj, fc::variant& v, uint32_t max_depth);
      friend void ::fc::from_variant(const fc::variant& v, graphene::chain::match_object& match_obj, uint32_t max_depth);

      void pack_impl(std::ostream& stream) const;
      void unpack_impl(std::istream& stream);
      void on_initiate_match(database& db);
      void on_game_complete(database& db, const game_object& game);
      game_id_type start_next_game(database& db, match_id_type match_id);

      class impl;
      std::unique_ptr<impl> my;
   };
      
   typedef multi_index_container<
      match_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >      >
   > match_object_multi_index_type;
   typedef generic_index<match_object, match_object_multi_index_type> match_index;

   template<typename Stream>
   inline Stream& operator<<( Stream& s, const match_object& match_obj )
   { 
      // pack all fields exposed in the header in the usual way
      // instead of calling the derived pack, just serialize the one field in the base class
      //   fc::raw::pack<Stream, const graphene::db::abstract_object<match_object> >(s, match_obj);
      fc::raw::pack(s, match_obj.id);
      fc::raw::pack(s, match_obj.tournament_id);
      fc::raw::pack(s, match_obj.players);
      fc::raw::pack(s, match_obj.games);
      fc::raw::pack(s, match_obj.game_winners);
      fc::raw::pack(s, match_obj.number_of_wins);
      fc::raw::pack(s, match_obj.number_of_ties);
      fc::raw::pack(s, match_obj.match_winners);
      fc::raw::pack(s, match_obj.start_time);
      fc::raw::pack(s, match_obj.end_time);

      // fc::raw::pack the contents hidden in the impl class
      std::ostringstream stream;
      match_obj.pack_impl(stream);
      std::string stringified_stream(stream.str());
      fc::raw::pack(s, stream.str());

      return s;
   }

   template<typename Stream>
   inline Stream& operator>>( Stream& s, match_object& match_obj )
   { 
      // unpack all fields exposed in the header in the usual way
      //fc::raw::unpack<Stream, graphene::db::abstract_object<match_object> >(s, match_obj);
      fc::raw::unpack(s, match_obj.id);
      fc::raw::unpack(s, match_obj.tournament_id);
      fc::raw::unpack(s, match_obj.players);
      fc::raw::unpack(s, match_obj.games);
      fc::raw::unpack(s, match_obj.game_winners);
      fc::raw::unpack(s, match_obj.number_of_wins);
      fc::raw::unpack(s, match_obj.number_of_ties);
      fc::raw::unpack(s, match_obj.match_winners);
      fc::raw::unpack(s, match_obj.start_time);
      fc::raw::unpack(s, match_obj.end_time);

      // fc::raw::unpack the contents hidden in the impl class
      std::string stringified_stream;
      fc::raw::unpack(s, stringified_stream);
      std::istringstream stream(stringified_stream);
      match_obj.unpack_impl(stream);
      
      return s;
   }

} }

FC_REFLECT_ENUM(graphene::chain::match_state,
                (waiting_on_previous_matches)
                (match_in_progress)
                (match_complete))

//FC_REFLECT_TYPENAME(graphene::chain::match_object) // manually serialized
FC_REFLECT(graphene::chain::match_object, (players))

