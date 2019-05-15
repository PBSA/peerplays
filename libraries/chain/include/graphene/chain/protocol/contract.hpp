#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/types.hpp>

namespace graphene { namespace chain {

    /**
     * @struct eth_op
     * @brief ethereum operation struct
     * This struct describes Ethereum trx and enviroment for this trx execution
     */
    struct eth_op {
        /// Account id of regisrar account
        account_id_type registrar;
        /**
         * @brief fc::option with contract-receiver id
         * This fc::option describes type of Ethereum trx
         * If option.valid() == false, then it is contract creation
         * Else it is contract call
         */
        optional<contract_id_type> receiver;
        /**
         * @brief std::set of allowed assets for contract
         * This set describes allowed assets for contract (method transferBalance in state)
         * Empty set means that all assets allowed
         */
        std::set<uint64_t> allowed_assets;
        /// Asset id for transfer amount
        asset_id_type asset_id_transfer;
        /// Value for transfer
        uint64_t value;
        /// Asset id for gas
        asset_id_type asset_id_gas;
        /// Gas price for Ethereum trx
        uint64_t gasPrice;
        /// Gas limit for Ethereum trx
        uint64_t gas;
        /**
         * @brief Code of Ethereum trx
         * If it contract creation, then it is contract code
         * Else it is contract method signature and args for this method (if need)
         */
        std::string code;
    };

    struct contract_operation : public base_operation
    {
        struct fee_parameters_type{
            uint64_t fee       = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
            uint32_t price_per_kbyte = 10; /// only required for large memos.
        };

        /// Fee for `contract_operation`
        asset               fee;
        /// Fee payer
        account_id_type     registrar;

        /// Type of VM, which will be run
        vm_types          vm_type;
        /// Serealized data for VM
        std::vector<unsigned char>   data;

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

FC_REFLECT( graphene::chain::eth_op, (registrar)(receiver)(allowed_assets)(asset_id_transfer)(value)(asset_id_gas)(gasPrice)(gas)(code) )
FC_REFLECT( graphene::chain::contract_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::contract_operation, (fee)(registrar)(vm_type)(data) )

FC_REFLECT( graphene::chain::contract_transfer_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::contract_transfer_operation, (fee)(from)(to)(amount)(extensions) )
