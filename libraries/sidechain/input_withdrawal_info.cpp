#include <sidechain/input_withdrawal_info.hpp>
#include <graphene/chain/database.hpp>

namespace sidechain {

uint64_t info_for_vin::count_id_info_for_vin = 0;

bool info_for_vin::comparer::operator() ( const info_for_vin& lhs, const info_for_vin& rhs ) const
{
   if( lhs.created != rhs.created ) {
      return lhs.created < rhs.created;
   }
   return lhs.id < rhs.id;
}

void input_withdrawal_info::insert_info_for_vin( const prev_out& out, const std::string& address, std::vector<char> script )
{
   info_for_vins.insert( info_for_vin( out, address, script ) );
}

void input_withdrawal_info::modify_info_for_vin( const info_for_vin& obj, const std::function<void( info_for_vin& e )>& func )
{
   info_for_vins.modify( obj, func );
}

void input_withdrawal_info::mark_as_used_vin( const info_for_vin& obj )
{
   info_for_vins.modify( obj, [&]( info_for_vin& o ) {
      o.created = true;
   } );
}

void input_withdrawal_info::remove_info_for_vin( const info_for_vin& obj )
{
   info_for_vins.remove( obj );
}

std::pair<bool, input_withdrawal_info::iterator_id_vin> input_withdrawal_info::find_info_for_vin( uint64_t id )
{ 
   return info_for_vins.find( id );
}

std::vector<info_for_vin> input_withdrawal_info::get_info_for_vins()
{
   std::vector<info_for_vin> result;

   info_for_vins.safe_for<by_id_and_not_created>( [&]( info_for_vin_index::index<by_id_and_not_created>::type::iterator itr_b,
                                                       info_for_vin_index::index<by_id_and_not_created>::type::iterator itr_e )
   {
      for( size_t i = 0; itr_b != itr_e && i < 5 && !itr_b->created; i++ ) {  // 5 amount vins to bitcoin transaction
         info_for_vin vin;
         vin.identifier = itr_b->identifier;
         vin.out.hash_tx = itr_b->out.hash_tx;
         vin.out.n_vout = itr_b->out.n_vout;
         vin.out.amount = itr_b->out.amount;
         vin.address = itr_b->address;
         // vin.script = get account address, from the address get the script
         
         result.push_back( vin );
         ++itr_b;
      }
   } );

   return result;
}

void input_withdrawal_info::insert_info_for_vout( const graphene::chain::account_id_type& payer, /*ayment_type addr_type,*/ const std::string& data, const uint64_t& amount )
{
   db.create<graphene::chain::info_for_vout_object>([&](graphene::chain::info_for_vout_object& obj) {
      obj.payer = payer;
      // obj.addr_type = addr_type;
      obj.data = data;
      obj.amount = amount;
   });
}

void input_withdrawal_info::mark_as_used_vout( const graphene::chain::info_for_vout_object& obj )
{
   db.modify<graphene::chain::info_for_vout_object>( obj, [&]( graphene::chain::info_for_vout_object& o ) {
      o.created = true;
   });
}

void input_withdrawal_info::remove_info_for_vout( const info_for_vout& obj )
{
   db.remove( obj );
}

std::pair<bool, input_withdrawal_info::iterator_id_vout> input_withdrawal_info::find_info_for_vout( uint64_t id )
{
   const auto& info_for_vout_idx = db.get_index_type<graphene::chain::info_for_vout_index>().indices().get< graphene::chain::by_id >();
   auto itr = info_for_vout_idx.find( graphene::chain::info_for_vout_id_type( id ) );
   return std::make_pair( itr != info_for_vout_idx.end(), itr );
}

size_t input_withdrawal_info::size_info_for_vouts()
{
   const auto& info_for_vout_idx = db.get_index_type<graphene::chain::info_for_vout_index>().indices().get< graphene::chain::by_id >();
   return info_for_vout_idx.size();
}

std::vector<info_for_vout> input_withdrawal_info::get_info_for_vouts()
{
   std::vector<info_for_vout> result;

   const auto& info_for_vout_idx = db.get_index_type<graphene::chain::info_for_vout_index>().indices().get< graphene::chain::by_id_and_not_created >();
   auto itr = info_for_vout_idx.begin();
   for(size_t i = 0; i < 5 && itr != info_for_vout_idx.end() && !itr->created; i++) {
      info_for_vout vout;
      vout.payer = itr->payer;
      // vout.addr_type = itr->addr_type;
      vout.data = itr->data;
      vout.amount = itr->amount;

      result.push_back( vout );
      ++itr;
   }

   return result;
}

}
