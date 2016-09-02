#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {

   class tournament_create_evaluator : public evaluator<tournament_create_evaluator>
   {
      public:
         typedef tournament_create_operation operation_type;

         void_result do_evaluate( const tournament_create_operation& o );
         object_id_type do_apply( const tournament_create_operation& o );
   };

   class tournament_join_evaluator : public evaluator<tournament_join_evaluator>
   {
      private:
         const tournament_object* _tournament_obj = nullptr;
         const tournament_details_object* _tournament_details_obj = nullptr;
         const account_object* _payer_account = nullptr;
         const asset_object* _buy_in_asset_type = nullptr;
      public:
         typedef tournament_join_operation operation_type;

         void_result do_evaluate( const tournament_join_operation& o );
         void_result do_apply( const tournament_join_operation& o );
   };

} }
