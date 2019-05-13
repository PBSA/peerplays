#pragma once
#include <graphene/chain/database.hpp>
#include <graphene/chain/protocol/types.hpp>

namespace graphene { namespace chain {

/**
 * Determine whether the fee_paying_account for certain operations, in the context of
 * sport_obj, event_group_obj, event_obj, betting_market_group_obj and betting_market_obj
 * is the manager of this, or of the objects hierarchically above.
 */

bool is_manager( const database& db, const sport_id_type& sport_id, const account_id_type& manager_id );

bool is_manager( const database& db, const event_group_id_type& event_group_id, const account_id_type& manager_id );

bool is_manager( const database& db, const event_id_type& event_id, const account_id_type& manager_id );

bool is_manager( const database& db, const betting_market_group_id_type& betting_market_group_id_type,
                 const account_id_type& manager_id );

bool is_manager( const database& db, const betting_market_id_type& betting_market_id_type,
                 const account_id_type& manager_id );

} } // graphene::chain