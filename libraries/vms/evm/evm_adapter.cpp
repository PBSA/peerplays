#include <evm_adapter.hpp>
#include <graphene/chain/database.hpp>

namespace vms { namespace evm {
   
void evm_adapter::publish_contract_transfer( const contract_or_account_id& from_id, const contract_or_account_id& to_id,
                                              const uint64_t& asset_id, const int64_t& value )
{
   object_id_type to;
   if( to_id.first ) {
      to = contract_id_type( to_id.second );
   } else {
      to = account_id_type( to_id.second );
   }
   db.publish_contract_transfer( contract_id_type( from_id.second ), to, asset( value, asset_id_type( asset_id ) ) );
}

std::string evm_adapter::get_next_result_id() const
{
   // return std::string( db.get_index<result_contract_object>().get_next_id() );
}

std::vector<std::pair<int64_t, uint64_t>> evm_adapter::get_contract_balances( const uint64_t& from ) const
{
   std::vector<std::pair<int64_t, uint64_t>> result;
   const contract_balance_index& balance_index = db.get_index_type< contract_balance_index >();
   auto range = balance_index.indices().get< by_contract_asset >().equal_range( boost::make_tuple( contract_id_type( from ) ) );
   for (const contract_balance_object& balance : boost::make_iterator_range( range.first, range.second ) ) {
      if( balance.balance > 0 ) {
         result.push_back( std::make_pair( balance.balance.value, balance.asset_type.instance.value ) );
      }
   }
   return result;
}

void evm_adapter::delete_contract_balances( const uint64_t& id )
{
   const contract_balance_index& balance_index = db.get_index_type< contract_balance_index >();
   auto range = balance_index.indices().get< by_contract_asset >().equal_range(boost::make_tuple( contract_id_type( id ) ));
   auto iter = boost::make_iterator_range( range.first, range.second ).begin();
   auto end = boost::make_iterator_range( range.first, range.second ).end();
   while( iter != end ){
      auto temp = iter;
      iter++;
      db.remove( *temp );
   }
}

void evm_adapter::contract_suicide( const uint64_t& id )
{
   auto& index = db.get_index_type<contract_index>().indices().get<by_id>();
   auto itr = index.find( contract_id_type( id ) );
   db.modify(*itr, [&] (contract_object& co) {
      co.suicided = true;
   });
}

} }
