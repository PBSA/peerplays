#define DEFAULT_LOGGER "betting"

#include <graphene/chain/event_group_object.hpp>
#include <graphene/chain/event_object.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {
    
void event_group_object::cancel_events(database& db) const
{
    const auto& events_for_group = db.get_index_type<event_object_index>().indices().get<by_event_group_id>();
    auto event_it = events_for_group.lower_bound(id);
    auto event_end_it = events_for_group.upper_bound(id);
    for (; event_it != event_end_it; ++event_it)
    {
        db.modify( *event_it, [&](event_object& event_obj) {
            event_obj.dispatch_new_status(db, event_status::canceled);
        });
    }
}

}}
