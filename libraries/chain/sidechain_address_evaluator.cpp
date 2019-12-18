#include <graphene/chain/sidechain_address_evaluator.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/sidechain_address_object.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene { namespace chain {

void_result add_sidechain_address_evaluator::do_evaluate(const sidechain_address_add_operation& op)
{ try{

   const auto& idx = db().get_index_type<sidechain_address_index>().indices().get<by_account_and_sidechain>();
   FC_ASSERT( idx.find(boost::make_tuple(op.sidechain_address_account, op.sidechain)) == idx.end(), "Duplicated item" );
   return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type add_sidechain_address_evaluator::do_apply(const sidechain_address_add_operation& op)
{ try {
    const auto& new_sidechain_address_object = db().create<sidechain_address_object>( [&]( sidechain_address_object& obj ){
        obj.sidechain_address_account = op.sidechain_address_account;
        obj.sidechain = op.sidechain;
        obj.address = op.address;
        obj.private_key = op.private_key;
        obj.public_key = op.public_key;
    });
    return new_sidechain_address_object.id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result update_sidechain_address_evaluator::do_evaluate(const sidechain_address_update_operation& op)
{ try {
    FC_ASSERT(db().get(op.sidechain_address_id).sidechain_address_account == op.sidechain_address_account);
    const auto& idx = db().get_index_type<sidechain_address_index>().indices().get<by_id>();
    FC_ASSERT( idx.find(op.sidechain_address_id) != idx.end() );
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

object_id_type update_sidechain_address_evaluator::do_apply(const sidechain_address_update_operation& op)
{ try {
   const auto& idx = db().get_index_type<sidechain_address_index>().indices().get<by_id>();
   auto itr = idx.find(op.sidechain_address_id);
   if(itr != idx.end())
   {
       db().modify(*itr, [&op](sidechain_address_object &sao) {
           if(op.address.valid()) sao.address = *op.address;
           if(op.private_key.valid()) sao.private_key = *op.private_key;
           if(op.public_key.valid()) sao.public_key = *op.public_key;
       });
   }
   return op.sidechain_address_id;
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result delete_sidechain_address_evaluator::do_evaluate(const sidechain_address_delete_operation& op)
{ try {
    FC_ASSERT(db().get(op.sidechain_address_id).sidechain_address_account == op.sidechain_address_account);
    const auto& idx = db().get_index_type<sidechain_address_index>().indices().get<by_id>();
    FC_ASSERT( idx.find(op.sidechain_address_id) != idx.end() );
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

void_result delete_sidechain_address_evaluator::do_apply(const sidechain_address_delete_operation& op)
{ try {
    const auto& idx = db().get_index_type<sidechain_address_index>().indices().get<by_id>();
    auto sidechain_address = idx.find(op.sidechain_address_id);
    if(sidechain_address != idx.end()) {
       db().remove(*sidechain_address);
    }
    return void_result();
} FC_CAPTURE_AND_RETHROW( (op) ) }

} } // namespace graphene::chain
