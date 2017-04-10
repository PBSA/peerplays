#include <graphene/chain/protocol/tournament.hpp>
#include <graphene/chain/tournament_object.hpp>
#include <graphene/chain/game_object.hpp>
#include <graphene/chain/tournament_evaluator.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
namespace graphene { namespace chain {

   void_result tournament_create_evaluator::do_evaluate( const tournament_create_operation& op )
   { try {
      database& d = db();
      FC_ASSERT(op.options.registration_deadline >= d.head_block_time(), "Registration deadline has already passed");

      // TODO: make this committee-set
      const fc::time_point_sec maximum_registration_deadline = d.head_block_time() + d.get_global_properties().parameters.maximum_registration_deadline;
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
      FC_ASSERT(op.options.whitelist.empty() || 
                op.options.whitelist.size() >= op.options.number_of_players, 
                "Whitelist must allow enough players to fill the tournament");
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
      const uint32_t maximum_tournament_number_of_wins = 100;
      FC_ASSERT(op.options.number_of_wins > 0);
      FC_ASSERT(op.options.number_of_wins <= maximum_tournament_number_of_wins, 
                "Matches may not require more than ${number_of_wins} wins", 
                ("number_of_wins", maximum_tournament_number_of_wins));

      // round_delay constraints
      const uint32_t minimum_round_delay = d.get_global_properties().parameters.min_round_delay;
      FC_ASSERT(op.options.round_delay >= minimum_round_delay,
                "Delay between games must not be less then ${min}",
                ("min", minimum_round_delay));
      const uint32_t maximum_round_delay = d.get_global_properties().parameters.max_round_delay;
      FC_ASSERT(op.options.round_delay <= maximum_round_delay,
                "Delay between games must not be greater then ${max}",
                ("max", maximum_round_delay));

      const rock_paper_scissors_game_options& game_options = op.options.game_options.get<rock_paper_scissors_game_options>();

      // time_per_commit_move constraints
      const uint32_t minimum_time_per_commit_move = d.get_global_properties().parameters.min_time_per_commit_move;
      FC_ASSERT(game_options.time_per_commit_move >= minimum_time_per_commit_move,
                "Time to commit the next move must not be less than ${min}",
                ("min", minimum_time_per_commit_move));
      const uint32_t maximum_time_per_commit_move = d.get_global_properties().parameters.max_time_per_commit_move;
      FC_ASSERT(game_options.time_per_commit_move <= maximum_time_per_commit_move,
                "Time to commit the next move must not be greater than ${max}",
                ("max", maximum_time_per_commit_move));

      // time_per_commit_reveal constraints
      const uint32_t minimum_time_per_reveal_move = d.get_global_properties().parameters.min_time_per_reveal_move;
      FC_ASSERT(game_options.time_per_reveal_move >= minimum_time_per_reveal_move,
                "Time to reveal the move must not be less than ${min}",
                ("min", minimum_time_per_reveal_move));
      const uint32_t maximum_time_per_reveal_move = d.get_global_properties().parameters.max_time_per_reveal_move;
      FC_ASSERT(game_options.time_per_reveal_move <= maximum_time_per_reveal_move,
                "Time to reveal the move must not be greater than ${max}",
                ("max", maximum_time_per_reveal_move));

      //cli-wallet supports 5 gesture games as well, but limit to 3 now as GUI wallet only supports 3 gesture games currently
      FC_ASSERT(game_options.number_of_gestures == 3,
                "GUI Wallet only supports 3 gestures currently");

      return void_result();
   } FC_CAPTURE_AND_RETHROW( (op) ) }
   
   object_id_type tournament_create_evaluator::do_apply( const tournament_create_operation& op )
   { try {
      const tournament_details_object& tournament_details =
        db().create<tournament_details_object>( [&]( tournament_details_object& a ) {
        });

      const tournament_object& new_tournament =
        db().create<tournament_object>( [&]( tournament_object& t ) {
            t.options = op.options;
            t.creator = op.creator;
            t.tournament_details_id = tournament_details.id;
          });

      // TODO: look up how to do this in the initial create
      db().modify(tournament_details, [&]( tournament_details_object& a ) {
            a.tournament_id = new_tournament.id;
          });


      fc_ilog(fc::logger::get("tournament"),
              "Created tournament ${id} with details id ${details_id}", 
              ("id", new_tournament.id)("details_id", tournament_details.id));
      return new_tournament.id;
   } FC_CAPTURE_AND_RETHROW( (op) ) }
   
   void_result tournament_join_evaluator::do_evaluate( const tournament_join_operation& op )
   { try {
      const database& d = db();
      _tournament_obj = &op.tournament_id(d);
      fc_ilog(fc::logger::get("tournament"), "details_id = ${id}",("id", _tournament_obj->tournament_details_id));
      _tournament_details_obj = &_tournament_obj->tournament_details_id(d);
      _payer_account = &op.payer_account_id(d);
      //const account_object& player_account = op.player_account_id(d);
      _buy_in_asset_type = &op.buy_in.asset_id(d);

      FC_ASSERT(_tournament_obj->get_state() == tournament_state::accepting_registrations);
      FC_ASSERT(_tournament_details_obj->registered_players.size() < _tournament_obj->options.number_of_players,
                "Tournament is already full");
      FC_ASSERT(d.head_block_time() <= _tournament_obj->options.registration_deadline, 
                "Registration deadline has already passed");

      FC_ASSERT(_tournament_obj->options.whitelist.empty() ||
                _tournament_obj->options.whitelist.find(op.player_account_id) != _tournament_obj->options.whitelist.end(),
                "Player is not on the whitelist for this tournament");

      FC_ASSERT(_tournament_details_obj->registered_players.find(op.player_account_id) == _tournament_details_obj->registered_players.end(),
                "Player is already registered for this tournament");
      FC_ASSERT(op.buy_in == _tournament_obj->options.buy_in, "Buy-in is incorrect");

      GRAPHENE_ASSERT(!_buy_in_asset_type->is_transfer_restricted(),
                      transfer_restricted_transfer_asset,
                      "Asset {asset} has transfer_restricted flag enabled",
                      ("asset", op.buy_in.asset_id));

      GRAPHENE_ASSERT(is_authorized_asset(d, *_payer_account, *_buy_in_asset_type),
                      transfer_from_account_not_whitelisted,
                      "payer account ${payer} is not whitelisted for asset ${asset}",
                      ("payer", op.payer_account_id)
                      ("asset", op.buy_in.asset_id));

      bool sufficient_balance = d.get_balance(*_payer_account, *_buy_in_asset_type).amount >= op.buy_in.amount;
      FC_ASSERT(sufficient_balance,
                "Insufficient Balance: paying account '${payer}' has insufficient balance to pay buy-in of ${buy_in} (balance is ${balance})", 
                ("payer", _payer_account->name)
                ("buy_in", d.to_pretty_string(op.buy_in))
                ("balance",d.to_pretty_string(d.get_balance(*_payer_account, *_buy_in_asset_type))));
      return void_result();
   } FC_CAPTURE_AND_RETHROW( (op) ) }
   
   void_result tournament_join_evaluator::do_apply( const tournament_join_operation& op )
   { try {
      db().modify(*_tournament_obj, [&](tournament_object& tournament_obj){
            tournament_obj.on_player_registered(db(), op.payer_account_id, op.player_account_id);
         });
      return void_result();
   } FC_CAPTURE_AND_RETHROW( (op) ) }
   
   void_result game_move_evaluator::do_evaluate( const game_move_operation& o )
   { try {
      const database& d = db();
      _game_obj = &o.game_id(d);
      _game_obj->evaluate_move_operation(d, o);
      return void_result();
   } FC_CAPTURE_AND_RETHROW( (o) ) }

   void_result game_move_evaluator::do_apply( const game_move_operation& o )
   { try {
      db().modify(*_game_obj, [&](game_object& game_obj){
        game_obj.on_move(db(), o);
         });
      return void_result();
   } FC_CAPTURE_AND_RETHROW( (o) ) }
} }


