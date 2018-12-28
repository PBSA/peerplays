#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/contract_object.hpp>
// #include <libdevcore/Common.h>
// #include <libethereum/Transaction.h>
// #include <eosio/chain/transaction.hpp>

namespace graphene { namespace chain {

    struct eth_op {
        account_id_type registrar;
        optional<contract_id_type> receiver;
        asset_id_type asset_id;
        uint64_t value;
        uint64_t gasPrice;
        uint64_t gas;
        std::string code;
    };

   //  struct wasm_op {
   //      eosio::chain::action msg;
   //  };

    struct contract_operation : public base_operation
    {
        struct fee_parameters_type{
            uint64_t fee       = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
            uint32_t price_per_kbyte = 10; /// only required for large memos.
        };

        asset               fee;
        account_id_type     registrar;

        uint8_t             version_vm;
        std::vector<char>   data;

        // string              code;

        // optional<eth_op>    eth;
        // optional<wasm_op>   wasm;

        account_id_type fee_payer()const { return registrar; }
        void            validate()const;
        share_type      calculate_fee(const fee_parameters_type& )const;
    };

    struct contract_transfer_operation : public base_operation
    {
       struct fee_parameters_type {
          uint64_t fee       = 0;
       };
    
       asset                fee;
       /// Contract to transfer asset from
       contract_id_type     from;
       /// Account or contract to transfer asset to
       object_id_type       to;
       /// The amount of asset to transfer from @ref from to @ref to
       asset                amount;
    
       /// User provided data encrypted to the memo key of the "to" account
       extensions_type   extensions;
    
       account_id_type fee_payer()const { return account_id_type(); }
       void            validate()const;
       share_type      calculate_fee(const fee_parameters_type& k)const;
    };

} } // graphene::chain

FC_REFLECT( graphene::chain::eth_op, (registrar)(receiver)(asset_id)(value)(gasPrice)(gas) )
// FC_REFLECT( graphene::chain::wasm_op, (msg) )
FC_REFLECT( graphene::chain::contract_operation::fee_parameters_type, (fee)(price_per_kbyte) )
// FC_REFLECT( graphene::chain::contract_operation, (fee)(registrar)(code)(eth) )
FC_REFLECT( graphene::chain::contract_operation, (fee)(registrar)(version_vm)(data) )

FC_REFLECT( graphene::chain::contract_transfer_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::contract_transfer_operation, (fee)(from)(to)(amount)(extensions) )
