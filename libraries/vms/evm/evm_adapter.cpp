#include <evm_adapter.hpp>
#include <evm_result.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/result_contract_object.hpp>
#include <graphene/chain/contracts_results_in_block_object.hpp>
#include <boost/range/adaptor/reversed.hpp>

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

vector<graphene::chain::block_id_type> evm_adapter::get_last_block_hashes() const
{
   return db.get_last_block_hashes();
}

signed_block evm_adapter::get_current_block() const
{
   return db.get_current_block();
}

fc::optional<fc::sha256> evm_adapter::get_last_valid_state_root( const uint32_t& block_number )
{
   const auto& contracts_results_in_block_idx = db.get_index_type<contracts_results_in_block_index>().indices().get<by_block_num>();
   const auto& result_contract_idx = db.get_index_type<result_contract_index>().indices().get<by_id>();

   auto it0 = contracts_results_in_block_idx.lower_bound( 0 );
   auto it1 = contracts_results_in_block_idx.upper_bound( block_number );

   for( const auto& obj : boost::adaptors::reverse( boost::make_iterator_range( it0, it1 ) ) ) {
      for( auto itr = obj.results_id.rbegin(); itr != obj.results_id.rend(); itr++ ) {
         const auto& result_itr = result_contract_idx.find( *itr );
         if( result_itr != result_contract_idx.end() && result_itr->vm_type == vm_types::EVM ) {
            const auto& res = db.db_res.get_results( std::string(result_itr->id) );
            auto result = fc::raw::unpack< evm_result > ( *res );
            return fc::sha256( result.state_root );
         }
      }
   }

   return fc::optional<fc::sha256>();
}

std::set<uint64_t> evm_adapter::get_allowed_assets( const uint64_t& id )
{
   auto& index = db.get_index_type<contract_index>().indices().get<by_id>();
   auto itr = index.find( contract_id_type( id ) );
   return itr != index.end() ? itr->allowed_assets : std::set<uint64_t>();
}

} }
