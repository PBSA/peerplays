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

struct event_create_operation : public base_operation
{
   struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
   asset             fee;

   /**
    * The name of the event
    */
   internationalized_string_type name;
   
   internationalized_string_type season;

   optional<time_point_sec> start_time;

   /**
    * This can be a event_group_id_type, or a
    * relative object id that resolves to a event_group_id_type
    */
   object_id_type event_group_id;

   extensions_type   extensions;

   account_id_type fee_payer()const { return GRAPHENE_WITNESS_ACCOUNT; }
   void            validate()const;
};

/**
 * The status of an event.  This is used to display in the UI, and setting
 * the event's status to certain values will propagate down to the 
 * betting market groups in the event:
 * - when set to `in_progress`, all betting market groups are set to `in_play`
 * - when set to `completed`, all betting market groups are set to `closed`
 * - when set to `frozen`, all betting market groups are set to `frozen`
 * - when set to `canceled`, all betting market groups are set to `canceled`
 */
enum class event_status
{
   upcoming,    /// Event has not started yet, betting is allowed
   in_progress, /// Event is in progress, if "in-play" betting is enabled, bets will be delayed
   frozen,      /// Betting is temporarily disabled
   finished,    /// Event has finished, no more betting allowed
   canceled,    /// Event has been canceled, all betting markets have been canceled
   settled,     /// All betting markets have been paid out
   STATUS_COUNT
};

struct event_update_operation : public base_operation
{
   struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
   asset             fee;

   event_id_type event_id;

   optional<object_id_type> new_event_group_id;

   optional<internationalized_string_type> new_name;

   optional<internationalized_string_type> new_season;

   optional<time_point_sec> new_start_time;

   optional<event_status> new_status;

   extensions_type   extensions;

   account_id_type fee_payer()const { return GRAPHENE_WITNESS_ACCOUNT; }
   void            validate()const;
};

/**
 * The current (or final) score of an event.
 * This is only used for display to the user, witnesses must resolve each
 * betting market explicitly.  
 * These are free-form strings that we assume will make sense to the user.
 * For a game like football, this may be a score like "3".  For races,
 * it could be a time like "1:53.4".
 */
struct event_update_status_operation : public base_operation
{
   struct fee_parameters_type { uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION; };
   asset             fee;

   /// the id of the event to update
   event_id_type event_id;

   /**
    * the new status of the event (if the status hasn't changed, the creator
    * of this operation must still set `status` to the event's current status)
    */
   event_status status;

   /*
    * scores for each competitor stored in same order as competitors in event_object 
    */
   vector<string> scores;

   extensions_type   extensions;

   account_id_type fee_payer()const { return GRAPHENE_WITNESS_ACCOUNT; }
   void            validate()const;
};

} }

FC_REFLECT( graphene::chain::event_create_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::event_create_operation, 
            (fee)(name)(season)(start_time)(event_group_id)(extensions) )

FC_REFLECT( graphene::chain::event_update_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::event_update_operation,
            (fee)(event_id)(new_event_group_id)(new_name)(new_season)(new_start_time)(new_status)(extensions) )

FC_REFLECT_ENUM( graphene::chain::event_status, (upcoming)(in_progress)(frozen)(finished)(canceled)(settled)(STATUS_COUNT) )
FC_REFLECT( graphene::chain::event_update_status_operation::fee_parameters_type, (fee) )
FC_REFLECT( graphene::chain::event_update_status_operation, 
            (fee)(event_id)(status)(scores)(extensions) )

