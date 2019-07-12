#include <sidechain/sign_bitcoin_transaction.hpp>
#include <sidechain/serialize.hpp>

namespace sidechain {

fc::sha256 get_signature_hash( const bitcoin_transaction& tx, const bytes& scriptCode, int64_t amount,
                               size_t in_index, int hash_type, bool is_witness )
{
   fc::datastream<size_t> ps;
   if ( is_witness )
      pack_tx_witness_signature( ps, scriptCode, tx, in_index, amount, hash_type );
   else
      pack_tx_signature( ps, scriptCode, tx, in_index, hash_type );

   std::vector<char> vec( ps.tellp() );
   if ( !vec.empty() ) {
      fc::datastream<char*> ds( vec.data(), vec.size() );
      if ( is_witness )
         pack_tx_witness_signature( ds, scriptCode, tx, in_index, amount, hash_type );
      else
         pack_tx_signature( ds, scriptCode, tx, in_index, hash_type );
   }

   return fc::sha256::hash( fc::sha256::hash( vec.data(), vec.size() ) );
}

std::vector<char> privkey_sign( const bytes& privkey, const fc::sha256 &hash, const secp256k1_context_t* context_sign )
{
   bytes sig;
   sig.resize( 72 );
   int sig_len = sig.size();

   FC_ASSERT( secp256k1_ecdsa_sign(
      context_sign,
      reinterpret_cast<unsigned char*>( hash.data() ),
      reinterpret_cast<unsigned char*>( sig.data() ),
      &sig_len,
      reinterpret_cast<const unsigned char*>( privkey.data() ),
      secp256k1_nonce_function_rfc6979,
      nullptr ) ); // TODO: replace assert with exception

   sig.resize( sig_len );

   return sig;
}

std::vector<bytes> sign_witness_transaction_part( const bitcoin_transaction& tx, const std::vector<bytes>& redeem_scripts,
                                                  const std::vector<uint64_t>& amounts, const bytes& privkey,
                                                  const secp256k1_context_t* context_sign, int hash_type )
{
   FC_ASSERT( tx.vin.size() == redeem_scripts.size() && tx.vin.size() == amounts.size() );
   FC_ASSERT( !privkey.empty() );
    
   std::vector< bytes > signatures;
   for( size_t i = 0; i < tx.vin.size(); i++ ) {
      const auto sighash = get_signature_hash( tx, redeem_scripts[i], static_cast<int64_t>( amounts[i] ), i, hash_type, true );
      auto sig = privkey_sign( privkey, sighash, context_sign );
      sig.push_back( static_cast<uint8_t>( hash_type ) );

      signatures.push_back( sig );
   }
   return signatures;
}

void sign_witness_transaction_finalize( bitcoin_transaction& tx, const std::vector<bytes>& redeem_scripts )
{
   FC_ASSERT( tx.vin.size() == redeem_scripts.size() );
    
   for( size_t i = 0; i < tx.vin.size(); i++ ) {
      tx.vin[i].scriptWitness.insert( tx.vin[i].scriptWitness.begin(), bytes() ); // Bitcoin workaround CHECKMULTISIG bug
      tx.vin[i].scriptWitness.push_back( redeem_scripts[i] );
   }
}

bool verify_sig( const bytes& sig, const bytes& pubkey, const bytes& msg, const secp256k1_context_t* context )
{
   std::vector<unsigned char> sig_temp( sig.begin(), sig.end() );
   std::vector<unsigned char> pubkey_temp( pubkey.begin(), pubkey.end() );
   std::vector<unsigned char> msg_temp( msg.begin(), msg.end() );

   int result = secp256k1_ecdsa_verify( context, msg_temp.data(), sig_temp.data(), sig_temp.size(), pubkey_temp.data(), pubkey_temp.size() );
   return result == 1;
}

std::vector<std::vector<bytes>> sort_sigs( const bitcoin_transaction& tx, const std::vector<bytes>& redeem_scripts,
                                           const std::vector<uint64_t>& amounts, const secp256k1_context_t* context )
{
   FC_ASSERT( redeem_scripts.size() == amounts.size() );

   using data = std::pair<size_t, bytes>;
   struct comp {
      bool operator() (const data& lhs, const data& rhs) const { return lhs.first < rhs.first; }
   };

   std::vector<std::vector<bytes>> new_stacks;

   for( size_t i = 0; i < redeem_scripts.size(); i++ ) {
      const std::vector<bytes>& keys = get_pubkey_from_redeemScript( redeem_scripts[i] );
      const auto& sighash = get_signature_hash( tx, redeem_scripts[i], static_cast<int64_t>( amounts[i] ), i, 1, true ).str();
      bytes sighash_temp( parse_hex( sighash ) );

      std::vector<bytes> stack( tx.vin[i].scriptWitness );
      std::vector<bool> marker( tx.vin[i].scriptWitness.size(), false );
      std::set<data, comp> sigs;

      for( size_t j = 0; j < keys.size(); j++ ) {     
          for( size_t l = 0; l < stack.size(); l++ ) {
             if( !verify_sig( stack[l], keys[j], sighash_temp, context ) || marker[l] )
                continue;
             sigs.insert(std::make_pair(j, stack[l]));
             marker[l] = true;
             break;
          }
      }

      std::vector<bytes> temp_sig;
      for( auto s : sigs ) {
         temp_sig.push_back( s.second );
      }
      new_stacks.push_back( temp_sig );
   }
   return new_stacks;
}

}
