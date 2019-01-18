#pragma once

#include <sidechain/types.hpp>

namespace sidechain {

enum class op {
   // push value
   _0 = 0x00,
   _1 = 0x51,
   _2 = 0x52,
   _3 = 0x53,
   _4 = 0x54,
   _5 = 0x55,
   _6 = 0x56,
   _7 = 0x57,
   _8 = 0x58,
   _9 = 0x59,
   _10 = 0x5a,
   _11 = 0x5b,
   _12 = 0x5c,
   _13 = 0x5d,
   _14 = 0x5e,
   _15 = 0x5f,
   _16 = 0x60,

   // control
   RETURN = 0x6a,

   // stack ops
   DUP = 0x76,

   // bit logic
   EQUAL = 0x87,
   EQUALVERIFY = 0x88,

   // crypto
   HASH160 = 0xa9,
   CHECKSIG = 0xac,
   CHECKMULTISIG = 0xae,
};

class script_builder {

public:

   script_builder& operator<<( op opcode );

   script_builder& operator<<( uint8_t number );

   script_builder& operator<<( size_t size );

   script_builder& operator<<( const bytes& sc );

   script_builder& operator<<( const fc::sha256& hash );

   script_builder& operator<<( const fc::ripemd160& hash );

   script_builder& operator<<( const fc::ecc::public_key_data& pubkey_data );

   operator bytes() const { return std::move( script ); }

private:

   bytes script;

};

}
