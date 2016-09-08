#pragma once
#include <graphene/chain/protocol/tournament.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <graphene/db/flat_index.hpp>
#include <graphene/db/generic_index.hpp>
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
      static const uint8_t type_id  = impl_tournament_details_object_type;

      /// List of players registered for this tournament
      flat_set<account_id_type> registered_players;

      /// List of payers who have contributed to the prize pool
      flat_map<account_id_type, share_type> payers;
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

      /// called by database maintenance code when registration for this contest has expired
      void on_registration_deadline_passed(database& db);
      void on_player_registered(database& db, account_id_type payer_id, account_id_type player_id);
      void on_start_time_arrived();
      void on_final_game_completed();

   private:
      class impl;
      std::unique_ptr<impl> my;
   };

   class match_object : public graphene::db::abstract_object<match_object>
   {
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = match_object_type;

      /// 
      flat_set<account_id_type> players;

      vector<flat_set<account_id_type> > game_winners;
   };
      
   class game_object : public graphene::db::abstract_object<game_object>
   {
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = game_object_type;

      flat_set<account_id_type> players;

      flat_set<account_id_type> winners;
   };



   struct by_registration_deadline {};
   struct by_start_time {};
   typedef multi_index_container<
      tournament_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_non_unique< tag<by_registration_deadline>, 
            composite_key<tournament_object, 
               const_mem_fun<tournament_object, tournament_state, &tournament_object::get_state>, 
               const_mem_fun<tournament_object, time_point_sec, &tournament_object::get_registration_deadline> > >,
         ordered_non_unique< tag<by_start_time>, 
            composite_key<tournament_object, 
               const_mem_fun<tournament_object, tournament_state, &tournament_object::get_state>, 
               member<tournament_object, optional<time_point_sec>, &tournament_object::start_time> > >
      >
   > tournament_object_multi_index_type;
   typedef generic_index<tournament_object, tournament_object_multi_index_type> tournament_index;

} }

FC_REFLECT_DERIVED(graphene::chain::tournament_details_object, (graphene::db::object),
                   (registered_players)
                   (payers))
FC_REFLECT_DERIVED(graphene::chain::tournament_object, (graphene::db::object),
                   (creator)
                   (options)
                   (start_time)
                   (end_time)
                   (prize_pool)
                   (tournament_details_id))
FC_REFLECT_ENUM(graphene::chain::tournament_state,
                (accepting_registrations)
                (awaiting_start)
                (in_progress)
                (registration_period_expired)
                (concluded))

