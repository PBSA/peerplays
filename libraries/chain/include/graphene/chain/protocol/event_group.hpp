/*
 * Copyright (c) 2018 Peerplays Blockchain Standards Association, and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/protocol/base.hpp>

namespace graphene { namespace chain {

struct event_group_create_operation : public base_operation
{
   struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
   asset             fee;

   /**
    * The name of the event_group
    */
   internationalized_string_type name;

   /**
    * This can be a sport_id_type, or a
    * relative object id that resolves to a sport_id_type
    */
   object_id_type sport_id;

   extensions_type   extensions;

   account_id_type fee_payer()const { return GRAPHENE_WITNESS_ACCOUNT; }
   void            validate()const;
};

struct event_group_update_operation : public base_operation
{
   struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
   asset             fee;

   event_group_id_type event_group_id;

    /**
    * This can be a sport_id_type, or a
    * relative object id that resolves to a sport_id_type
    */
   optional<object_id_type> new_sport_id;

   optional<internationalized_string_type> new_name;

   extensions_type   extensions;

   account_id_type fee_payer()const { return GRAPHENE_WITNESS_ACCOUNT; }
   void            validate()const;
};

struct event_group_delete_operation : public base_operation
{
   struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
   asset             fee;
    
   event_group_id_type event_group_id;
    
   extensions_type   extensions;
   
   account_id_type fee_payer()const { return GRAPHENE_WITNESS_ACCOUNT; }
   void            validate()const;
};

} }

FC_REFLECT( graphene::chain::event_group_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::event_group_create_operation, 
            (fee)(name)(sport_id)(extensions) )

FC_REFLECT( graphene::chain::event_group_update_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::event_group_update_operation,
            (fee)(new_sport_id)(new_name)(event_group_id)(extensions) )

FC_REFLECT( graphene::chain::event_group_delete_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::event_group_delete_operation,
            (fee)(event_group_id)(extensions) )
