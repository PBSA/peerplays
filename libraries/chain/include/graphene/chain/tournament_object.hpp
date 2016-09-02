#pragma once
#include <graphene/chain/protocol/tournament.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <graphene/db/flat_index.hpp>
#include <graphene/db/generic_index.hpp>
namespace graphene { namespace chain {
   class database;
   using namespace graphene::db;

   class tournament_object : public graphene::db::abstract_object<tournament_object>
   {
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id  = tournament_object_type;
      
      /// the account that created this tournament
      account_id_type creator;

      /// the options set when creating the tournament 
      tournament_options options;

      /// If the tournament has started, the time it started
      optional<time_point_sec> start_time;
      /// If the tournament has ended, the time it ended
      optional<time_point_sec> end_time;

      /// List of players registered for this tournament
      flat_set<account_id_type> registered_players;

      /// List of payers who have contributed to the prize pool
      flat_map<account_id_type, share_type> payers;

      /// Total prize pool accumulated ((sum of buy_ins, usually registered_players.size() * buy_in_amount)
      asset prize_pool;
   };

   typedef multi_index_container<
      tournament_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >
      >
   > tournament_object_multi_index_type;
   typedef generic_index<tournament_object, tournament_object_multi_index_type> tournament_index;

} }

FC_REFLECT_DERIVED( graphene::chain::tournament_object, (graphene::db::object),
                    (options)
                    (start_time)
                    (end_time) )

