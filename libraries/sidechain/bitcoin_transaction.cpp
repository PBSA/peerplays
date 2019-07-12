#include <sidechain/bitcoin_transaction.hpp>
#include <fc/crypto/base58.hpp>
#include <sidechain/bitcoin_script.hpp>
#include <sidechain/serialize.hpp>

namespace sidechain {

bool out_point::operator==( const out_point& op ) const
{
   if( this->hash == op.hash &&
       this->n == op.n )
   {
      return true;
   }
   return false;
}

bool tx_in::operator==( const tx_in& ti ) const
{
   if( this->prevout == ti.prevout &&
       this->scriptSig == ti.scriptSig &&
       this->nSequence == ti.nSequence )
   {
      return true;
   }
   return false;
}

bool tx_out::operator==( const tx_out& to ) const
{
   if( this->value == to.value &&
       this->scriptPubKey == to.scriptPubKey )
   {
      return true;
   }
   return false;
}

bool tx_out::is_p2wsh() const
{
   if( scriptPubKey.size() == 34 && scriptPubKey[0] == static_cast<char>(0x00) && scriptPubKey[1] == static_cast<char>(0x20) ) {
      return true;
   }
   return false;
}

bool tx_out::is_p2wpkh() const
{
   if( scriptPubKey.size() == 22 && scriptPubKey[0] == static_cast<char>(0x00) && scriptPubKey[1] == static_cast<char>(0x14) ) {
      return true;
   }
   return false;
}

bool tx_out::is_p2pkh() const
{
   if( scriptPubKey.size() == 25 && scriptPubKey[0] == static_cast<char>(0x76) && scriptPubKey[1] == static_cast<char>(0xa9) &&
      scriptPubKey[2] == static_cast<char>(0x14) && scriptPubKey[23] == static_cast<char>(0x88) && scriptPubKey[24] == static_cast<char>(0xac) ) {
         return true;
   }
   return false;
}

bool tx_out::is_p2sh() const
{
   if( scriptPubKey.size() == 23 && scriptPubKey[0] == static_cast<char>(0xa9) &&
      scriptPubKey[1] == static_cast<char>(0x14) && scriptPubKey[22] == static_cast<char>(0x87) ) {
         return true;
   }
   return false;
}

bool tx_out::is_p2pk() const
{
   if( scriptPubKey.size() == 35 && scriptPubKey[0] == static_cast<char>(0x21) && scriptPubKey[34] == static_cast<char>(0xac) ) {
      return true;
   }
   return false;
}

bytes tx_out::get_data_or_script() const
{
   if( is_p2pkh() ) {
      return bytes( scriptPubKey.begin() + 3, scriptPubKey.begin() + 23 );
   } else if( is_p2sh() || is_p2wpkh() ) {
      return bytes( scriptPubKey.begin() + 2, scriptPubKey.begin() + 22 );
   } else if( is_p2wsh() ) {
      return bytes( scriptPubKey.begin() + 2, scriptPubKey.begin() + 34 );
   } else if( is_p2pk() ) {
      return bytes( scriptPubKey.begin() + 1, scriptPubKey.begin() + 34 );
   }
   return scriptPubKey;
}

bool bitcoin_transaction::operator!=( const bitcoin_transaction& bt ) const
{
   if( this->nVersion != bt.nVersion ||
      this->vin != bt.vin ||
      this->vout != bt.vout ||
      this->nLockTime != bt.nLockTime )
   {
      return true;
   }
   return false;
}

fc::sha256 bitcoin_transaction::get_hash() const
{
   const auto bytes = pack( *this, true) ;
   const auto hash = fc::sha256::hash( fc::sha256::hash(bytes.data(), bytes.size()) );
   std::reverse( hash.data(), hash.data() + hash.data_size() );
   return hash;
}

fc::sha256 bitcoin_transaction::get_txid() const
{
   const auto bytes = pack( *this, false );
   const auto hash = fc::sha256::hash( fc::sha256::hash(bytes.data(), bytes.size()) );
   std::reverse( hash.data(), hash.data() + hash.data_size() );
   return hash;
}

size_t bitcoin_transaction::get_vsize() const
{
   static const auto witness_scale_factor = 4;

   fc::datastream<size_t> no_wit_ds;
   pack( no_wit_ds, *this, false );

   fc::datastream<size_t> wit_ds;
   pack( wit_ds, *this, true );

   const size_t weight = no_wit_ds.tellp() * ( witness_scale_factor - 1 ) + wit_ds.tellp();
   const size_t vsize = ( weight + witness_scale_factor - 1 ) / witness_scale_factor;

   return vsize;
}

void bitcoin_transaction_builder::set_version( int32_t version )
{
    tx.nVersion = version;
}

void bitcoin_transaction_builder::set_locktime(uint32_t lock_time)
{
    tx.nLockTime = lock_time;
}

void bitcoin_transaction_builder::add_in( payment_type type, const fc::sha256& txid, uint32_t n_out, const bytes& script_code, bool front, uint32_t sequence )
{
   out_point prevout;
   prevout.hash = txid;
   prevout.n = n_out;

   tx_in txin;
   txin.prevout = prevout;
   txin.nSequence = sequence;

   add_in( type, txin, script_code, front );
}

void bitcoin_transaction_builder::add_in( payment_type type, tx_in txin, const bytes& script_code, bool front )
{
   switch ( type ) {
      case payment_type::P2SH_WPKH:
      case payment_type::P2SH_WSH:
         FC_ASSERT( script_code != bytes() );
         txin.scriptSig = script_code;
         break;
      default:{
         if( txin.prevout.hash == fc::sha256("0000000000000000000000000000000000000000000000000000000000000000") ) { //coinbase
            FC_ASSERT( script_code != bytes() );
            txin.scriptSig = script_code;
         }
         break;
      }
   }

   if( front ) {
      tx.vin.insert( tx.vin.begin(), txin );
   } else {
      tx.vin.push_back( txin );
   }
}

void bitcoin_transaction_builder::add_out( payment_type type, int64_t amount, const std::string& base58_address, bool front )
{
   // TODO: add checks
   const auto address_bytes = fc::from_base58(base58_address);
   add_out( type, amount, bytes( address_bytes.begin() + 1, address_bytes.begin() + 1 + 20 ), front );
}

void bitcoin_transaction_builder::add_out( payment_type type, int64_t amount, const fc::ecc::public_key_data& pubkey, bool front )
{
   FC_ASSERT( is_payment_to_pubkey( type ) ); 

   if ( type == payment_type::P2PK ) {
      const auto pubkey_bytes = bytes( pubkey.begin(), pubkey.begin() + pubkey.size() );
      add_out( type, amount, pubkey_bytes, front );
   } else {
      const auto hash256 = fc::sha256::hash( pubkey.begin(), pubkey.size() );
      const auto hash160 = fc::ripemd160::hash( hash256.data(), hash256.data_size() );
      add_out( type, amount, bytes( hash160.data(), hash160.data() + hash160.data_size() ), front );
   }
}

void bitcoin_transaction_builder::add_out( payment_type type, int64_t amount, const bytes& script_code, bool front )
{
   tx_out out;
   out.value = amount;
   out.scriptPubKey = get_script_pubkey( type, script_code );

   if( front ) {
      tx.vout.insert( tx.vout.begin(), out );
   } else {
      tx.vout.push_back( out );
   }
}

void bitcoin_transaction_builder::add_out_all_type( const uint64_t& amount, const bitcoin_address& address, bool front )
{
   switch( address.get_type() ) {
      case payment_type::P2PK: {
         bytes raw_address( address.get_raw_address() );
         fc::ecc::public_key_data public_key;
         std::copy( raw_address.begin(), raw_address.end(), public_key.begin() );
         add_out( address.get_type(), amount, public_key, front );
         break;
      }
      case payment_type::P2PKH:
      case payment_type::P2SH:
      case payment_type::P2SH_WPKH:
      case payment_type::P2SH_WSH: {
         add_out( address.get_type(), amount, address.get_address(), front );
         break;
      }
      case payment_type::P2WPKH:
      case payment_type::P2WSH: {
         add_out( address.get_type(), amount, address.get_raw_address(), front );
         break;
      }
   }
}

inline bool bitcoin_transaction_builder::is_payment_to_pubkey( payment_type type ) 
{
   switch ( type ) {
   case payment_type::P2PK:
   case payment_type::P2PKH:
   case payment_type::P2WPKH:
   case payment_type::P2SH_WPKH:
      return true;
   default:
      return false;
   }
}

bytes bitcoin_transaction_builder::get_script_pubkey( payment_type type, const bytes& script_code )
{
    switch ( type ) {
    case payment_type::NULLDATA:
       return script_builder() << op::RETURN << script_code;
    case payment_type::P2PK:
       return script_builder() << script_code << op::CHECKSIG;
    case payment_type::P2PKH:
       return script_builder() << op::DUP << op::HASH160 << script_code << op::EQUALVERIFY << op::CHECKSIG;
    case payment_type::P2SH:
    case payment_type::P2SH_WPKH:
    case payment_type::P2SH_WSH:
       return script_builder() << op::HASH160 << script_code << op::EQUAL;
    case payment_type::P2WPKH:
    case payment_type::P2WSH:
       return script_builder() << op::_0 << script_code;
    default:
       return script_code;
    }
}

}
