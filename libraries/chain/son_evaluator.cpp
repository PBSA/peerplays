#include <graphene/chain/son_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/son_object.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene { namespace chain {

void_result create_son_evaluator::do_evaluate(const son_create_operation& op)
{ try{
   FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK");
   FC_ASSERT(db().get(op.owner_account).is_lifetime_member(), "Only Lifetime members may register a SON.");
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type create_son_evaluator::do_apply(const son_create_operation& op)
{ try {
    vote_id_type vote_id;
    db().modify(db().get_global_properties(), [&vote_id](global_property_object& p) {
        vote_id = get_next_vote_id(p, vote_id_type::son);
    });

    const auto& new_son_object = db().create<son_object>( [&]( son_object& obj ){
        obj.son_account = op.owner_account;
        obj.vote_id = vote_id;
        obj.url = op.url;
        obj.deposit = op.deposit;
        obj.signing_key = op.signing_key;
        obj.pay_vb = op.pay_vb;
    });
    return new_son_object.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result update_son_evaluator::do_evaluate(const son_update_operation& op)
{ try {
    FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON HARDFORK"); // can be removed after HF date pass
    FC_ASSERT(db().get(op.son_id).son_account == op.owner_account);
    const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
    FC_ASSERT( idx.find(op.son_id) != idx.end() );
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type update_son_evaluator::do_apply(const son_update_operation& op)
{ try {
   const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
   auto itr = idx.find(op.son_id);
   if(itr != idx.end())
   {
       db().modify(*itr, [&op](son_object &so) {
           if(op.new_url.valid()) so.url = *op.new_url;
           if(op.new_deposit.valid()) so.deposit = *op.new_deposit;
           if(op.new_signing_key.valid()) so.signing_key = *op.new_signing_key;
           if(op.new_pay_vb.valid()) so.pay_vb = *op.new_pay_vb;
       });
   }
   return op.son_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result delete_son_evaluator::do_evaluate(const son_delete_operation& op)
{ try {
    FC_ASSERT(db().head_block_time() >= HARDFORK_SON_TIME, "Not allowed until SON_HARDFORK"); // can be removed after HF date pass
    FC_ASSERT(db().get(op.son_id).son_account == op.owner_account);
    const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
    FC_ASSERT( idx.find(op.son_id) != idx.end() );
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result delete_son_evaluator::do_apply(const son_delete_operation& op)
{ try {
    const auto& idx = db().get_index_type<son_index>().indices().get<by_id>();
    db().remove(*idx.find(op.son_id));
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // namespace graphene::chain
