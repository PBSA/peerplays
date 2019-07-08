#include <graphene/chain/btc-sidechain/son_operations_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/btc-sidechain/son.hpp>

namespace graphene { namespace chain {

void_result create_son_member_evaluator::do_evaluate(const son_member_create_operation& op)
{ try{
    FC_ASSERT(db().get(op.owner_account).is_lifetime_member(), "Only Lifetime members may register a SON.");
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type create_son_member_evaluator::do_apply(const son_member_create_operation& op)
{ try {
    vote_id_type vote_id;
    db().modify(db().get_global_properties(), [&vote_id](global_property_object& p) {
        vote_id = get_next_vote_id(p, vote_id_type::son);
    });

    const auto& new_son_object = db().create<son_member_object>( [&]( son_member_object& obj ){
        obj.son_member_account = op.owner_account;
        obj.vote_id            = vote_id;
        obj.url                = op.url;
    });
    return new_son_object.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result delete_son_member_evaluator::do_evaluate(const son_member_delete_operation& op)
{ try {
    database& d = db();
    const auto& idx = d.get_index_type<son_member_index>().indices().get<by_account>();
    FC_ASSERT( idx.find(op.owner_account) != idx.end() );    
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result delete_son_member_evaluator::do_apply(const son_member_delete_operation& op)
{ try {
    const auto& idx = db().get_index_type<son_member_index>().indices().get<by_account>();
    db().remove(*idx.find(op.owner_account));
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // namespace graphene::chain
 