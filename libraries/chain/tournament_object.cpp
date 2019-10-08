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
#include <graphene/chain/database.hpp>
#include <graphene/chain/tournament_object.hpp>
#include <graphene/chain/match_object.hpp>
#include <graphene/chain/affiliate_payout.hpp>

#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/msm/back/tools.hpp>

namespace graphene { namespace chain {

   namespace msm = boost::msm;
   namespace mpl = boost::mpl;

   namespace 
   {
      // Events
      struct player_registered
      {
         database& db;
         account_id_type payer_id;
         account_id_type player_id;
         player_registered(database& db, account_id_type payer_id, account_id_type player_id) : 
            db(db), payer_id(payer_id), player_id(player_id) 
         {}
      };
      struct player_unregistered
      {
         database& db;
         account_id_type player_id;
         player_unregistered(database& db, account_id_type player_id) :
            db(db), player_id(player_id)
         {}
      };
      struct registration_deadline_passed
      {
         database& db;
         registration_deadline_passed(database& db) : db(db) {};
      };
      struct start_time_arrived 
      {
         database& db;
         start_time_arrived(database& db) : db(db) {};
      };

      struct match_completed 
      {
         database& db;
         const match_object& match;
         match_completed(database& db, const match_object& match) : db(db), match(match) {}
      };

      struct tournament_state_machine_ : public msm::front::state_machine_def<tournament_state_machine_>
      {
         // disable a few state machine features we don't use for performance
         typedef int no_exception_thrown;
         typedef int no_message_queue;

         // States
         struct accepting_registrations : public msm::front::state<>{};
         struct awaiting_start : public msm::front::state<>
         {
            void on_entry(const player_registered& event, tournament_state_machine_& fsm)
            {
               fc_ilog(fc::logger::get("tournament"),
                       "Tournament ${id} now has enough players registered to begin",
                       ("id", fsm.tournament_obj->id));
               if (fsm.tournament_obj->options.start_time)
                  fsm.tournament_obj->start_time = fsm.tournament_obj->options.start_time;
               else
                  fsm.tournament_obj->start_time = event.db.head_block_time() + fc::seconds(*fsm.tournament_obj->options.start_delay);
            }
         };
         struct in_progress : public msm::front::state<>
         {
            // reverse the bits in an integer
            static uint32_t reverse_bits(uint32_t x)
            {
               x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
               x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
               x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
               x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
               return ((x >> 16) | (x << 16));
            }

            match_id_type create_match(database& db, tournament_id_type tournament_id, 
                                       const vector<account_id_type>& players)
            {
               const match_object& match =
                  db.create<match_object>( [&]( match_object& match ) {
                     match.tournament_id = tournament_id;
                     match.players = players;
                     match.number_of_wins.resize(match.players.size());
                     match.start_time = db.head_block_time();
                     if (match.players.size() == 1)
                     {
                        // this is a bye
                        match.end_time = db.head_block_time();
                     }
                  });
               return match.id;
            }
            
            void on_entry(const start_time_arrived& event, tournament_state_machine_& fsm)
            {
               fc_ilog(fc::logger::get("tournament"),
                       "Tournament ${id} is beginning",
                       ("id", fsm.tournament_obj->id));
               const tournament_details_object& tournament_details_obj = fsm.tournament_obj->tournament_details_id(event.db);

               // Create the "seeding" order for the tournament as a random shuffle of the players.
               //
               // If this were a game of skill where players were ranked, this algorithm expects the 
               // most skilled players to the front of the list
               vector<account_id_type> seeded_players(tournament_details_obj.registered_players.begin(), 
                                                      tournament_details_obj.registered_players.end());
               for (unsigned i = seeded_players.size() - 1; i >= 1; --i)
               {
                  unsigned j = (unsigned)event.db.get_random_bits(i + 1);
                  std::swap(seeded_players[i], seeded_players[j]);
               }

               // Create all matches in the tournament now.
               // If the number of players isn't a power of two, we will compensate with  byes 
               // in the first round.  
               const uint32_t num_players = fsm.tournament_obj->options.number_of_players;
               uint32_t num_rounds = boost::multiprecision::detail::find_msb(num_players - 1) + 1;
               uint32_t num_matches = (1 << num_rounds) - 1;
               uint32_t num_matches_in_first_round = 1 << (num_rounds - 1);

               // First, assign players to their first round of matches in the paired_players
               // array, where the first two play against each other, the second two play against
               // each other, etc.
               // Anyone with account_id_type() as their opponent gets a bye
               vector<account_id_type> paired_players;
               paired_players.resize(num_matches_in_first_round * 2);
               for (uint32_t player_num = 0; player_num < num_players; ++player_num)
               {
                  uint32_t player_position = reverse_bits(player_num ^ (player_num >> 1)) >> (32 - num_rounds);
                  paired_players[player_position] = seeded_players[player_num];
               }

               // now create the match objects for this first round
               vector<match_id_type> matches;
               matches.reserve(num_matches);

               // create a bunch of empty matches
               for (unsigned i = 0; i < num_matches; ++i)
                  matches.emplace_back(create_match(event.db, fsm.tournament_obj->id, vector<account_id_type>()));

               // then walk through our paired players by twos, starting the first matches
               for (unsigned i = 0; i < num_matches_in_first_round; ++i)
               {
                  vector<account_id_type> players;
                  players.emplace_back(paired_players[2 * i]);
                  if (paired_players[2 * i + 1] != account_id_type())
                     players.emplace_back(paired_players[2 * i + 1]);
                  event.db.modify(matches[i](event.db), [&](match_object& match) {
                     match.players = players;
                     match.on_initiate_match(event.db);
                     });
               }
               event.db.modify(tournament_details_obj, [&](tournament_details_object& tournament_details_obj){
                  tournament_details_obj.matches = matches;
               });

               // find "bye" matches, complete missing player in the next match
               for (unsigned i = 0; i < num_matches_in_first_round; ++i)
               {
                  const match_object& match = matches[i](event.db);
                  if (match.players.size() == 1) // is "bye"
                  {
                      unsigned tournament_num_matches = tournament_details_obj.matches.size();
                      unsigned next_round_match_index = (i + tournament_num_matches + 1) / 2;
                      assert(next_round_match_index < tournament_num_matches);
                      const match_object& next_round_match = tournament_details_obj.matches[next_round_match_index](event.db);
                      event.db.modify(next_round_match, [&](match_object& next_match) {
                         next_match.players.emplace_back(match.players[0]);
                         if (next_match.players.size() > 1) // both previous matches were "bye"
                            next_match.on_initiate_match(event.db);

                     });
                   }
               }

            }
            void on_entry(const match_completed& event, tournament_state_machine_& fsm)
            {
               tournament_object& tournament = *fsm.tournament_obj;
               fc_ilog(fc::logger::get("tournament"),
                       "Match ${match_id} in tournament tournament ${tournament_id} is still in progress",
                       ("match_id", event.match.id)("tournament_id", tournament.id));

               // this wasn't the final match that just finished, so figure out if we can start the next match.
               // The next match can start if both this match and the previous match have completed
               const tournament_details_object& tournament_details_obj = fsm.tournament_obj->tournament_details_id(event.db);
               unsigned num_matches = tournament_details_obj.matches.size();
               auto this_match_iter = std::find(tournament_details_obj.matches.begin(), tournament_details_obj.matches.end(), event.match.id);
               assert(this_match_iter != tournament_details_obj.matches.end());
               unsigned this_match_index = std::distance(tournament_details_obj.matches.begin(), this_match_iter);
               // TODO: we currently create all matches at startup, so they are numbered sequentially.  We could get the index
               // by subtracting match.id as long as this behavior doesn't change

               unsigned next_round_match_index = (this_match_index + num_matches + 1) / 2;
               assert(next_round_match_index < num_matches);
               const match_object& next_round_match = tournament_details_obj.matches[next_round_match_index](event.db);

               // each match will have two players, match.players[0] and match.players[1].
               // for consistency, we want to feed the winner of this match into the correct
               // slot in the next match 
               unsigned winner_index_in_next_match = (this_match_index + num_matches + 1) % 2;
               unsigned other_match_index = num_matches - ((num_matches - next_round_match_index) * 2 + winner_index_in_next_match);
               const match_object& other_match = tournament_details_obj.matches[other_match_index](event.db);

               // the winners of the matches event.match and other_match will play in next_round_match

               assert(event.match.match_winners.size() <= 1);

               event.db.modify(next_round_match, [&](match_object& next_match_obj) {

                  if (!event.match.match_winners.empty()) // if there is a winner
                  {
                     if (winner_index_in_next_match == 0)
                        next_match_obj.players.insert(next_match_obj.players.begin(), *event.match.match_winners.begin());
                     else
                        next_match_obj.players.push_back(*event.match.match_winners.begin());
                  }

                  if (other_match.get_state() == match_state::match_complete)
                  {
                     next_match_obj.on_initiate_match(event.db);
                  }

               });
            }
         };

         struct registration_period_expired : public msm::front::state<>
         {
            void on_entry(const registration_deadline_passed& event, tournament_state_machine_& fsm)
            {
               fc_ilog(fc::logger::get("tournament"),
                       "Tournament ${id} is canceled",
                       ("id", fsm.tournament_obj->id));
               // repay everyone who paid into the prize pool
               const tournament_details_object& details = fsm.tournament_obj->tournament_details_id(event.db);
               for (const auto& payer_pair : details.payers)
               {
                  // TODO: create a virtual operation to record the refund
                  // we'll think of this as just releasing an asset that the user had locked up
                  // for a period of time, not as a transfer back to the user; it doesn't matter
                  // if they are currently authorized to transfer this asset, they never really 
                  // transferred it in the first place
                  asset amount(payer_pair.second, fsm.tournament_obj->options.buy_in.asset_id);
                  event.db.adjust_balance(payer_pair.first, amount);

                  // Generating a virtual operation that shows the payment
                  tournament_payout_operation op;
                  op.tournament_id = fsm.tournament_obj->id;
                  op.payout_amount = amount;
                  op.payout_account_id = payer_pair.first;
                  op.type = payout_type::buyin_refund;
                  event.db.push_applied_operation(op);
               }
            }
         };

         struct concluded : public msm::front::state<>
         {
            void on_entry(const match_completed& event, tournament_state_machine_& fsm)
            {
               tournament_object& tournament_obj = *fsm.tournament_obj;
               fc_ilog(fc::logger::get("tournament"),
                       "Tournament ${id} is complete",
                       ("id", tournament_obj.id));
               tournament_obj.end_time = event.db.head_block_time();

               // Distribute prize money when a tournament ends
#ifndef NDEBUG
               const tournament_details_object& details = tournament_obj.tournament_details_id(event.db);
               share_type total_prize = 0;
               for (const auto& payer_pair : details.payers)
                  total_prize += payer_pair.second;
               assert(total_prize == tournament_obj.prize_pool);
#endif
               assert(event.match.match_winners.size() ==  1);
               const account_id_type& winner = *event.match.match_winners.begin();
               uint16_t rake_fee_percentage = event.db.get_global_properties().parameters.rake_fee_percentage;

               const asset_object & asset_obj = asset_id_type(0)(event.db);
               optional<asset_dividend_data_id_type> dividend_id = asset_obj.dividend_data_id;

               share_type rake_amount = 0;
               if (dividend_id)
                  rake_amount = (fc::uint128_t(tournament_obj.prize_pool.value) * rake_fee_percentage / GRAPHENE_1_PERCENT / 100).to_uint64();
               asset won_prize(tournament_obj.prize_pool - rake_amount, tournament_obj.options.buy_in.asset_id);
               tournament_payout_operation op;

               if (won_prize.amount.value)
               {
                  // Adjusting balance of winner
                  event.db.adjust_balance(winner, won_prize);

                  // Generating a virtual operation that shows the payment
                  op.tournament_id = tournament_obj.id;
                  op.payout_amount = won_prize;
                  op.payout_account_id = winner;
                  op.type = payout_type::prize_award;
                  event.db.push_applied_operation(op);
               }

               if (rake_amount.value)
               {
                  affiliate_payout_helper payout_helper( event.db, tournament_obj );
                  rake_amount -= payout_helper.payout( winner, rake_amount );
                  payout_helper.commit();
                  FC_ASSERT( rake_amount.value >= 0 );
               }

               if (rake_amount.value)
               {
                  // Adjusting balance of dividend_distribution_account
                  const asset_dividend_data_id_type& asset_dividend_data_id_= *dividend_id;
                  const asset_dividend_data_object& dividend_obj = asset_dividend_data_id_(event.db);
                  const account_id_type& rake_account_id = dividend_obj.dividend_distribution_account;
                  asset rake(rake_amount, tournament_obj.options.buy_in.asset_id);
                  event.db.adjust_balance(rake_account_id, rake);

                  // Generating a virtual operation that shows the payment
                  op.payout_amount = rake;
                  op.payout_account_id = rake_account_id;
                  op.type = payout_type::rake_fee;
                  event.db.push_applied_operation(op);
               }
            }
         };

         typedef accepting_registrations initial_state;

         typedef tournament_state_machine_ x; // makes transition table cleaner
         
         // Guards
         bool will_be_fully_registered(const player_registered& event)
         {
            fc_ilog(fc::logger::get("tournament"),
                    "In will_be_fully_registered guard, returning ${value}",
                    ("value", tournament_obj->registered_players == tournament_obj->options.number_of_players - 1));
            return tournament_obj->registered_players == tournament_obj->options.number_of_players - 1;
         }

         bool was_final_match(const match_completed& event)
         {
            const tournament_details_object& tournament_details_obj = tournament_obj->tournament_details_id(event.db);
            auto final_match_id = tournament_details_obj.matches[tournament_details_obj.matches.size() - 1];
            bool was_final = event.match.id == final_match_id;
            fc_ilog(fc::logger::get("tournament"),
                    "In was_final_match guard, returning ${value}",
                    ("value", was_final));
            return was_final;
         }

         void register_player(const player_registered& event)
         {
            fc_ilog(fc::logger::get("tournament"),
                    "In register_player action, player_id is ${player_id}, payer_id is ${payer_id}",
                    ("player_id", event.player_id)("payer_id", event.payer_id));

            event.db.adjust_balance(event.payer_id, -tournament_obj->options.buy_in);
            const tournament_details_object& tournament_details_obj = tournament_obj->tournament_details_id(event.db);
            event.db.modify(tournament_details_obj, [&](tournament_details_object& tournament_details_obj){
                    tournament_details_obj.payers[event.payer_id] += tournament_obj->options.buy_in.amount;
                    tournament_details_obj.registered_players.insert(event.player_id);
                    tournament_details_obj.players_payers[event.player_id] = event.payer_id;
                 });
            ++tournament_obj->registered_players;
            tournament_obj->prize_pool += tournament_obj->options.buy_in.amount;
         }

         void unregister_player(const player_unregistered& event)
         {
            fc_ilog(fc::logger::get("tournament"),
                    "In unregister_player action, player_id is ${player_id}",
                    ("player_id", event.player_id));

            const tournament_details_object& tournament_details_obj = tournament_obj->tournament_details_id(event.db);
            account_id_type payer_id  = tournament_details_obj.players_payers.at(event.player_id);
            event.db.adjust_balance(payer_id, tournament_obj->options.buy_in);
            event.db.modify(tournament_details_obj, [&](tournament_details_object& tournament_details_obj){
                    tournament_details_obj.payers[payer_id] -= tournament_obj->options.buy_in.amount;
                    if (tournament_details_obj.payers[payer_id] <= 0)
                        tournament_details_obj.payers.erase(payer_id);
                    tournament_details_obj.registered_players.erase(event.player_id);
                    tournament_details_obj.players_payers.erase(event.player_id);
                 });
            --tournament_obj->registered_players;
            tournament_obj->prize_pool -= tournament_obj->options.buy_in.amount;
         }

         // Transition table for tournament
         struct transition_table : mpl::vector<
         //    Start                       Event                         Next                       Action               Guard
         //  +---------------------------+-----------------------------+----------------------------+---------------------+----------------------+
         a_row < accepting_registrations, player_registered,            accepting_registrations,     &x::register_player >,
         a_row < accepting_registrations, player_unregistered,          accepting_registrations,     &x::unregister_player >,
         row   < accepting_registrations, player_registered,            awaiting_start,              &x::register_player,  &x::will_be_fully_registered >,
         _row  < accepting_registrations, registration_deadline_passed, registration_period_expired >,
         //  +---------------------------+-----------------------------+----------------------------+---------------------+----------------------+
         a_row < awaiting_start,          player_unregistered,          accepting_registrations,     &x::unregister_player >,
         _row  < awaiting_start,          start_time_arrived,           in_progress >,
         //  +---------------------------+-----------------------------+----------------------------+---------------------+----------------------+
         _row  < in_progress,             match_completed,              in_progress >,
         g_row < in_progress,             match_completed,              concluded,                                         &x::was_final_match >
         //  +---------------------------+-----------------------------+----------------------------+---------------------+----------------------+
         > {};


         tournament_object* tournament_obj;
         tournament_state_machine_(tournament_object* tournament_obj) : tournament_obj(tournament_obj) {}
      };
      typedef msm::back::state_machine<tournament_state_machine_> tournament_state_machine;
   }

   class tournament_object::impl {
   public:
      tournament_state_machine state_machine;

      impl(tournament_object* self) : state_machine(self) {}
   };

   tournament_object::tournament_object() :
      my(new impl(this))
   {
   }

   tournament_object::tournament_object(const tournament_object& rhs) : 
      graphene::db::abstract_object<tournament_object>(rhs),
      creator(rhs.creator),
      options(rhs.options),
      start_time(rhs.start_time),
      end_time(rhs.end_time),
      prize_pool(rhs.prize_pool),
      registered_players(rhs.registered_players),
      tournament_details_id(rhs.tournament_details_id),
      my(new impl(this))
   {
      my->state_machine = rhs.my->state_machine;
      my->state_machine.tournament_obj = this;
   }

   tournament_object& tournament_object::operator=(const tournament_object& rhs)
   {
      //graphene::db::abstract_object<tournament_object>::operator=(rhs);
      id = rhs.id;
      creator = rhs.creator;
      options = rhs.options;
      start_time = rhs.start_time;
      end_time = rhs.end_time;
      prize_pool = rhs.prize_pool;
      registered_players = rhs.registered_players;
      tournament_details_id = rhs.tournament_details_id;
      my->state_machine = rhs.my->state_machine;
      my->state_machine.tournament_obj = this;

      return *this;
   }

   tournament_object::~tournament_object()
   {
   }

   bool verify_tournament_state_constants()
   {
      unsigned error_count = 0;
      typedef msm::back::generate_state_set<tournament_state_machine::stt>::type all_states;
      static char const* filled_state_names[mpl::size<all_states>::value];
      mpl::for_each<all_states,boost::msm::wrap<mpl::placeholders::_1> >
         (msm::back::fill_state_names<tournament_state_machine::stt>(filled_state_names));
      for (unsigned i = 0; i < mpl::size<all_states>::value; ++i)
      {
         try
         {
            // this is an approximate test, the state name provided by typeinfo will be mangled, but should
            // at least contain the string we're looking for
            const char* fc_reflected_value_name = fc::reflector<tournament_state>::to_string((tournament_state)i);
            if (!strcmp(fc_reflected_value_name, filled_state_names[i]))
               fc_elog(fc::logger::get("tournament"),
                       "Error, state string mismatch between fc and boost::msm for int value ${int_value}: boost::msm -> ${boost_string}, fc::reflect -> ${fc_string}",
                       ("int_value", i)("boost_string", filled_state_names[i])("fc_string", fc_reflected_value_name));
         }
         catch (const fc::bad_cast_exception&)
         {
            fc_elog(fc::logger::get("tournament"),
                    "Error, no reflection for value ${int_value} in enum tournament_state",
                    ("int_value", i));
            ++error_count;
         }
      }

      return error_count == 0;
   }

   tournament_state tournament_object::get_state() const
   {
      static bool state_constants_are_correct = verify_tournament_state_constants();
      (void)&state_constants_are_correct;
      tournament_state state = (tournament_state)my->state_machine.current_state()[0];

      return state;  
   }

   void tournament_object::pack_impl(std::ostream& stream) const
   {
      boost::archive::binary_oarchive oa(stream, boost::archive::no_header|boost::archive::no_codecvt|boost::archive::no_xml_tag_checking);
      oa << my->state_machine;
   }

   void tournament_object::unpack_impl(std::istream& stream)
   {
      boost::archive::binary_iarchive ia(stream, boost::archive::no_header|boost::archive::no_codecvt|boost::archive::no_xml_tag_checking);
      ia >> my->state_machine;
   }

   void tournament_object::on_registration_deadline_passed(database& db)
   {
      my->state_machine.process_event(registration_deadline_passed(db));
   }

   void tournament_object::on_player_registered(database& db, account_id_type payer_id, account_id_type player_id)
   {
      my->state_machine.process_event(player_registered(db, payer_id, player_id));
   }

   void tournament_object::on_player_unregistered(database& db, account_id_type player_id)
   {
      my->state_machine.process_event(player_unregistered(db, player_id));
   }

   void tournament_object::on_start_time_arrived(database& db)
   {
      my->state_machine.process_event(start_time_arrived(db));
   }

   void tournament_object::on_match_completed(database& db, const match_object& match)
   {
      my->state_machine.process_event(match_completed(db, match));
   }

   void tournament_object::check_for_new_matches_to_start(database& db) const
   {
      const tournament_details_object& tournament_details_obj = tournament_details_id(db);

      unsigned num_matches = tournament_details_obj.matches.size();
      uint32_t num_rounds = boost::multiprecision::detail::find_msb(num_matches + 1);

      // Scan the matches by round to find the last round where all matches are complete
      int last_complete_round = -1;
      bool first_incomplete_match_was_waiting = false;
      for (unsigned round_num = 0; round_num < num_rounds; ++round_num)
      {
         uint32_t num_matches_in_this_round = 1 << (num_rounds - round_num - 1);
         uint32_t first_match_in_round = (num_matches - (num_matches >> round_num));
         bool all_matches_in_round_complete = true;
         for (uint32_t match_num = first_match_in_round; match_num < first_match_in_round + num_matches_in_this_round; ++match_num)
         {
            const match_object& match = tournament_details_obj.matches[match_num](db);
            if (match.get_state() != match_state::match_complete)
            {
               first_incomplete_match_was_waiting = match.get_state() == match_state::waiting_on_previous_matches;
               all_matches_in_round_complete = false;
               break;
            }
         }
         if (all_matches_in_round_complete)
            last_complete_round = round_num;
         else
            break;
      }

      if (last_complete_round == -1)
         return;

      // We shouldn't be here if the final match is complete
      assert(last_complete_round != num_rounds - 1);
      if (last_complete_round == num_rounds - 1)
         return;

      if (first_incomplete_match_was_waiting)
      {
         // all previous matches have completed, and the first match in this round hasn't been
         // started (which means none of the matches in this round should have started)
         unsigned first_incomplete_round = last_complete_round + 1;
         uint32_t num_matches_in_incomplete_round = 1 << (num_rounds - first_incomplete_round - 1);
         uint32_t first_match_in_incomplete_round = num_matches - (num_matches >> first_incomplete_round);
         for (uint32_t match_num = first_match_in_incomplete_round; 
              match_num < first_match_in_incomplete_round + num_matches_in_incomplete_round;
              ++match_num)
         {
            int left_child_index = (num_matches - 1) - ((num_matches - 1 - match_num) * 2 + 2);
            int right_child_index = left_child_index + 1;
            const match_object& match_to_start = tournament_details_obj.matches[left_child_index](db);
            const match_object& left_match = tournament_details_obj.matches[left_child_index](db);
            const match_object& right_match = tournament_details_obj.matches[right_child_index](db);
            std::vector<account_id_type> winners;
            if (!left_match.match_winners.empty())
            {
               assert(left_match.match_winners.size() == 1);
               winners.emplace_back(*left_match.match_winners.begin());
            }
            if (!right_match.match_winners.empty())
            {
               assert(right_match.match_winners.size() == 1);
               winners.emplace_back(*right_match.match_winners.begin());
            }
            db.modify(match_to_start, [&](match_object& match) {
               match.players = winners;
               //match.state = ready_to_begin;
            });

         }
      }
   }

   fc::sha256 rock_paper_scissors_throw::calculate_hash() const
   {
      std::vector<char> full_throw_packed(fc::raw::pack(*this));
      return fc::sha256::hash(full_throw_packed.data(), full_throw_packed.size());
   }


   vector<tournament_id_type> tournament_players_index::get_registered_tournaments_for_account( const account_id_type& a )const
   {
      auto iter = account_to_joined_tournaments.find(a);
      if (iter != account_to_joined_tournaments.end())
         return vector<tournament_id_type>(iter->second.begin(), iter->second.end());
      return vector<tournament_id_type>();
   }

   void tournament_players_index::object_inserted(const object& obj)
   {
       assert( dynamic_cast<const tournament_details_object*>(&obj) ); // for debug only
       const tournament_details_object& details = static_cast<const tournament_details_object&>(obj);

       for (const account_id_type& account_id : details.registered_players)
          account_to_joined_tournaments[account_id].insert(details.tournament_id);
   }

   void tournament_players_index::object_removed(const object& obj)
   {
       assert( dynamic_cast<const tournament_details_object*>(&obj) ); // for debug only
       const tournament_details_object& details = static_cast<const tournament_details_object&>(obj);

       for (const account_id_type& account_id : details.registered_players)
       {
          auto iter = account_to_joined_tournaments.find(account_id);
          if (iter != account_to_joined_tournaments.end())
             iter->second.erase(details.tournament_id);
       }
   }

   void tournament_players_index::about_to_modify(const object& before)
   {
      assert( dynamic_cast<const tournament_details_object*>(&before) ); // for debug only
      const tournament_details_object& details = static_cast<const tournament_details_object&>(before);
      before_account_ids = details.registered_players;
   }

   void tournament_players_index::object_modified(const object& after)
   {
      assert( dynamic_cast<const tournament_details_object*>(&after) ); // for debug only
      const tournament_details_object& details = static_cast<const tournament_details_object&>(after);

      {
         vector<account_id_type> newly_registered_players(details.registered_players.size());
         auto end_iter = std::set_difference(details.registered_players.begin(), details.registered_players.end(),
                                             before_account_ids.begin(), before_account_ids.end(), 
                                             newly_registered_players.begin());
         newly_registered_players.resize(end_iter - newly_registered_players.begin());
         for (const account_id_type& account_id : newly_registered_players)
            account_to_joined_tournaments[account_id].insert(details.tournament_id);
      }

      {
         vector<account_id_type> newly_unregistered_players(before_account_ids.size());
         auto end_iter = std::set_difference(before_account_ids.begin(), before_account_ids.end(), 
                                             details.registered_players.begin(), details.registered_players.end(),
                                             newly_unregistered_players.begin());
         newly_unregistered_players.resize(end_iter - newly_unregistered_players.begin());
         for (const account_id_type& account_id : newly_unregistered_players)
         {
            auto iter = account_to_joined_tournaments.find(account_id);
            if (iter != account_to_joined_tournaments.end())
               iter->second.erase(details.tournament_id);
         }
      }
   }
} } // graphene::chain

namespace fc { 
   // Manually reflect tournament_object to variant to properly reflect "state"
   void to_variant(const graphene::chain::tournament_object& tournament_obj, fc::variant& v, uint32_t max_depth)
   {
      fc_elog(fc::logger::get("tournament"), "In tournament_obj to_variant");
      elog("In tournament_obj to_variant");
      fc::mutable_variant_object o;
      o("id", fc::variant(tournament_obj.id, max_depth))
       ("creator", fc::variant(tournament_obj.creator, max_depth))
       ("options", fc::variant(tournament_obj.options, max_depth))
       ("start_time", fc::variant(tournament_obj.start_time, max_depth))
       ("end_time", fc::variant(tournament_obj.end_time, max_depth))
       ("prize_pool", fc::variant(tournament_obj.prize_pool, max_depth))
       ("registered_players", fc::variant(tournament_obj.registered_players, max_depth))
       ("tournament_details_id", fc::variant(tournament_obj.tournament_details_id, max_depth))
       ("state", fc::variant(tournament_obj.get_state(), max_depth));

      v = o;
   }

   // Manually reflect tournament_object to variant to properly reflect "state"
   void from_variant(const fc::variant& v, graphene::chain::tournament_object& tournament_obj, uint32_t max_depth)
   {
      fc_elog(fc::logger::get("tournament"), "In tournament_obj from_variant");
      tournament_obj.id = v["id"].as<graphene::chain::tournament_id_type>( max_depth );
      tournament_obj.creator = v["creator"].as<graphene::chain::account_id_type>( max_depth );
      tournament_obj.options = v["options"].as<graphene::chain::tournament_options>( max_depth );
      tournament_obj.start_time = v["start_time"].as<optional<time_point_sec> >( max_depth );
      tournament_obj.end_time = v["end_time"].as<optional<time_point_sec> >( max_depth );
      tournament_obj.prize_pool = v["prize_pool"].as<graphene::chain::share_type>( max_depth );
      tournament_obj.registered_players = v["registered_players"].as<uint32_t>( max_depth );
      tournament_obj.tournament_details_id = v["tournament_details_id"].as<graphene::chain::tournament_details_id_type>( max_depth );
      graphene::chain::tournament_state state = v["state"].as<graphene::chain::tournament_state>( max_depth );
      const_cast<int*>(tournament_obj.my->state_machine.current_state())[0] = (int)state;
   }
} //end namespace fc


