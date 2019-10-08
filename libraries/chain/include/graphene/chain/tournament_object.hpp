#pragma once
#include <graphene/chain/protocol/tournament.hpp>
#include <graphene/chain/rock_paper_scissors.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <graphene/db/flat_index.hpp>
#include <graphene/db/generic_index.hpp>
#include <fc/crypto/hex.hpp>
#include <sstream>

namespace graphene { namespace chain {
   class tournament_object;
} }

namespace fc { 
   void to_variant(const graphene::chain::tournament_object& tournament_obj, fc::variant& v);
   void from_variant(const fc::variant& v, graphene::chain::tournament_object& tournament_obj);
} //end namespace fc

namespace graphene { namespace chain {
   class database;
   using namespace graphene::db;

   /// The tournament object has a lot of details, most of which are only of interest to anyone
   /// involved in the tournament.  The main `tournament_object` contains all of the information
   /// needed to display an overview of the tournament, this object contains the rest.
   class tournament_details_object : public graphene::db::abstract_object<tournament_details_object>
   {
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = tournament_details_object_type;

      /// the tournament object for which this is the details
      tournament_id_type tournament_id;

      /// List of players registered for this tournament
      flat_set<account_id_type> registered_players;

      /// List of payers who have contributed to the prize pool
      flat_map<account_id_type, share_type> payers;

      /// List of player payer pairs needed by torunament leave operation
      flat_map<account_id_type, account_id_type> players_payers;

      /// List of all matches in this tournament.  When the tournament starts, all matches
      /// are created.  Matches in the first round will have players, matches in later
      /// rounds will not be populated.
      vector<match_id_type> matches;
   };

   enum class tournament_state
   {
      accepting_registrations,
      awaiting_start,
      in_progress,
      registration_period_expired,
      concluded
   };

   class tournament_object : public graphene::db::abstract_object<tournament_object>
   {
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = tournament_object_type;

      tournament_object();
      tournament_object(const tournament_object& rhs);
      ~tournament_object();
      tournament_object& operator=(const tournament_object& rhs);
      
      tournament_id_type get_id() const { return id; };
      /// the account that created this tournament
      account_id_type creator;

      /// the options set when creating the tournament 
      tournament_options options;

      /// If the tournament has started, the time it started
      optional<time_point_sec> start_time;
      /// If the tournament has ended, the time it ended
      optional<time_point_sec> end_time;

      /// Total prize pool accumulated 
      /// This is the sum of all payers in the details object, and will be
      /// registered_players.size() * buy_in_amount
      share_type prize_pool;

      /// The number of players registered for the tournament
      /// (same as the details object's registered_players.size(), here to avoid
      /// the GUI having to get the details object)
      uint32_t registered_players = 0;

      /// The current high-level status of the tournament (whether it is currently running or has been canceled, etc)
      //tournament_state state;

      /// Detailed information on this tournament
      tournament_details_id_type tournament_details_id;

      tournament_state get_state() const;

      time_point_sec get_registration_deadline() const { return options.registration_deadline; }

      // serialization functions:
      // for serializing to raw, go through a temporary sstream object to avoid
      // having to implement serialization in the header file
      template<typename Stream>
      friend Stream& operator<<( Stream& s, const tournament_object& tournament_obj );

      template<typename Stream>
      friend Stream& operator>>( Stream& s, tournament_object& tournament_obj );

      friend void ::fc::to_variant(const graphene::chain::tournament_object& tournament_obj, fc::variant& v);
      friend void ::fc::from_variant(const fc::variant& v, graphene::chain::tournament_object& tournament_obj);

      void pack_impl(std::ostream& stream) const;
      void unpack_impl(std::istream& stream);

      /// called by database maintenance code when registration for this contest has expired
      void on_registration_deadline_passed(database& db);
      void on_player_registered(database& db, account_id_type payer_id, account_id_type player_id);
      void on_player_unregistered(database& db, account_id_type player_id);
      void on_start_time_arrived(database& db);
      void on_match_completed(database& db, const match_object& match);

      void check_for_new_matches_to_start(database& db) const;
   private:
      class impl;
      std::unique_ptr<impl> my;
   };

   struct by_registration_deadline {};
   struct by_start_time {};
   typedef multi_index_container<
      tournament_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_unique< tag<by_registration_deadline>, 
            composite_key<tournament_object, 
               const_mem_fun<tournament_object, tournament_state, &tournament_object::get_state>, 
               const_mem_fun<tournament_object, time_point_sec, &tournament_object::get_registration_deadline>,
               member< object, object_id_type, &object::id > > >,
         ordered_unique< tag<by_start_time>, 
            composite_key<tournament_object, 
               const_mem_fun<tournament_object, tournament_state, &tournament_object::get_state>, 
               member<tournament_object, optional<time_point_sec>, &tournament_object::start_time>,
               member< object, object_id_type, &object::id > > >
      >
   > tournament_object_multi_index_type;
   typedef generic_index<tournament_object, tournament_object_multi_index_type> tournament_index;

   typedef multi_index_container<
      tournament_details_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >      >
   > tournament_details_object_multi_index_type;
   typedef generic_index<tournament_details_object, tournament_details_object_multi_index_type> tournament_details_index;


   template<typename Stream>
   inline Stream& operator<<( Stream& s, const tournament_object& tournament_obj )
   { 
      fc_elog(fc::logger::get("tournament"), "In tournament_obj to_raw");
      // pack all fields exposed in the header in the usual way
      // instead of calling the derived pack, just serialize the one field in the base class
      //   fc::raw::pack<Stream, const graphene::db::abstract_object<tournament_object> >(s, tournament_obj);
      fc::raw::pack(s, tournament_obj.id);
      fc::raw::pack(s, tournament_obj.creator);
      fc::raw::pack(s, tournament_obj.options);
      fc::raw::pack(s, tournament_obj.start_time);
      fc::raw::pack(s, tournament_obj.end_time);
      fc::raw::pack(s, tournament_obj.prize_pool);
      fc::raw::pack(s, tournament_obj.registered_players);
      fc::raw::pack(s, tournament_obj.tournament_details_id);

      // fc::raw::pack the contents hidden in the impl class
      std::ostringstream stream;
      tournament_obj.pack_impl(stream);
      std::string stringified_stream(stream.str());
      fc_elog(fc::logger::get("tournament"), "Serialized state ${state} to bytes ${bytes}", 
              ("state", tournament_obj.get_state())("bytes", fc::to_hex(stringified_stream.c_str(), stringified_stream.size())));
      fc::raw::pack(s, stream.str());

      return s;
   }
   template<typename Stream>
   inline Stream& operator>>( Stream& s, tournament_object& tournament_obj )
   { 
      fc_elog(fc::logger::get("tournament"), "In tournament_obj from_raw");
      // unpack all fields exposed in the header in the usual way
      //fc::raw::unpack<Stream, graphene::db::abstract_object<tournament_object> >(s, tournament_obj);
      fc::raw::unpack(s, tournament_obj.id);
      fc::raw::unpack(s, tournament_obj.creator);
      fc::raw::unpack(s, tournament_obj.options);
      fc::raw::unpack(s, tournament_obj.start_time);
      fc::raw::unpack(s, tournament_obj.end_time);
      fc::raw::unpack(s, tournament_obj.prize_pool);
      fc::raw::unpack(s, tournament_obj.registered_players);
      fc::raw::unpack(s, tournament_obj.tournament_details_id);

      // fc::raw::unpack the contents hidden in the impl class
      std::string stringified_stream;
      fc::raw::unpack(s, stringified_stream);
      std::istringstream stream(stringified_stream);
      tournament_obj.unpack_impl(stream);
      fc_elog(fc::logger::get("tournament"), "Deserialized state ${state} from bytes ${bytes}", 
              ("state", tournament_obj.get_state())("bytes", fc::to_hex(stringified_stream.c_str(), stringified_stream.size())));
      
      return s;
   }

   /**
    *  @brief This secondary index will allow a reverse lookup of all tournaments 
    *  a particular account has registered for.  This will be attached
    *  to the tournament details index because the registrations are contained
    *  in the tournament details object, but it will index the tournament ids
    *  since that is most useful to the GUI.
    */
   class tournament_players_index : public secondary_index
   {
      public:
         virtual void object_inserted( const object& obj ) override;
         virtual void object_removed( const object& obj ) override;
         virtual void about_to_modify( const object& before ) override;
         virtual void object_modified( const object& after  ) override;

         /** given an account, map it to the set of tournaments in which that account is registered as a player */
         map< account_id_type, flat_set<tournament_id_type> > account_to_joined_tournaments;

         vector<tournament_id_type> get_registered_tournaments_for_account( const account_id_type& a )const;
      protected:

         flat_set<account_id_type> before_account_ids;
   };


} }

FC_REFLECT_DERIVED(graphene::chain::tournament_details_object, (graphene::db::object),
                   (tournament_id)
                   (registered_players)
                   (payers)
                   (players_payers)
                   (matches))
//FC_REFLECT_TYPENAME(graphene::chain::tournament_object) // manually serialized
FC_REFLECT(graphene::chain::tournament_object, (creator))
FC_REFLECT_ENUM(graphene::chain::tournament_state,
                (accepting_registrations)
                (awaiting_start)
                (in_progress)
                (registration_period_expired)
                (concluded))

