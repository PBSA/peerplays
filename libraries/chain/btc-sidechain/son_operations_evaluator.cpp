#include <graphene/chain/btc-sidechain/son_operations_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/btc-sidechain/son.hpp>

namespace graphene { namespace chain {

void_result create_son_member_evaluator::do_evaluate(const son_member_create_operation& op)
{ try{
    FC_ASSERT(db().get(op.owner_account).is_lifetime_member(), "Only Lifetime members may register a SON.");
    FC_ASSERT(db().get_balance(op.owner_account, asset_id_type()).amount >= db().get_global_properties().parameters.son_deposit_amount,
              "Not enought core asset to create SON." );
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

    db().adjust_balance( op.owner_account, -asset( db().get_global_properties().parameters.son_deposit_amount, asset_id_type() ) );

    return new_son_object.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result delete_son_member_evaluator::do_evaluate(const son_member_delete_operation& op)
{ try {
    database& d = db();
    const auto& idx = d.get_index_type<son_member_index>().indices().get<by_account>();\
    auto find_iter = idx.find(op.owner_account);
    FC_ASSERT( find_iter != idx.end(), "No SON registed for this account." );
    FC_ASSERT( !find_iter->time_of_deleting, "SON already on deleting stage." );
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result delete_son_member_evaluator::do_apply(const son_member_delete_operation& op)
{ try {
    const auto& idx = db().get_index_type<son_member_index>().indices().get<by_account>();
    db().modify( *idx.find(op.owner_account), [this]( son_member_object& o ) {
        o.time_of_deleting = db().head_block_time();
    });
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // namespace graphene::chain
 