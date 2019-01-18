#include <sidechain/bitcoin_script.hpp>
#include <sidechain/serialize.hpp>

namespace sidechain {

script_builder& script_builder::operator<<( op opcode )
{
   const auto op_byte = static_cast<uint8_t>( opcode );
   if ( op_byte < 0 || op_byte > 0xff )
      throw std::runtime_error( "script_builder::operator<<(OP): invalid opcode" );
   script.push_back( op_byte );
   return *this;
}

script_builder& script_builder::operator<<( uint8_t number )
{
   FC_ASSERT( 0 <= number && number <= 16 );

   if ( number == 0 )
      script.push_back( static_cast<uint8_t>( op::_0 ) );
   else
      script.push_back( static_cast<uint8_t>( op::_1 ) + number - 1 );

   return *this;
}

script_builder& script_builder::operator<<( size_t size )
{
   sidechain::write_compact_size( script, size );
   return *this;
}

script_builder& script_builder::operator<<( const bytes& sc ) {
   sidechain::write_compact_size( script, sc.size() );
   script.insert( script.end(), sc.begin(), sc.end() );
   return *this;
}

script_builder& script_builder::operator<<( const fc::sha256& hash )
{
   sidechain::write_compact_size( script, hash.data_size() );
   script.insert( script.end(), hash.data(), hash.data() + hash.data_size() );
   return *this;
}

script_builder& script_builder::operator<<( const fc::ripemd160& hash )
{
   sidechain::write_compact_size( script, hash.data_size() );
   script.insert( script.end(), hash.data(), hash.data() + hash.data_size() );
   return *this;
}

script_builder& script_builder::operator<<( const fc::ecc::public_key_data& pubkey_data )
{
   sidechain::write_compact_size( script, pubkey_data.size() );
   script.insert( script.end(), pubkey_data.begin(), pubkey_data.begin() + pubkey_data.size() );
   return *this;
}

}
