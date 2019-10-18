#include <graphene/chain/database.hpp>
#include <graphene/chain/son_object.hpp>

namespace graphene { namespace chain {
   void son_object::pay_son_fee(share_type pay, database& db) {
      db.adjust_balance(son_account, pay);
   }
}}
