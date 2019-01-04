#include <graphene/chain/protocol/contract.hpp>

namespace graphene { namespace chain {

    share_type contract_operation::calculate_fee( const fee_parameters_type& schedule )const
    {
        share_type core_fee_required = schedule.fee;
        if( data.size() > 0 )
            core_fee_required += calculate_data_fee( data.size(), schedule.price_per_kbyte );
        return core_fee_required;
    }


    void contract_operation::validate() const
    {
        // FC_ASSERT( !eth.valid() );
    }

    share_type contract_transfer_operation::calculate_fee( const fee_parameters_type& schedule )const
    {
        return schedule.fee;
    }

    void contract_transfer_operation::validate()const
    {
        FC_ASSERT( fee.amount >= 0 );
        FC_ASSERT( to.is<account_id_type>() || to.is<contract_id_type>() );
        FC_ASSERT( amount.amount > 0 );
    }

} } // graphene::chain
