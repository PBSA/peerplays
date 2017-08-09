#include <fc/filesystem.hpp>
#include <fc/optional.hpp>
#include <fc/variant_object.hpp>
#include <fc/smart_ref_impl.hpp>

#include <graphene/app/application.hpp>

#include <graphene/chain/block_database.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/betting_market_object.hpp>

#include <graphene/chain/witness_object.hpp>

#include <graphene/utilities/key_conversion.hpp>

#include <graphene/bookie/bookie_api.hpp>
#include <graphene/bookie/bookie_plugin.hpp>

namespace graphene { namespace bookie {

namespace detail {

class bookie_api_impl
{
   public:
      bookie_api_impl(graphene::app::application& _app);

      binned_order_book get_binned_order_book(graphene::chain::betting_market_id_type betting_market_id, int32_t precision);
      std::shared_ptr<graphene::bookie::bookie_plugin> get_plugin();
      asset get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id);
      void get_events_containing_sub_string(std::vector<event_object>& events, const std::string& sub_string, const std::string& language);

      graphene::app::application& app;
};

bookie_api_impl::bookie_api_impl(graphene::app::application& _app) : app(_app)
{}


binned_order_book bookie_api_impl::get_binned_order_book(graphene::chain::betting_market_id_type betting_market_id, int32_t precision)
{
    std::shared_ptr<graphene::chain::database> db = app.chain_database();
    const auto& bet_odds_idx = db->get_index_type<graphene::chain::bet_object_index>().indices().get<graphene::chain::by_odds>();
    const chain_parameters& current_params = db->get_global_properties().parameters;

    graphene::chain::bet_multiplier_type bin_size = GRAPHENE_BETTING_ODDS_PRECISION;
    if (precision > 0)
        for (int32_t i = 0; i < precision; ++i) {
            FC_ASSERT(bin_size > (GRAPHENE_BETTING_MIN_MULTIPLIER - GRAPHENE_BETTING_ODDS_PRECISION), "invalid precision");
            bin_size /= 10;
        }
    else if (precision < 0)
        for (int32_t i = 0; i > precision; --i) {
            FC_ASSERT(bin_size < (GRAPHENE_BETTING_MAX_MULTIPLIER - GRAPHENE_BETTING_ODDS_PRECISION), "invalid precision");
            bin_size *= 10;
        }

    binned_order_book result; 

    // use a bet_object here for convenience. we really only use it to track the amount, odds, and back_or_lay
    // note, the current bin is accumulating the matching bets, so it will be of type 'lay' when we're 
    // binning 'back' bets, and vice versa
    fc::optional<bet_object> current_bin;

    auto flush_current_bin = [&current_bin, &result]()
    {
        if (current_bin) // do nothing if the current bin is empty
        {
            order_bin current_order_bin;

            current_order_bin.backer_multiplier = current_bin->backer_multiplier;
            current_order_bin.amount_to_bet = current_bin->get_approximate_matching_amount(true /* round up */);
            //idump((*current_bin)(current_order_bin));
            if (current_bin->back_or_lay == bet_type::lay)
                result.aggregated_back_bets.emplace_back(std::move(current_order_bin));
            else // current_bin is aggregating back positions
                result.aggregated_lay_bets.emplace_back(std::move(current_order_bin));

            current_bin.reset();
        }
    };

    // iterate through both sides of the order book (backs at increasing odds then lays at decreasing odds)
    for (auto bet_odds_iter = bet_odds_idx.lower_bound(std::make_tuple(betting_market_id));
         bet_odds_iter != bet_odds_idx.end() && betting_market_id == bet_odds_iter->betting_market_id;
         ++bet_odds_iter)
    {
        if (current_bin && 
            (bet_odds_iter->back_or_lay == current_bin->back_or_lay /* we have switched from back to lay bets */ ||
             (bet_odds_iter->back_or_lay == bet_type::back ? bet_odds_iter->backer_multiplier > current_bin->backer_multiplier :
                                                             bet_odds_iter->backer_multiplier < current_bin->backer_multiplier)))
            flush_current_bin();

        if (!current_bin)
        {
            // if there is no current bin, create one appropriate for the bet we're processing
            current_bin = graphene::chain::bet_object();

            // for back bets, we want to group all bets with odds from 3.0001 to 4 into the "4" bin
            // for lay bets, we want to group all bets with odds from 3 to 3.9999 into the "3" bin
            if (bet_odds_iter->back_or_lay == bet_type::back)
            {
               current_bin->backer_multiplier = (bet_odds_iter->backer_multiplier + bin_size - 1) / bin_size * bin_size;
               current_bin->backer_multiplier = std::min<graphene::chain::bet_multiplier_type>(current_bin->backer_multiplier, current_params.max_bet_multiplier);
               current_bin->back_or_lay = bet_type::lay;
            }
            else
            {
               current_bin->backer_multiplier = bet_odds_iter->backer_multiplier / bin_size * bin_size;
               current_bin->backer_multiplier = std::max<graphene::chain::bet_multiplier_type>(current_bin->backer_multiplier, current_params.min_bet_multiplier);
               current_bin->back_or_lay = bet_type::back;
            }

            current_bin->amount_to_bet.amount = 0;
        }

        current_bin->amount_to_bet.amount += bet_odds_iter->get_exact_matching_amount();
    }
    if (current_bin)
        flush_current_bin();

    return result;
}

std::shared_ptr<graphene::bookie::bookie_plugin> bookie_api_impl::get_plugin()
{
   return app.get_plugin<graphene::bookie::bookie_plugin>("bookie");
}

asset bookie_api_impl::get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id)
{
    return get_plugin()->get_total_matched_bet_amount_for_betting_market_group(group_id);
}

void bookie_api_impl::get_events_containing_sub_string(std::vector<event_object>& events, const std::string& sub_string, const std::string& language)
{
    get_plugin()->get_events_containing_sub_string(events, sub_string, language);
}

} // detail

bookie_api::bookie_api(graphene::app::application& app) :
   my(std::make_shared<detail::bookie_api_impl>(app))
{
}

binned_order_book bookie_api::get_binned_order_book(graphene::chain::betting_market_id_type betting_market_id, int32_t precision)
{
   return my->get_binned_order_book(betting_market_id, precision);
}

asset bookie_api::get_total_matched_bet_amount_for_betting_market_group(betting_market_group_id_type group_id)
{
    return my->get_total_matched_bet_amount_for_betting_market_group(group_id);
}

std::vector<event_object> bookie_api::get_events_containing_sub_string(const std::string& sub_string, const std::string& language)
{
    std::vector<event_object> events;
    my->get_events_containing_sub_string(events, sub_string, language);
    return events;
}


} } // graphene::bookie


