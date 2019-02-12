#include <sidechain/input_withdrawal_info.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/primary_wallet_vout_object.hpp>
#include <graphene/chain/bitcoin_address_object.hpp>
#include <sidechain/bitcoin_transaction_confirmations.hpp>

namespace sidechain {

uint64_t info_for_vin::count_id_info_for_vin = 0;
uint64_t bitcoin_transaction_confirmations::count_id_tx_conf = 0;

info_for_vin::info_for_vin( const prev_out& _out, const std::string& _address, bytes _script, bool _resend ) :
   id( count_id_info_for_vin++ ), out( _out ), address( _address ), script( _script ), resend( _resend )
{
   identifier = fc::sha256::hash( out.hash_tx + std::to_string( out.n_vout ) );
}

bool info_for_vin::operator!=( const info_for_vin& obj ) const
{
   if( this->identifier != obj.identifier ||
       this->out.hash_tx != obj.out.hash_tx ||
       this->out.n_vout != obj.out.n_vout ||
       this->out.amount != obj.out.amount ||
       this->address != obj.address ||
       this->script != obj.script )
   {
      return true;
   }
   return false;
}

bool info_for_vin::comparer::operator() ( const info_for_vin& lhs, const info_for_vin& rhs ) const
{
   if( lhs.used != rhs.used ) {
      return lhs.used < rhs.used;
   } else if ( lhs.resend != rhs.resend ) {
      return lhs.resend > rhs.resend;
   }
   return lhs.id < rhs.id;
}

std::vector<bytes> input_withdrawal_info::get_redeem_scripts( const std::vector<info_for_vin>& info_vins )
{
   std::vector<bytes> redeem_scripts;
   const auto& bitcoin_address_idx = db.get_index_type<bitcoin_address_index>().indices().get< by_address >();
   for( const auto& v : info_vins ) {
      const auto& pbtc_address = bitcoin_address_idx.find( v.address );
      redeem_scripts.push_back( pbtc_address->address.get_redeem_script() );
   }
   return redeem_scripts;
}

std::vector<uint64_t> input_withdrawal_info::get_amounts( const std::vector<info_for_vin>& info_vins )
{
   std::vector<uint64_t> amounts;
   for( const auto& v : info_vins ) {
      amounts.push_back( v.out.amount );
   }
   return amounts;
}

fc::optional<info_for_vin> input_withdrawal_info::get_info_for_pw_vin()
{
   fc::optional< primary_wallet_vout_object > vout = db.pw_vout_manager.get_latest_unused_vout();
   if( !vout.valid() || db.pw_vout_manager.is_max_vouts() ) {
      return fc::optional<info_for_vin>();
   }

   const auto& pw_address = db.get_latest_PW().address;

   info_for_vin vin;
   vin.identifier = vout->hash_id;
   vin.out = vout->vout;
   vin.address = pw_address.get_address();
   vin.script = pw_address.get_witness_script();

   return vin;
}

void input_withdrawal_info::insert_info_for_vin( const prev_out& out, const std::string& address, bytes script, bool resend )
{
   info_for_vins.insert( info_for_vin( out, address, script, resend ) );
}

void input_withdrawal_info::modify_info_for_vin( const info_for_vin& obj, const std::function<void( info_for_vin& e )>& func )
{
   info_for_vins.modify<by_identifier>( obj.identifier, func );
}

void input_withdrawal_info::mark_as_used_vin( const info_for_vin& obj )
{
   info_for_vins.modify<by_identifier>( obj.identifier, [&]( info_for_vin& o ) {
      o.used = true;
   });
}

void input_withdrawal_info::mark_as_unused_vin( const info_for_vin& obj )
{
   info_for_vins.modify<by_identifier>( obj.identifier, [&]( info_for_vin& o ) {
      o.used = false;
   });
}

void input_withdrawal_info::remove_info_for_vin( const info_for_vin& obj )
{
   info_for_vins.remove<by_identifier>( obj.identifier );
}

fc::optional<info_for_vin> input_withdrawal_info::find_info_for_vin( fc::sha256 identifier )
{ 
   return info_for_vins.find<by_identifier>( identifier );
}

std::vector<info_for_vin> input_withdrawal_info::get_info_for_vins()
{
   std::vector<info_for_vin> result;
   const auto max_vins = db.get_sidechain_params().maximum_condensing_tx_vins;

   info_for_vins.safe_for<by_id_resend_not_used>( [&]( info_for_vin_index::index<by_id_resend_not_used>::type::iterator itr_b,
                                                       info_for_vin_index::index<by_id_resend_not_used>::type::iterator itr_e )
   {
      for( size_t i = 0; itr_b != itr_e && i < max_vins && !itr_b->used; i++ ) {
         info_for_vin vin;
         vin.identifier = itr_b->identifier;
         vin.out.hash_tx = itr_b->out.hash_tx;
         vin.out.n_vout = itr_b->out.n_vout;
         vin.out.amount = itr_b->out.amount;
         vin.address = itr_b->address;
         vin.script = itr_b->script;
         
         result.push_back( vin );
         ++itr_b;
      }
   } );

   return result;
}

void input_withdrawal_info::insert_info_for_vout( const graphene::chain::account_id_type& payer, const std::string& data, const uint64_t& amount )
{
   db.create<graphene::chain::info_for_vout_object>([&](graphene::chain::info_for_vout_object& obj) {
      obj.payer = payer;
      obj.address = bitcoin_address( data );
      obj.amount = amount;
   });
}

void input_withdrawal_info::mark_as_used_vout( const graphene::chain::info_for_vout_object& obj )
{
   const auto& info_for_vout_idx = db.get_index_type<graphene::chain::info_for_vout_index>().indices().get< graphene::chain::by_id >();
   auto itr = info_for_vout_idx.find( obj.id );

   db.modify<graphene::chain::info_for_vout_object>( *itr, [&]( graphene::chain::info_for_vout_object& o ) {
      o.used = true;
   });
}

void input_withdrawal_info::mark_as_unused_vout( const graphene::chain::info_for_vout_object& obj )
{
   const auto& info_for_vout_idx = db.get_index_type<graphene::chain::info_for_vout_index>().indices().get< graphene::chain::by_id >();
   auto itr = info_for_vout_idx.find( obj.id );

   db.modify<graphene::chain::info_for_vout_object>( *itr, [&]( graphene::chain::info_for_vout_object& o ) {
      o.used = false;
   });
}

void input_withdrawal_info::remove_info_for_vout( const info_for_vout& obj )
{
   const auto& info_for_vout_idx = db.get_index_type<graphene::chain::info_for_vout_index>().indices().get< graphene::chain::by_id >();
   auto itr = info_for_vout_idx.find( obj.id );
   db.remove( *itr );
}

fc::optional<graphene::chain::info_for_vout_object> input_withdrawal_info::find_info_for_vout( graphene::chain::info_for_vout_id_type id )
{
   const auto& info_for_vout_idx = db.get_index_type<graphene::chain::info_for_vout_index>().indices().get< graphene::chain::by_id >();
   auto itr = info_for_vout_idx.find( id );
   if( itr != info_for_vout_idx.end() )
      return fc::optional<graphene::chain::info_for_vout_object>( *itr );
   return fc::optional<graphene::chain::info_for_vout_object>();
}

size_t input_withdrawal_info::size_info_for_vouts()
{
   const auto& info_for_vout_idx = db.get_index_type<graphene::chain::info_for_vout_index>().indices().get< graphene::chain::by_id >();
   return info_for_vout_idx.size();
}

std::vector<info_for_vout> input_withdrawal_info::get_info_for_vouts()
{
   std::vector<info_for_vout> result;

   const auto& info_for_vout_idx = db.get_index_type<graphene::chain::info_for_vout_index>().indices().get< graphene::chain::by_id_and_not_used >();
   const auto max_vouts = db.get_sidechain_params().maximum_condensing_tx_vouts;

   auto itr = info_for_vout_idx.begin();
   for(size_t i = 0; i < max_vouts && itr != info_for_vout_idx.end() && !itr->used; i++) {

      result.push_back( *itr );
      ++itr;
   }

   return result;
}

}
