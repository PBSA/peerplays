#pragma once
#include <graphene/chain/protocol/types.hpp>


namespace sidechain {

   struct sidechain_parameters_extension {
      uint8_t multisig_sigs_num           = SIDECHAIN_DEFAULT_NUMBER_SIG_MULTISIG;
      uint8_t condensing_tx_vins_num      = SIDECHAIN_DEFAULT_CONDENSING_TX_VINS_NUMBER;
      uint8_t condensing_tx_vouts_num     = SIDECHAIN_DEFAULT_CONDENSING_TX_VOUTS_NUMBER;

      graphene::chain::account_id_type managing_account;
      graphene::chain::asset_id_type asset_id;
   };

}

FC_REFLECT( sidechain::sidechain_parameters_extension, 
            (multisig_sigs_num)
            (condensing_tx_vins_num)
            (condensing_tx_vins_num)
            (managing_account)
            (asset_id)
)