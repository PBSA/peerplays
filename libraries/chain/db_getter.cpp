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

#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/global_property_object.hpp>

#include <fc/smart_ref_impl.hpp>

#include <ctime>
#include <algorithm>

namespace graphene { namespace chain {

const asset_object& database::get_core_asset() const
{
   return get(asset_id_type());
}

const global_property_object& database::get_global_properties()const
{
   return get( global_property_id_type() );
}

const chain_property_object& database::get_chain_properties()const
{
   return get( chain_property_id_type() );
}

const dynamic_global_property_object& database::get_dynamic_global_properties() const
{
   return get( dynamic_global_property_id_type() );
}

const fee_schedule&  database::current_fee_schedule()const
{
   return get_global_properties().parameters.current_fees;
}

time_point_sec database::head_block_time()const
{
   return get( dynamic_global_property_id_type() ).time;
}

uint32_t database::head_block_num()const
{
   return get( dynamic_global_property_id_type() ).head_block_number;
}

block_id_type database::head_block_id()const
{
   return get( dynamic_global_property_id_type() ).head_block_id;
}

decltype( chain_parameters::block_interval ) database::block_interval( )const
{
   return get_global_properties().parameters.block_interval;
}

const chain_id_type& database::get_chain_id( )const
{
   return get_chain_properties().chain_id;
}

const node_property_object& database::get_node_properties()const
{
   return _node_property_object;
}

node_property_object& database::node_properties()
{
   return _node_property_object;
}

uint32_t database::last_non_undoable_block_num() const
{
   return head_block_num() - _undo_db.size();
}

std::vector<uint32_t> database::get_seeds(asset_id_type for_asset, uint8_t count_winners) const
{
   FC_ASSERT( count_winners <= 64 );
   std::string salted_string = std::string(_random_number_generator._seed) + std::to_string(for_asset.instance.value);
   uint32_t* seeds = (uint32_t*)(fc::sha256::hash(salted_string)._hash);

   std::vector<uint32_t> result;
   result.reserve(64);

   for( int s = 0; s < 8; ++s ) {
      uint32_t* sub_seeds = ( uint32_t* ) fc::sha256::hash( std::to_string( seeds[s] ) + std::to_string( for_asset.instance.value ) )._hash;
      for( int ss = 0; ss < 8; ++ss ) {
         result.push_back(sub_seeds[ss]);
      }
   }
   return result;
}

const std::vector<uint32_t> database::get_winner_numbers( asset_id_type for_asset, uint32_t count_members, uint8_t count_winners ) const
{
   std::vector<uint32_t> result;
   if( count_members < count_winners ) count_winners = count_members;
   if( count_winners == 0 ) return result;
   result.reserve(count_winners);

   auto seeds = get_seeds(for_asset, count_winners);

   for (auto current_seed = seeds.begin(); current_seed != seeds.end(); ++current_seed) {
      uint8_t winner_num = *current_seed % count_members;
      while( std::find(result.begin(), result.end(), winner_num) != result.end() ) {
         *current_seed = (*current_seed * 1103515245 + 12345) / 65536; //using gcc's consts for pseudorandom
         winner_num = *current_seed % count_members;
      }
      result.push_back(winner_num);
      if (result.size() >= count_winners) break;
   }
   
   FC_ASSERT(result.size() == count_winners);
   return result;
}

} }
