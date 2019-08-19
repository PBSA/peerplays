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
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/database.hpp>

#include <fc/uint128.hpp>

#include <cmath>

using namespace graphene::chain;

share_type asset_bitasset_data_object::max_force_settlement_volume(share_type current_supply) const
{
   if( options.maximum_force_settlement_volume == 0 )
      return 0;
   if( options.maximum_force_settlement_volume == GRAPHENE_100_PERCENT )
      return current_supply + force_settled_volume;

   fc::uint128 volume = current_supply.value + force_settled_volume.value;
   volume *= options.maximum_force_settlement_volume;
   volume /= GRAPHENE_100_PERCENT;
   return volume.to_uint64();
}

void asset_bitasset_data_object::update_median_feeds(time_point_sec current_time)
{
   current_feed_publication_time = current_time;
   vector<std::reference_wrapper<const price_feed>> current_feeds;
   for( const pair<account_id_type, pair<time_point_sec,price_feed>>& f : feeds )
   {
      if( (current_time - f.second.first).to_seconds() < options.feed_lifetime_sec &&
          f.second.first != time_point_sec() )
      {
         current_feeds.emplace_back(f.second.second);
         current_feed_publication_time = std::min(current_feed_publication_time, f.second.first);
      }
   }

   // If there are no valid feeds, or the number available is less than the minimum to calculate a median...
   if( current_feeds.size() < options.minimum_feeds )
   {
      //... don't calculate a median, and set a null feed
      current_feed_publication_time = current_time;
      current_feed = price_feed();
      return;
   }
   if( current_feeds.size() == 1 )
   {
      current_feed = std::move(current_feeds.front());
      return;
   }

   // *** Begin Median Calculations ***
   price_feed median_feed;
   const auto median_itr = current_feeds.begin() + current_feeds.size() / 2;
#define CALCULATE_MEDIAN_VALUE(r, data, field_name) \
   std::nth_element( current_feeds.begin(), median_itr, current_feeds.end(), \
                     [](const price_feed& a, const price_feed& b) { \
      return a.field_name < b.field_name; \
   }); \
   median_feed.field_name = median_itr->get().field_name;

   BOOST_PP_SEQ_FOR_EACH( CALCULATE_MEDIAN_VALUE, ~, GRAPHENE_PRICE_FEED_FIELDS )
#undef CALCULATE_MEDIAN_VALUE
   // *** End Median Calculations ***

   current_feed = median_feed;
}


time_point_sec asset_object::get_lottery_expiration() const 
{
   if( lottery_options )
      return lottery_options->end_date;
   return time_point_sec();
}

asset asset_object::amount_from_string(string amount_string) const
{ try {
   bool negative_found = false;
   bool decimal_found = false;
   for( const char c : amount_string )
   {
      if( isdigit( c ) )
         continue;

      if( c == '-' && !negative_found )
      {
         negative_found = true;
         continue;
      }

      if( c == '.' && !decimal_found )
      {
         decimal_found = true;
         continue;
      }

      FC_THROW( (amount_string) );
   }

   share_type satoshis = 0;

   share_type scaled_precision = asset::scaled_precision( precision );

   const auto decimal_pos = amount_string.find( '.' );
   const string lhs = amount_string.substr( negative_found, decimal_pos );
   if( !lhs.empty() )
      satoshis += fc::safe<int64_t>(std::stoll(lhs)) *= scaled_precision;

   if( decimal_found )
   {
      const size_t max_rhs_size = std::to_string( scaled_precision.value ).substr( 1 ).size();

      string rhs = amount_string.substr( decimal_pos + 1 );
      FC_ASSERT( rhs.size() <= max_rhs_size );

      while( rhs.size() < max_rhs_size )
         rhs += '0';

      if( !rhs.empty() )
         satoshis += std::stoll( rhs );
   }

   FC_ASSERT( satoshis <= GRAPHENE_MAX_SHARE_SUPPLY );

   if( negative_found )
      satoshis *= -1;

   return amount(satoshis);
   } FC_CAPTURE_AND_RETHROW( (amount_string) ) }

string asset_object::amount_to_string(share_type amount) const
{
   share_type scaled_precision = 1;
   for( uint8_t i = 0; i < precision; ++i )
      scaled_precision *= 10;
   assert(scaled_precision > 0);

   string result = fc::to_string(amount.value / scaled_precision.value);
   auto decimals = amount.value % scaled_precision.value;
   if( decimals )
      result += "." + fc::to_string(scaled_precision.value + decimals).erase(0,1);
   return result;
}


vector<account_id_type> asset_object::get_holders( database& db ) const
{
   auto& asset_bal_idx = db.get_index_type< account_balance_index >().indices().get< by_asset_balance >();
   
   uint64_t max_supply = get_id()(db).options.max_supply.value;
   
   vector<account_id_type> holders; // repeating if balance > 1
   holders.reserve(max_supply);
   const auto range = asset_bal_idx.equal_range( boost::make_tuple( get_id() ) );
   for( const account_balance_object& bal : boost::make_iterator_range( range.first, range.second ) )
      for( uint64_t balance = bal.balance.value; balance > 0; --balance)
         holders.push_back( bal.owner );
   return holders;
}

void asset_object::distribute_benefactors_part( database& db )
{
   transaction_evaluation_state eval( &db );
   uint64_t jackpot = get_id()( db ).dynamic_data( db ).current_supply.value * lottery_options->ticket_price.amount.value;
   
   for( auto benefactor : lottery_options->benefactors ) {
      lottery_reward_operation reward_op;
      reward_op.lottery = get_id();
      reward_op.winner = benefactor.id;
      reward_op.is_benefactor_reward = true;
      reward_op.win_percentage = benefactor.share;
      reward_op.amount = asset( jackpot * benefactor.share / GRAPHENE_100_PERCENT, db.get_balance(id).asset_id );
      db.apply_operation(eval, reward_op);
   }
}

map< account_id_type, vector< uint16_t > > asset_object::distribute_winners_part( database& db )
{
   transaction_evaluation_state eval( &db );
      
   auto holders = get_holders( db );
   FC_ASSERT( dynamic_data( db ).current_supply == holders.size() );
   map<account_id_type, vector<uint16_t> > structurized_participants;
   for( account_id_type holder : holders )
   {
      if( !structurized_participants.count( holder ) )
         structurized_participants.emplace( holder, vector< uint16_t >() );
   }
   uint64_t jackpot = get_id()( db ).dynamic_data( db ).current_supply.value * lottery_options->ticket_price.amount.value;
   auto winner_numbers = db.get_winner_numbers( get_id(), holders.size(), lottery_options->winning_tickets.size() );
   
   auto& tickets( lottery_options->winning_tickets );
   
   if( holders.size() < tickets.size() ) {
      uint16_t percents_to_distribute = 0;
      for( auto i = tickets.begin() + holders.size(); i != tickets.end(); ) {
         percents_to_distribute += *i;
         i = tickets.erase(i);
      }
      for( auto t = tickets.begin(); t != tickets.begin() + holders.size(); ++t )
         *t += percents_to_distribute / holders.size();
   }
   auto sweeps_distribution_percentage = db.get_global_properties().parameters.sweeps_distribution_percentage();
   for( int c = 0; c < winner_numbers.size(); ++c ) {
      auto winner_num = winner_numbers[c];
      lottery_reward_operation reward_op;
      reward_op.lottery = get_id();
      reward_op.is_benefactor_reward = false;
      reward_op.winner = holders[winner_num];
      reward_op.win_percentage = tickets[c];
      reward_op.amount = asset( jackpot * tickets[c] * ( 1. - sweeps_distribution_percentage / (double)GRAPHENE_100_PERCENT ) / GRAPHENE_100_PERCENT , db.get_balance(id).asset_id );
      db.apply_operation(eval, reward_op);
      
      structurized_participants[ holders[ winner_num ] ].push_back( tickets[c] );
   }
   return structurized_participants;
}

void asset_object::distribute_sweeps_holders_part( database& db )
{
   transaction_evaluation_state eval( &db );
   
   auto& asset_bal_idx = db.get_index_type< account_balance_index >().indices().get< by_asset_balance >();
   
   auto sweeps_params = db.get_global_properties().parameters;
   uint64_t distribution_asset_supply = sweeps_params.sweeps_distribution_asset()( db ).dynamic_data( db ).current_supply.value;
   const auto range = asset_bal_idx.equal_range( boost::make_tuple( sweeps_params.sweeps_distribution_asset() ) );
   
   uint64_t holders_sum = 0;
   for( const account_balance_object& holder_balance : boost::make_iterator_range( range.first, range.second ) )
   {
      int64_t holder_part = db.get_balance(id).amount.value / (double)distribution_asset_supply * holder_balance.balance.value * SWEEPS_VESTING_BALANCE_MULTIPLIER;
      db.adjust_sweeps_vesting_balance( holder_balance.owner, holder_part );
      holders_sum += holder_part;
   }
   uint64_t balance_rest = db.get_balance( get_id() ).amount.value * SWEEPS_VESTING_BALANCE_MULTIPLIER - holders_sum;
   db.adjust_sweeps_vesting_balance( sweeps_params.sweeps_vesting_accumulator_account(), balance_rest );
   db.adjust_balance( get_id(), -db.get_balance( get_id() ) );
}

void asset_object::end_lottery( database& db )
{
   transaction_evaluation_state eval(&db);
   
   FC_ASSERT( is_lottery() );
   FC_ASSERT( lottery_options->is_active && ( lottery_options->end_date <= db.head_block_time() || lottery_options->ending_on_soldout ) );

   auto participants = distribute_winners_part( db );
   if( participants.size() > 0) {
      distribute_benefactors_part( db );
      distribute_sweeps_holders_part( db );
   }
   
   lottery_end_operation end_op;
   end_op.lottery = id;
   end_op.participants = participants;
   db.apply_operation(eval, end_op);
}

void lottery_balance_object::adjust_balance( const asset& delta )
{
   FC_ASSERT( delta.asset_id == balance.asset_id );
   balance += delta;
}

void sweeps_vesting_balance_object::adjust_balance( const asset& delta )
{
   FC_ASSERT( delta.asset_id == asset_id );
   balance += delta.amount.value;
}
