#pragma once

#include <memory>
#include <string>

#include <fc/api.hpp>
#include <fc/variant_object.hpp>

#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/protocol/asset.hpp>
#include <graphene/chain/event_object.hpp>

using namespace graphene::chain;

namespace graphene { namespace app {
   class application;
} }

namespace graphene { namespace bookie {

namespace detail {
   class bookie_api_impl;
}

struct order_bin {
   graphene::chain::share_type amount_to_bet;
   graphene::chain::bet_multiplier_type backer_multiplier;
};

struct binned_order_book {
   std::vector<order_bin> aggregated_back_bets;
   std::vector<order_bin> aggregated_lay_bets;
};

struct matched_bet_object {
   // all fields from bet_object
   bet_id_type id;

   account_id_type bettor_id;
   
   betting_market_id_type betting_market_id;

   asset amount_to_bet; // this is the original amount, not the amount remaining

   bet_multiplier_type backer_multiplier;

   bet_type back_or_lay;

   fc::optional<fc::time_point_sec> end_of_delay;

   // plus fields from this plugin
   share_type amount_matched;

   std::vector<operation_history_id_type> associated_operations;
};

class bookie_api
{
   public:
      bookie_api(graphene::app::application& app);

      /**
       * Returns the current order book, binned according to the given precision.
       * precision = 1 means bin using one decimal place.  for backs, (1 - 1.1], (1.1 - 1.2], etc.
       * precision = 2 would bin on (1 - 1.01], (1.01 - 1.02]
       */
      binned_order_book get_binned_order_book(graphene::chain::betting_market_id_type betting_market_id, int32_t precision);
      asset get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id);
      std::vector<event_object> get_events_containing_sub_string(const std::string& sub_string, const std::string& language);
      fc::variants get_objects(const vector<object_id_type>& ids)const;
      std::vector<matched_bet_object> get_matched_bets_for_bettor(account_id_type bettor_id) const;
      std::vector<matched_bet_object> get_all_matched_bets_for_bettor(account_id_type bettor_id, bet_id_type start = bet_id_type(), unsigned limit = 1000) const;
      std::shared_ptr<detail::bookie_api_impl> my;
};

} }

FC_REFLECT(graphene::bookie::order_bin, (amount_to_bet)(backer_multiplier))
FC_REFLECT(graphene::bookie::binned_order_book, (aggregated_back_bets)(aggregated_lay_bets))
FC_REFLECT(graphene::bookie::matched_bet_object, (id)(bettor_id)(betting_market_id)(amount_to_bet)(backer_multiplier)(back_or_lay)(end_of_delay)(amount_matched)(associated_operations))

FC_API(graphene::bookie::bookie_api,
       (get_binned_order_book)
       (get_total_matched_bet_amount_for_betting_market_group)
       (get_events_containing_sub_string)
       (get_objects)
       (get_matched_bets_for_bettor)
       (get_all_matched_bets_for_bettor))

