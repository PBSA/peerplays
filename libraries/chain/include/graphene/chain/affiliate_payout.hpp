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

#include <graphene/chain/protocol/asset.hpp>
#include <graphene/chain/protocol/affiliate.hpp>
#include <graphene/chain/betting_market_object.hpp>
#include <graphene/chain/tournament_object.hpp>

namespace graphene { namespace chain {
   class database;

   namespace impl {
      class game_type_visitor {
      public:
         typedef app_tag result_type;

         inline app_tag operator()( const rock_paper_scissors_game_options& o )const { return rps; }
      };
   }

   template<typename GAME>
   app_tag get_tag_for_game( const GAME& game );

   template<>
   inline app_tag get_tag_for_game( const betting_market_group_object& game )
   {
      return bookie;
   }

   template<>
   inline app_tag get_tag_for_game( const tournament_object& game )
   {
      return game.options.game_options.visit( impl::game_type_visitor() );
   }

   class affiliate_payout_helper {
   public:
      template<typename GAME>
      affiliate_payout_helper( database& db, const GAME& game )
      : _db(db), tag( get_tag_for_game( game ) ) {}

      share_type payout( account_id_type player, const asset& amount );
      share_type payout( const account_object& player, const asset& amount );
      void commit();

   private:
      database& _db;
      app_tag tag;
      std::map<account_id_type, asset> accumulator;
   };

} } // graphene::chain
