#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <fc/crypto/sha256.hpp>

namespace graphene { namespace peerplays_sidechain {

enum class sidechain_type {
   bitcoin,
   //ethereum,
   //eos
};

using bytes = std::vector<char>;

struct prev_out
{
   bool operator!=( const prev_out& obj ) const
   {
      if( this->hash_tx != obj.hash_tx ||
          this->n_vout != obj.n_vout ||
          this->amount != obj.amount )
      {
         return true;
      }
      return false;
   }

   std::string hash_tx;
   uint32_t n_vout;
   uint64_t amount;
};

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

struct sidechain_event_data {
    sidechain_type sidechain;
    std::string transaction_id;
    std::string from;
    std::string to;
    int64_t amount;
};

} } // graphene::peerplays_sidechain

FC_REFLECT_ENUM(graphene::peerplays_sidechain::sidechain_type, (bitcoin) )
