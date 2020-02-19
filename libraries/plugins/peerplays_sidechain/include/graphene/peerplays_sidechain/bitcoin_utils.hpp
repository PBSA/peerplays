#pragma once
#include <graphene/peerplays_sidechain/defs.hpp>
#include <fc/optional.hpp>

namespace graphene { namespace peerplays_sidechain {

enum bitcoin_network {
   mainnet,
   testnet,
   regtest
};

bytes generate_redeem_script(std::vector<std::pair<fc::ecc::public_key, int> > key_data);
std::string p2wsh_address_from_redeem_script(const bytes& script, bitcoin_network network = mainnet);
bytes lock_script_for_redeem_script(const bytes& script);


std::vector<bytes> signatures_for_raw_transaction(const bytes& unsigned_tx,
                                                 std::vector<uint64_t> in_amounts,
                                                 const bytes& redeem_script,
                                                 const fc::ecc::private_key& priv_key);

/*
 * unsigned_tx - tx,  all inputs of which are spends of the PW P2SH address
 * returns signed transaction
 */
bytes sign_pw_transfer_transaction(const bytes& unsigned_tx,
                                   std::vector<uint64_t> in_amounts,
                                   const bytes& redeem_script,
                                   const std::vector<fc::optional<fc::ecc::private_key>>& priv_keys);

///
////// \brief Adds dummy signatures instead of real signatures
////// \param unsigned_tx
////// \param redeem_script
////// \param key_count
////// \return can be used as partially signed tx
bytes add_dummy_signatures_for_pw_transfer(const bytes& unsigned_tx,
                                           const bytes& redeem_script,
                                           unsigned int key_count);

///
/// \brief replaces dummy sgnatures in partially signed tx with real tx
/// \param partially_signed_tx
/// \param in_amounts
/// \param priv_key
/// \param key_idx
/// \return
///
bytes partially_sign_pw_transfer_transaction(const bytes& partially_signed_tx,
                                   std::vector<uint64_t> in_amounts,
                                   const fc::ecc::private_key& priv_key,
                                   unsigned int key_idx);

///
/// \brief Creates ready to publish bitcoin transaction from unsigned tx and
///        full set of the signatures. This is alternative way to create tx
///        with partially_sign_pw_transfer_transaction
/// \param unsigned_tx
/// \param signatures
/// \param redeem_script
/// \return
///
bytes add_signatures_to_unsigned_tx(const bytes& unsigned_tx,
                                    const std::vector<std::vector<bytes> >& signatures,
                                    const bytes& redeem_script);

struct btc_outpoint
{
   fc::uint256 hash;
   uint32_t n;

   void to_bytes(bytes& stream) const;
   size_t fill_from_bytes(const bytes& data, size_t pos = 0);
};

struct btc_in
{
   btc_outpoint prevout;
   bytes scriptSig;
   uint32_t nSequence;
   std::vector<bytes> scriptWitness;

   void to_bytes(bytes& stream) const;
   size_t fill_from_bytes(const bytes& data, size_t pos = 0);
};

struct btc_out
{
   int64_t nValue;
   bytes scriptPubKey;

   void to_bytes(bytes& stream) const;
   size_t fill_from_bytes(const bytes& data, size_t pos = 0);
};

struct btc_tx
{
   std::vector<btc_in> vin;
   std::vector<btc_out> vout;
   int32_t nVersion;
   uint32_t nLockTime;
   bool hasWitness;

   void to_bytes(bytes& stream) const;
   size_t fill_from_bytes(const bytes& data, size_t pos = 0);
};

}}

