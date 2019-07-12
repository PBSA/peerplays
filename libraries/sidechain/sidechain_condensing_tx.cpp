#include <sidechain/sidechain_condensing_tx.hpp>

namespace sidechain {

sidechain_condensing_tx::sidechain_condensing_tx( const std::vector<info_for_vin>& vin_info, const std::vector<info_for_vout>& vout_info )
{
   create_vins_for_condensing_tx( vin_info );
   create_vouts_for_condensing_tx( vout_info );
}

void sidechain_condensing_tx::create_pw_vin( const info_for_vin& vin_info, bool front )
{
   bytes witness_script_temp = {0x22};
   witness_script_temp.insert( witness_script_temp.end(), vin_info.script.begin(), vin_info.script.end() );
   tb.add_in( payment_type::P2WSH, fc::sha256( vin_info.out.hash_tx ), vin_info.out.n_vout, witness_script_temp, front );

   amount_vins += vin_info.out.amount;
}

void sidechain_condensing_tx::create_pw_vout( const uint64_t amount, const bytes& wit_script_out, bool front )
{
   tb.add_out( payment_type::P2WSH, amount, bytes(wit_script_out.begin() + 2, wit_script_out.end()), front );
}

void sidechain_condensing_tx::create_vins_for_condensing_tx( const std::vector<info_for_vin>& vin_info )
{
   for( const auto& info : vin_info ) {
      bytes witness_script_temp = {0x22};
      witness_script_temp.insert( witness_script_temp.end(), info.script.begin(), info.script.end() );
      tb.add_in( payment_type::P2SH_WSH, fc::sha256( info.out.hash_tx ), info.out.n_vout, witness_script_temp );
      amount_vins += info.out.amount;
      count_transfer_vin++;
   }
}

void sidechain_condensing_tx::create_vouts_for_condensing_tx( const std::vector<info_for_vout>& vout_info )
{
   for( const auto& info : vout_info ) {
      tb.add_out_all_type( info.amount, info.address );
      amount_transfer_to_bitcoin += info.amount;
      count_transfer_vout++;
   }
}

void sidechain_condensing_tx::create_vouts_for_witness_fee( const accounts_keys& witness_active_keys, bool front )
{
   for( auto& key : witness_active_keys ) {
      tb.add_out( payment_type::P2PK, 0, key.second, front );
      count_witness_vout++;
   }
}

uint64_t sidechain_condensing_tx::get_estimate_tx_size( bitcoin_transaction tx, size_t number_witness )
{
   bytes temp_sig(72, 0x00);
   bytes temp_key(34, 0x00);
   bytes temp_script(3, 0x00);
   for(size_t i = 0; i < number_witness; i++) {
      temp_script.insert(temp_script.begin() + 1, temp_key.begin(), temp_key.end());
   }

   std::vector<bytes> temp_scriptWitness = { {},{temp_sig},{temp_sig},{temp_sig},{temp_sig},{temp_sig},{temp_script} };
   for( auto& vin : tx.vin ) {
      vin.scriptWitness = temp_scriptWitness;
   }

   return tx.get_vsize();
}

void sidechain_condensing_tx::subtract_fee( const uint64_t& fee, const uint16_t& witnesses_percentage )
{
   bitcoin_transaction tx = get_transaction();

   uint64_t fee_size = fee / ( count_transfer_vin + count_transfer_vout );
   if( fee % ( count_transfer_vin + count_transfer_vout ) != 0 ) {
      fee_size += 1;
   }

   bool is_pw_vin = tx.vout.size() > ( count_witness_vout + count_transfer_vout );
   if( is_pw_vin ) {
      tx.vout[0].value = tx.vout[0].value - fee_size * count_transfer_vin;
   }

   uint64_t fee_witnesses = 0;
   size_t offset = is_pw_vin ? 1 + count_witness_vout : count_witness_vout;
   for( ; offset < tx.vout.size(); offset++ ) {
      uint64_t amount_without_fee_size = tx.vout[offset].value - fee_size;
      uint64_t amount_fee_witness = ( amount_without_fee_size * witnesses_percentage ) / GRAPHENE_100_PERCENT;
      tx.vout[offset].value = amount_without_fee_size;
      tx.vout[offset].value -= amount_fee_witness;
      fee_witnesses += amount_fee_witness;
   }

   if( count_witness_vout > 0 ) {
      uint64_t fee_witness = fee_witnesses / count_witness_vout;
      size_t limit = is_pw_vin ? 1 + count_witness_vout : count_witness_vout;
      size_t offset = is_pw_vin ? 1 : 0;
      FC_ASSERT( fee_witness > 0 );
      for( ; offset < limit; offset++ ) {
         tx.vout[offset].value += fee_witness;
      }
   }

   tb = std::move( bitcoin_transaction_builder( tx ) );
}

}
