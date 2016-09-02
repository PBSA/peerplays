#include <graphene/chain/protocol/tournament.hpp>
#include <graphene/chain/tournament_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
namespace graphene { namespace chain {

   void_result tournament_create_evaluator::do_evaluate( const tournament_create_operation& op )
   { try {
      database& d = db();
      FC_ASSERT(op.options.type_of_game == rock_paper_scissors, "Unsupported game type ${type}", ("type", op.options.type_of_game));

      FC_ASSERT(op.options.registration_deadline >= d.head_block_time(), "Registration deadline has already passed");

      // TODO: make this committee-set
      const fc::time_point_sec maximum_registration_deadline = d.head_block_time() + fc::days(30);
      FC_ASSERT(op.options.registration_deadline <= maximum_registration_deadline, 
                "Registration deadline must be before ${maximum_registration_deadline}", 
                ("maximum_registration_deadline", maximum_registration_deadline));

      FC_ASSERT(op.options.number_of_players > 1, "If you're going to play with yourself, do it off-chain");
      // TODO: make this committee-set
      const uint32_t maximum_players_in_tournament = 256;
      FC_ASSERT(op.options.number_of_players <= maximum_players_in_tournament, 
                "Tournaments may not have more than ${maximum_players_in_tournament} players", 
                ("maximum_players_in_tournament", maximum_players_in_tournament));

      // TODO: make this committee-set
      const uint32_t maximum_tournament_whitelist_length = 1000;
      FC_ASSERT(op.options.whitelist.size() != 1, "Can't create a tournament for one player");
      FC_ASSERT(op.options.whitelist.size() < maximum_tournament_whitelist_length, 
                "Whitelist must not be longer than ${maximum_tournament_whitelist_length}",
                ("maximum_tournament_whitelist_length", maximum_tournament_whitelist_length));
      
      if (op.options.start_time)
      {
          FC_ASSERT(!op.options.start_delay, "Cannot specify both a fixed start time and a delay");
          FC_ASSERT(*op.options.start_time >= op.options.registration_deadline, 
                    "Cannot start before registration deadline expires");
          // TODO: make this committee-set
          const uint32_t maximum_start_time_in_future = 60 * 60 * 24 * 7 * 4; // 1 month
          FC_ASSERT((*op.options.start_time - d.head_block_time()).to_seconds() <= maximum_start_time_in_future,
                    "Start time is too far in the future");
      }
      else if (op.options.start_delay)
      {
          FC_ASSERT(!op.options.start_time, "Cannot specify both a fixed start time and a delay");
          // TODO: make this committee-set
          const uint32_t maximum_start_delay = 60 * 60 * 24 * 7; // 1 week
          FC_ASSERT(*op.options.start_delay < maximum_start_delay, 
                    "Start delay is too long");
      }
      else
          FC_THROW("Must specify either a fixed start time or a delay");

      // TODO: make this committee-set
      const uint32_t maximum_round_delay = 60 * 60; // one hour
      FC_ASSERT(op.options.round_delay < maximum_round_delay, 
                "Round delay is too long");

      // TODO: make this committee-set
      const uint32_t maximum_tournament_number_of_wins = 100;
      FC_ASSERT(op.options.number_of_wins > 0);
      FC_ASSERT(op.options.number_of_wins <= maximum_tournament_number_of_wins, 
                "Matches may not require more than ${number_of_wins} wins", 
                ("number_of_wins", maximum_tournament_number_of_wins));

      return void_result();
   } FC_CAPTURE_AND_RETHROW( (op) ) }
   
   object_id_type tournament_create_evaluator::do_apply( const tournament_create_operation& op )
   { try {
   } FC_CAPTURE_AND_RETHROW( (op) ) }
   
   void_result tournament_join_evaluator::do_evaluate( const tournament_join_operation& op )
   { try {
      return void_result();
   } FC_CAPTURE_AND_RETHROW( (op) ) }
   
   object_id_type tournament_join_evaluator::do_apply( const tournament_join_operation& op )
   { try {
   } FC_CAPTURE_AND_RETHROW( (op) ) }
   
} }


