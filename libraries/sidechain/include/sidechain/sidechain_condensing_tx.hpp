#pragma once
#include <sidechain/input_withdrawal_info.hpp>
#include <sidechain/bitcoin_transaction.hpp>

using namespace sidechain;

namespace sidechain {

class sidechain_condensing_tx
{

public:

   sidechain_condensing_tx( const std::vector<info_for_vin>& vin_info, const std::vector<info_for_vout>& vout_info );

   void create_pw_vin( const info_for_vin& vin_info, bool front = true );
   
   void create_pw_vout( const uint64_t amount, const bytes& wit_script_out, bool front = true );

   void create_vouts_for_witness_fee( const accounts_keys& witness_active_keys, bool front = true );

   static uint64_t get_estimate_tx_size(  bitcoin_transaction tx, size_t number_witness  );

   void subtract_fee( const uint64_t& fee, const uint16_t& witnesses_percentage );

   bitcoin_transaction get_transaction() const { return tb.get_transaction(); }

   uint64_t get_amount_vins() { return amount_vins; }

   uint64_t get_amount_transfer_to_bitcoin() { return amount_transfer_to_bitcoin; }

private:

   void create_vins_for_condensing_tx( const std::vector<info_for_vin>& vin_info );

   void create_vouts_for_condensing_tx( const std::vector<info_for_vout>& vout_info );

   uint32_t count_transfer_vin = 0;
   
   uint32_t count_transfer_vout = 0;
   
   uint32_t count_witness_vout = 0;

   uint64_t amount_vins = 0;

   uint64_t amount_transfer_to_bitcoin = 0;

   bitcoin_transaction_builder tb;

};

}
