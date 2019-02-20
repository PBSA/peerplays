#pragma once

#include <graphene/chain/protocol/types.hpp>
#include <sidechain/types.hpp>
#include <fc/crypto/sha256.hpp>

namespace graphene { namespace chain { class database; } }

using graphene::chain::primary_wallet_vout_object;
using graphene::chain::primary_wallet_vout_id_type;

namespace sidechain {

class primary_wallet_vout_manager
{

public:
   primary_wallet_vout_manager( graphene::chain::database& _db ) : db( _db ) {}

   bool is_max_vouts() const;

   fc::optional< primary_wallet_vout_object > get_latest_unused_vout() const;

   fc::optional< primary_wallet_vout_object > get_vout( fc::sha256 hash_id ) const;
   
   void create_new_vout( const sidechain::prev_out& out );

   void delete_vouts_after( fc::sha256 hash_id );

   void confirm_vout( fc::sha256 hash_id );

   void mark_as_used_vout( fc::sha256 hash_id );

   void mark_as_unused_vout( fc::sha256 hash_id );

private:

   fc::sha256 create_hash_id( const std::string& hash_tx, const uint32_t& n_vout, const uint64_t& id ) const;

   fc::optional< primary_wallet_vout_id_type > get_vout_id( fc::sha256 hash_id ) const;

   graphene::chain::database& db;

};

}
