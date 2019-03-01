#include <graphene/chain/manager.hpp>
#include <graphene/chain/sport_object.hpp>
#include <graphene/chain/event_group_object.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/betting_market_object.hpp>

namespace graphene { namespace chain {

bool is_manager( const database& db, const sport_id_type& sport_id, const account_id_type& manager_id )
{
    return sport_id(db).manager == manager_id || GRAPHENE_WITNESS_ACCOUNT == manager_id;
} 

bool is_manager( const database& db, const event_group_id_type& event_group_id, const account_id_type& manager_id )
{
    const event_group_object& event_group_obj = event_group_id(db);
    if( event_group_obj.manager == manager_id || GRAPHENE_WITNESS_ACCOUNT == manager_id ) 
        return true;
    
    return is_manager( db, event_group_obj.sport_id, manager_id );
}   

bool is_manager( const database& db, const event_id_type& event_id, const account_id_type& manager_id )
{
    const event_object& event_obj = event_id(db);
    if( event_obj.manager == manager_id || GRAPHENE_WITNESS_ACCOUNT == manager_id ) 
        return true;

    return is_manager( db, event_obj.event_group_id, manager_id );
}

bool is_manager( const database& db, const betting_market_group_id_type& betting_market_group_id,
    const account_id_type& manager_id ) 
{
    return GRAPHENE_WITNESS_ACCOUNT == manager_id || is_manager( db, betting_market_group_id(db).event_id, manager_id );
}

bool is_manager( const database& db, const betting_market_id_type& betting_market_id,
    const account_id_type& manager_id )
{
    return GRAPHENE_WITNESS_ACCOUNT == manager_id || is_manager( db, betting_market_id(db).group_id, manager_id );
}

} } // graphene::chain