#include <sidechain/network/sidechain_net_manager.hpp>
#include <sidechain/serialize.hpp>
#include <sidechain/sidechain_parameters.hpp>

#include <fc/network/http/connection.hpp>
#include <fc/network/ip.hpp>

#include <thread>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <graphene/chain/bitcoin_address_object.hpp>

namespace sidechain {

sidechain_net_manager::sidechain_net_manager( graphene::chain::database* _db, std::string _ip, 
                                             uint32_t _zmq, uint32_t _rpc, std::string _user, std::string _password )
{
   initialize_manager(_db, _ip, _zmq, _rpc, _user, _password );
}

void sidechain_net_manager::initialize_manager( graphene::chain::database* _db, std::string _ip, 
                                             uint32_t _zmq, uint32_t _rpc, std::string _user, std::string _password )
{
   fc::http::connection conn;
   try {
      conn.connect_to( fc::ip::endpoint( fc::ip::address( _ip ), _rpc ) );
   } catch ( fc::exception e ) {
      elog( "No BTC node running at ${ip} or wrong rpc port: ${port}", ("ip", _ip) ("port", _rpc) );
      FC_ASSERT( false );
   }

   listener = std::unique_ptr<zmq_listener>( new zmq_listener( _ip, _zmq ) );
   bitcoin_client = std::unique_ptr<bitcoin_rpc_client>( new bitcoin_rpc_client( _ip, _rpc, _user, _password ) );
   db = _db;

   listener->block_received.connect([this]( const std::string& block_hash ) {
      std::thread( &sidechain_net_manager::handle_block, this, block_hash ).detach();
   } );

   db->send_btc_tx.connect([this]( const sidechain::bitcoin_transaction& trx ) {
      std::thread( &sidechain_net_manager::send_btc_tx, this, trx ).detach();
   } );
}

void sidechain_net_manager::update_tx_infos( const std::string& block_hash )
{
   std::string block = bitcoin_client->receive_full_block( block_hash );
   if( block != "" ) {
      const auto& vins = extract_info_from_block( block );
      const auto& addr_idx = db->get_index_type<bitcoin_address_index>().indices().get<by_address>();
      for( const auto& v : vins ) {
         const auto& addr_itr = addr_idx.find( v.address );
         FC_ASSERT( addr_itr != addr_idx.end() );
         db->i_w_info.insert_info_for_vin( prev_out{ v.out.hash_tx, v.out.n_vout, v.out.amount }, v.address, addr_itr->address.get_witness_script() );
      }
   }
}

void sidechain_net_manager::update_tx_approvals()
{
   std::vector<fc::sha256> trx_for_check;
   const auto& confirmations_num = db->get_sidechain_params().confirmations_num;

   db->bitcoin_confirmations.safe_for<by_hash>([&]( btc_tx_confirmations_index::iterator itr_b, btc_tx_confirmations_index::iterator itr_e ){
      for(auto iter = itr_b; iter != itr_e; iter++) {
         db->bitcoin_confirmations.modify<by_hash>( iter->transaction_id, [&]( bitcoin_transaction_confirmations& obj ) {
            obj.count_block++;
         });

         if( iter->count_block == confirmations_num ) {
            trx_for_check.push_back( iter->transaction_id );
         }
      }
   });

   update_transaction_status( trx_for_check );

}

void sidechain_net_manager::update_estimated_fee()
{
   db->estimated_feerate = bitcoin_client->receive_estimated_fee();
}

void sidechain_net_manager::send_btc_tx( const sidechain::bitcoin_transaction& trx )
{
   std::set<fc::sha256> valid_vins;
   for( const auto& v : trx.vin ) {
      valid_vins.insert( v.prevout.hash );
   }
   db->bitcoin_confirmations.insert( bitcoin_transaction_confirmations( trx.get_txid(), valid_vins ) );

   FC_ASSERT( !bitcoin_client->connection_is_not_defined() );
   const auto tx_hex = fc::to_hex( pack( trx ) );

   bitcoin_client->send_btc_tx( tx_hex );
}

bool sidechain_net_manager::connection_is_not_defined() const
{
   return listener->connection_is_not_defined() && bitcoin_client->connection_is_not_defined();
}

void sidechain_net_manager::handle_block( const std::string& block_hash )
{
   update_tx_approvals();
   update_estimated_fee();
   update_tx_infos( block_hash );
}

std::vector<info_for_vin> sidechain_net_manager::extract_info_from_block( const std::string& _block )
{
   std::stringstream ss( _block );
   boost::property_tree::ptree block;
   boost::property_tree::read_json( ss, block );

   std::vector<info_for_vin> result;

   const auto& addr_idx = db->get_index_type<bitcoin_address_index>().indices().get<by_address>();

   for (const auto& tx_child : block.get_child("tx")) {
      const auto& tx = tx_child.second;

      for ( const auto& o : tx.get_child("vout") ) {
         const auto script = o.second.get_child("scriptPubKey");

         if( !script.count("addresses") ) continue;

         for (const auto& addr : script.get_child("addresses")) { // in which cases there can be more addresses?
            const auto address_base58 = addr.second.get_value<std::string>();

            if( !addr_idx.count( address_base58 ) ) continue;

            info_for_vin vin;
            vin.out.hash_tx = tx.get_child("txid").get_value<std::string>();
            vin.out.amount = parse_amount( o.second.get_child( "value" ).get_value<std::string>() );
            vin.out.n_vout = o.second.get_child( "n" ).get_value<uint32_t>();
            vin.address = address_base58;
            result.push_back( vin );
         }
      }
   }

   return result;
}

std::set<fc::sha256> sidechain_net_manager::get_valid_vins( const std::string tx_hash )
{
   const auto& confirmations_obj = db->bitcoin_confirmations.find<sidechain::by_hash>( fc::sha256( tx_hash ) );
   FC_ASSERT( confirmations_obj.valid() );

   std::set<fc::sha256> valid_vins;
   for( const auto& v : confirmations_obj->valid_vins ) {
      auto confirmations = bitcoin_client->receive_confirmations_tx( v.str() );
      if( confirmations == 0 ) {
         continue;
      }
      valid_vins.insert( v );
   }
   return valid_vins;
}

void sidechain_net_manager::update_transaction_status( std::vector<fc::sha256> trx_for_check )
{
   const auto& confirmations_num = db->get_sidechain_params().confirmations_num;

   for( const auto& trx : trx_for_check ) {
      auto confirmations = bitcoin_client->receive_confirmations_tx( trx.str() );
      db->bitcoin_confirmations.modify<by_hash>( trx, [&]( bitcoin_transaction_confirmations& obj ) {
         obj.count_block = confirmations;
      });

      if( confirmations >= confirmations_num ) {
         db->bitcoin_confirmations.modify<by_hash>( trx, [&]( bitcoin_transaction_confirmations& obj ) {
            obj.confirmed = true;
         });

      } else if( confirmations == 0 ) {
         auto is_in_mempool =  bitcoin_client->receive_mempool_entry_tx( trx.str() );

         std::set<fc::sha256> valid_vins;
         if( !is_in_mempool ) {
            valid_vins = get_valid_vins( trx.str() );
         }

         db->bitcoin_confirmations.modify<by_hash>( trx, [&]( bitcoin_transaction_confirmations& obj ) {
            obj.missing = !is_in_mempool;
            obj.valid_vins = valid_vins;
         });
      }
   }
}

// Removes dot from amount output: "50.00000000"
inline uint64_t sidechain_net_manager::parse_amount(std::string raw) {
    raw.erase(std::remove(raw.begin(), raw.end(), '.'), raw.end());
    return std::stoll(raw);
}

}