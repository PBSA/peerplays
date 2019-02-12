#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <sidechain/input_withdrawal_info.hpp>
#include <sidechain/bitcoin_transaction.hpp>

namespace graphene { namespace chain {

   struct bitcoin_transaction_send_operation : public base_operation
   {
      struct fee_parameters_type { 
         uint64_t fee             = 0;
         uint32_t price_per_kbyte = 0;
      };

      asset                                          fee;
      account_id_type                                payer;

      sidechain::info_for_vin                        pw_vin;
      std::vector< sidechain::info_for_vin >         vins;
      std::vector< info_for_vout_id_type >           vouts;

      sidechain::bitcoin_transaction                 transaction;
      uint64_t                                       fee_for_size;

      account_id_type fee_payer()const { return payer; }
      void            validate()const {}
      share_type      calculate_fee( const fee_parameters_type& k )const {
         share_type fee_required = k.fee;
            fee_required += calculate_data_fee( fc::raw::pack_size(*this), k.price_per_kbyte );
         return fee_required;
      }
   };

   struct bitcoin_transaction_sign_operation : public base_operation
   {
      struct fee_parameters_type {
         uint64_t fee             = 0;
         uint32_t price_per_kbyte = 0;
      };

      asset                         fee;
      account_id_type               payer;
      proposal_id_type              proposal_id;
      std::vector<sidechain::bytes> signatures;

      account_id_type   fee_payer()const { return payer; }
      void              validate()const {}
      share_type        calculate_fee( const fee_parameters_type& k )const { return 0; }
   };

   struct revert_trx_info
   {
      revert_trx_info() = default;
      revert_trx_info( fc::sha256 trx_id, std::set< fc::sha256 > vins ): transaction_id( trx_id ), valid_vins( vins ) {}

      fc::sha256                transaction_id;
      std::set< fc::sha256 >    valid_vins;

      bool operator ==( const revert_trx_info& trx_info ){
         return this->transaction_id == trx_info.transaction_id && this->valid_vins == trx_info.valid_vins;
      }

      bool operator !=( const revert_trx_info& trx_info ){
         return this->transaction_id != trx_info.transaction_id || this->valid_vins != trx_info.valid_vins;
      }
   };

   struct bitcoin_transaction_revert_operation : public base_operation
   {
      struct fee_parameters_type {
         uint64_t fee             = 0;
         uint32_t price_per_kbyte = 0;
      };

      asset                          fee;
      account_id_type                payer;

      std::vector< revert_trx_info > transactions_info;


      account_id_type   fee_payer()const { return payer; }
      void              validate()const {}
      share_type        calculate_fee( const fee_parameters_type& k )const { return k.fee; }
   };

} } // graphene::chain

FC_REFLECT( graphene::chain::bitcoin_transaction_send_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::bitcoin_transaction_send_operation, (fee)(payer)(pw_vin)(vins)(vouts)(transaction)(fee_for_size) )

FC_REFLECT( graphene::chain::bitcoin_transaction_sign_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::bitcoin_transaction_sign_operation, (fee)(payer)(proposal_id)(signatures) )

FC_REFLECT( graphene::chain::revert_trx_info, (transaction_id)(valid_vins) )
FC_REFLECT( graphene::chain::bitcoin_transaction_revert_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::bitcoin_transaction_revert_operation, (fee)(payer)(transactions_info) )
