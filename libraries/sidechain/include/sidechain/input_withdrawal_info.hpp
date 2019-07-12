#pragma once

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include <sidechain/types.hpp>
#include <sidechain/thread_safe_index.hpp>
#include <fc/crypto/sha256.hpp>

#include <graphene/chain/info_for_vout_object.hpp>
#include <graphene/chain/protocol/types.hpp>

using boost::multi_index_container;
using namespace boost::multi_index;

namespace graphene { namespace chain { class database; } }

namespace sidechain {

struct info_for_vin
{
   info_for_vin() = default;

   info_for_vin( const prev_out& _out, const std::string& _address, bytes _script = bytes(), bool _resend = false );

   bool operator!=( const info_for_vin& obj ) const;

   struct comparer {
      bool operator() ( const info_for_vin& lhs, const info_for_vin& rhs ) const;
   };

   static uint64_t count_id_info_for_vin;
   uint64_t id;

   fc::sha256 identifier;

   prev_out out;
   std::string address;
   bytes script;

   bool used = false;
   bool resend = false;
};

struct by_id;
struct by_identifier;
struct by_id_resend_not_used;

using info_for_vin_index = boost::multi_index_container<info_for_vin,
   indexed_by<
      ordered_unique<tag<by_id>, member<info_for_vin, uint64_t, &info_for_vin::id>>,
      ordered_unique<tag<by_identifier>, member<info_for_vin, fc::sha256, &info_for_vin::identifier>>,
      ordered_non_unique<tag<by_id_resend_not_used>, identity< info_for_vin >, info_for_vin::comparer >
   >
>;

class input_withdrawal_info
{

public:
   input_withdrawal_info( graphene::chain::database& _db ) : db( _db ) {}


   std::vector<sidechain::bytes> get_redeem_scripts( const std::vector<info_for_vin>& info_vins );

   std::vector<uint64_t> get_amounts( const std::vector<info_for_vin>& info_vins );


   fc::optional<info_for_vin> get_info_for_pw_vin();


   void insert_info_for_vin( const prev_out& out, const std::string& address, bytes script = bytes(), bool resend = false );

   void modify_info_for_vin( const info_for_vin& obj, const std::function<void( info_for_vin& e )>& func );

   void mark_as_used_vin( const info_for_vin& obj );

   void mark_as_unused_vin( const info_for_vin& obj );

   void remove_info_for_vin( const info_for_vin& obj );

   fc::optional<info_for_vin> find_info_for_vin( fc::sha256 identifier );

   size_t size_info_for_vins() { return info_for_vins.size(); }

   std::vector<info_for_vin> get_info_for_vins();


   void insert_info_for_vout( const graphene::chain::account_id_type& payer, const std::string& data, const uint64_t& amount );

   void mark_as_used_vout( const graphene::chain::info_for_vout_object& obj );

   void mark_as_unused_vout( const graphene::chain::info_for_vout_object& obj );

   void remove_info_for_vout( const info_for_vout& obj );

   fc::optional<graphene::chain::info_for_vout_object> find_info_for_vout( graphene::chain::info_for_vout_id_type id );

   size_t size_info_for_vouts();

   std::vector<info_for_vout> get_info_for_vouts();

private:

   graphene::chain::database& db;

   thread_safe_index<info_for_vin_index> info_for_vins;

};

}

FC_REFLECT( sidechain::info_for_vin, (identifier)(out)(address)(script)(used)(resend) )