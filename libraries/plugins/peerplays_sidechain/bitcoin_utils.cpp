#include <graphene/peerplays_sidechain/bitcoin_utils.hpp>
#include <fc/io/raw.hpp>
#include <fc/crypto/base58.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/sha256.hpp>
#include <secp256k1.h>

namespace graphene { namespace peerplays_sidechain {

static const unsigned char OP_0 = 0x00;
static const unsigned char OP_1 = 0x51;
static const unsigned char OP_2 = 0x52;
static const unsigned char OP_3 = 0x53;
static const unsigned char OP_4 = 0x54;
static const unsigned char OP_5 = 0x55;
static const unsigned char OP_6 = 0x56;
static const unsigned char OP_7 = 0x57;
static const unsigned char OP_8 = 0x58;
static const unsigned char OP_9 = 0x59;
static const unsigned char OP_10 = 0x5a;
static const unsigned char OP_11 = 0x5b;
static const unsigned char OP_12 = 0x5c;
static const unsigned char OP_13 = 0x5d;
static const unsigned char OP_14 = 0x5e;
static const unsigned char OP_15 = 0x5f;
static const unsigned char OP_16 = 0x60;

static const unsigned char OP_IF = 0x63;
static const unsigned char OP_ENDIF = 0x68;
static const unsigned char OP_SWAP = 0x7c;
static const unsigned char OP_EQUAL = 0x87;
static const unsigned char OP_ADD = 0x93;
static const unsigned char OP_GREATERTHAN = 0xa0;
static const unsigned char OP_HASH160 = 0xa9;
static const unsigned char OP_CHECKSIG = 0xac;

class WriteBytesStream{
public:
   WriteBytesStream(bytes& buffer) : storage_(buffer) {}

   void write(const unsigned char* d, size_t s) {
      storage_.insert(storage_.end(), d, d + s);
   }

   bool put(unsigned char c) {
      storage_.push_back(c);
      return true;
   }

   void writedata8(uint8_t obj)
   {
       write((unsigned char*)&obj, 1);
   }

   void writedata16(uint16_t obj)
   {
       obj = htole16(obj);
       write((unsigned char*)&obj, 2);
   }

   void writedata32(uint32_t obj)
   {
       obj = htole32(obj);
       write((unsigned char*)&obj, 4);
   }

   void writedata64(uint64_t obj)
   {
       obj = htole64(obj);
       write((unsigned char*)&obj, 8);
   }

   void write_compact_int(uint64_t val)
   {
      if (val < 253)
      {
          writedata8(val);
      }
      else if (val <= std::numeric_limits<unsigned short>::max())
      {
          writedata8(253);
          writedata16(val);
      }
      else if (val <= std::numeric_limits<unsigned int>::max())
      {
          writedata8(254);
          writedata32(val);
      }
      else
      {
          writedata8(255);
          writedata64(val);
      }
   }

   void writedata(const bytes& data)
   {
      write_compact_int(data.size());
      write(&data[0], data.size());
   }
private:
   bytes& storage_;
};

class ReadBytesStream{
public:
   ReadBytesStream(const bytes& buffer, size_t pos = 0) : storage_(buffer), pos_(pos), end_(buffer.size()) {}

   size_t current_pos() const { return pos_; }
   void set_pos(size_t pos)
   {
      if(pos > end_)
         FC_THROW("Invalid position in BTC tx buffer");
      pos_ = pos;
   }

   inline bool read( unsigned char* d, size_t s ) {
     if( end_ - pos_ >= s ) {
       memcpy( d, &storage_[pos_], s );
       pos_ += s;
       return true;
     }
     FC_THROW( "invalid bitcoin tx buffer" );
   }

   inline bool get( unsigned char& c )
   {
     if( pos_ < end_ ) {
       c = storage_[pos_++];
       return true;
     }
     FC_THROW( "invalid bitcoin tx buffer" );
   }

   uint8_t readdata8()
   {
       uint8_t obj;
       read((unsigned char*)&obj, 1);
       return obj;
   }
   uint16_t readdata16()
   {
       uint16_t obj;
       read((unsigned char*)&obj, 2);
       return le16toh(obj);
   }
   uint32_t readdata32()
   {
       uint32_t obj;
       read((unsigned char*)&obj, 4);
       return le32toh(obj);
   }
   uint64_t readdata64()
   {
       uint64_t obj;
       read((unsigned char*)&obj, 8);
       return le64toh(obj);
   }

   uint64_t read_compact_int()
   {
       uint8_t size = readdata8();
       uint64_t ret = 0;
       if (size < 253)
       {
           ret = size;
       }
       else if (size == 253)
       {
           ret = readdata16();
           if (ret < 253)
               FC_THROW("non-canonical ReadCompactSize()");
       }
       else if (size == 254)
       {
           ret = readdata32();
           if (ret < 0x10000u)
               FC_THROW("non-canonical ReadCompactSize()");
       }
       else
       {
           ret = readdata64();
           if (ret < 0x100000000ULL)
               FC_THROW("non-canonical ReadCompactSize()");
       }
       if (ret > (uint64_t)0x02000000)
           FC_THROW("ReadCompactSize(): size too large");
       return ret;
   }

   void readdata(bytes& data)
   {
      size_t s = read_compact_int();
      data.clear();
      data.resize(s);
      read(&data[0], s);
   }

private:
   const bytes& storage_;
   size_t pos_;
   size_t end_;
};

void btc_outpoint::to_bytes(bytes& stream) const
{
   WriteBytesStream str(stream);
   // TODO: write size?
   str.write((unsigned char*)hash.data(), hash.data_size());
   str.writedata32(n);
}

size_t btc_outpoint::fill_from_bytes(const bytes& data, size_t pos)
{
   ReadBytesStream str(data, pos);
   // TODO: read size?
   str.read((unsigned char*)hash.data(), hash.data_size());
   n = str.readdata32();
   return str.current_pos();
}

void btc_in::to_bytes(bytes& stream) const
{
   prevout.to_bytes(stream);
   WriteBytesStream str(stream);
   str.writedata(scriptSig);
   str.writedata32(nSequence);
}

size_t btc_in::fill_from_bytes(const bytes& data, size_t pos)
{
   pos = prevout.fill_from_bytes(data, pos);
   ReadBytesStream str(data, pos);
   str.readdata(scriptSig);
   nSequence = str.readdata32();
   return str.current_pos();
}

void btc_out::to_bytes(bytes& stream) const
{
   WriteBytesStream str(stream);
   str.writedata64(nValue);
   str.writedata(scriptPubKey);
}

size_t btc_out::fill_from_bytes(const bytes& data, size_t pos)
{
   ReadBytesStream str(data, pos);
   nValue = str.readdata64();
   str.readdata(scriptPubKey);
   return str.current_pos();
}

void btc_tx::to_bytes(bytes& stream) const
{
   WriteBytesStream str(stream);
   str.writedata32(nVersion);
   if(hasWitness)
   {
      // write dummy inputs and flag
      str.write_compact_int(0);
      unsigned char flags = 1;
      str.put(flags);
   }
   str.write_compact_int(vin.size());
   for(const auto& in: vin)
      in.to_bytes(stream);
   str.write_compact_int(vout.size());
   for(const auto& out: vout)
      out.to_bytes(stream);
   if(hasWitness)
   {
      for(const auto& in: vin)
      {
         str.write_compact_int(in.scriptWitness.size());
         for(const auto& stack_item: in.scriptWitness)
            str.writedata(stack_item);
      }
   }
   str.writedata32(nLockTime);
}

size_t btc_tx::fill_from_bytes(const bytes& data, size_t pos)
{
   ReadBytesStream ds( data, pos );
   nVersion = ds.readdata32();
   unsigned char flags = 0;
   vin.clear();
   vout.clear();
   hasWitness = false;
   /* Try to read the vin. In case the dummy is there, this will be read as an empty vector. */
   size_t vin_size = ds.read_compact_int();
   vin.resize(vin_size);
   pos = ds.current_pos();
   for(auto& in: vin)
      pos = in.fill_from_bytes(data, pos);
   ds.set_pos(pos);
   if (vin_size == 0) {
       /* We read a dummy or an empty vin. */
       ds.get(flags);
       if (flags != 0) {
          size_t vin_size = ds.read_compact_int();
          vin.resize(vin_size);
          pos = ds.current_pos();
          for(auto& in: vin)
             pos = in.fill_from_bytes(data, pos);
          ds.set_pos(pos);
          size_t vout_size = ds.read_compact_int();
          vout.resize(vout_size);
          pos = ds.current_pos();
          for(auto& out: vout)
             pos = out.fill_from_bytes(data, pos);
          ds.set_pos(pos);
          hasWitness = true;
       }
   } else {
       /* We read a non-empty vin. Assume a normal vout follows. */
      size_t vout_size = ds.read_compact_int();
      vout.resize(vout_size);
      pos = ds.current_pos();
      for(auto& out: vout)
         pos = out.fill_from_bytes(data, pos);
      ds.set_pos(pos);
   }
   if (hasWitness) {
      /* The witness flag is present, and we support witnesses. */
      for (auto& in: vin)
      {
         unsigned int size = ds.read_compact_int();
         in.scriptWitness.resize(size);
         for(auto& stack_item: in.scriptWitness)
            ds.readdata(stack_item);
      }
   }
   nLockTime = ds.readdata32();
   return ds.current_pos();
}


void add_data_to_script(bytes& script, const bytes& data)
{
   WriteBytesStream str(script);
   str.writedata(data);
}

void add_number_to_script(bytes& script, unsigned char data)
{
   WriteBytesStream str(script);
   if(data == 0)
      str.put(OP_0);
   else if(data == 1)
      str.put(OP_1);
   else if(data == 2)
      str.put(OP_2);
   else if(data == 3)
      str.put(OP_3);
   else if(data == 4)
      str.put(OP_4);
   else if(data == 5)
      str.put(OP_5);
   else if(data == 6)
      str.put(OP_6);
   else if(data == 7)
      str.put(OP_7);
   else if(data == 8)
      str.put(OP_8);
   else if(data == 9)
      str.put(OP_9);
   else if(data == 10)
      str.put(OP_10);
   else if(data == 11)
      str.put(OP_11);
   else if(data == 12)
      str.put(OP_12);
   else if(data == 13)
      str.put(OP_13);
   else if(data == 14)
      str.put(OP_14);
   else if(data == 15)
      str.put(OP_15);
   else if(data == 16)
      str.put(OP_16);
   else
      add_data_to_script(script, {data});
}

bytes generate_redeem_script(std::vector<std::pair<fc::ecc::public_key, int> > key_data)
{
   int total_weight = 0;
   bytes result;
   add_number_to_script(result, 0);
   for(auto& p: key_data)
   {
      total_weight += p.second;
      result.push_back(OP_SWAP);
      auto raw_data = p.first.serialize();
      add_data_to_script(result, bytes(raw_data.begin(), raw_data.begin() + raw_data.size()));
      result.push_back(OP_CHECKSIG);
      result.push_back(OP_IF);
      add_number_to_script(result, static_cast<unsigned char>(p.second));
      result.push_back(OP_ADD);
      result.push_back(OP_ENDIF);
   }
   int threshold_weight = 2 * total_weight / 3;
   add_number_to_script(result, static_cast<unsigned char>(threshold_weight));
   result.push_back(OP_GREATERTHAN);
   return result;
}

/** The Bech32 character set for encoding. */
const char* charset = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

/** Concatenate two byte arrays. */
bytes cat(bytes x, const bytes& y) {
    x.insert(x.end(), y.begin(), y.end());
    return x;
}

/** Expand a HRP for use in checksum computation. */
bytes expand_hrp(const std::string& hrp) {
    bytes ret;
    ret.resize(hrp.size() * 2 + 1);
    for (size_t i = 0; i < hrp.size(); ++i) {
        unsigned char c = hrp[i];
        ret[i] = c >> 5;
        ret[i + hrp.size() + 1] = c & 0x1f;
    }
    ret[hrp.size()] = 0;
    return ret;
}

/** Find the polynomial with value coefficients mod the generator as 30-bit. */
uint32_t polymod(const bytes& values) {
    uint32_t chk = 1;
    for (size_t i = 0; i < values.size(); ++i) {
        uint8_t top = chk >> 25;
        chk = (chk & 0x1ffffff) << 5 ^ values[i] ^
            (-((top >> 0) & 1) & 0x3b6a57b2UL) ^
            (-((top >> 1) & 1) & 0x26508e6dUL) ^
            (-((top >> 2) & 1) & 0x1ea119faUL) ^
            (-((top >> 3) & 1) & 0x3d4233ddUL) ^
            (-((top >> 4) & 1) & 0x2a1462b3UL);
    }
    return chk;
}

/** Create a checksum. */
bytes bech32_checksum(const std::string& hrp, const bytes& values) {
    bytes enc = cat(expand_hrp(hrp), values);
    enc.resize(enc.size() + 6);
    uint32_t mod = polymod(enc) ^ 1;
    bytes ret;
    ret.resize(6);
    for (size_t i = 0; i < 6; ++i) {
        ret[i] = (mod >> (5 * (5 - i))) & 31;
    }
    return ret;
}

/** Encode a Bech32 string. */
std::string bech32(const std::string& hrp, const bytes& values) {
    bytes checksum = bech32_checksum(hrp, values);
    bytes combined = cat(values, checksum);
    std::string ret = hrp + '1';
    ret.reserve(ret.size() + combined.size());
    for (size_t i = 0; i < combined.size(); ++i) {
        ret += charset[combined[i]];
    }
    return ret;
}

/** Convert from one power-of-2 number base to another. */
template<int frombits, int tobits, bool pad>
bool convertbits(bytes& out, const bytes& in) {
    int acc = 0;
    int bits = 0;
    const int maxv = (1 << tobits) - 1;
    const int max_acc = (1 << (frombits + tobits - 1)) - 1;
    for (size_t i = 0; i < in.size(); ++i) {
        int value = in[i];
        acc = ((acc << frombits) | value) & max_acc;
        bits += frombits;
        while (bits >= tobits) {
            bits -= tobits;
            out.push_back((acc >> bits) & maxv);
        }
    }
    if (pad) {
        if (bits) out.push_back((acc << (tobits - bits)) & maxv);
    } else if (bits >= frombits || ((acc << (tobits - bits)) & maxv)) {
        return false;
    }
    return true;
}

/** Encode a SegWit address. */
std::string segwit_addr_encode(const std::string& hrp, uint8_t witver, const bytes& witprog) {
    bytes enc;
    enc.push_back(witver);
    convertbits<8, 5, true>(enc, witprog);
    std::string ret = bech32(hrp, enc);
    return ret;
}

std::string p2wsh_address_from_redeem_script(const bytes& script, bitcoin_network network)
{
   // calc script hash
   fc::sha256 sh = fc::sha256::hash(reinterpret_cast<const char*>(&script[0]), script.size());
   bytes wp(sh.data(), sh.data() + sh.data_size());
   switch (network) {
   case(mainnet):
      return segwit_addr_encode("bc", 0, wp);
   case(testnet):
   case(regtest):
      return segwit_addr_encode("tb", 0, wp);
   default:
      FC_THROW("Unknown bitcoin network type");
   }
   FC_THROW("Unknown bitcoin network type");
}

bytes lock_script_for_redeem_script(const bytes &script)
{
   bytes result;
   result.push_back(OP_0);
   fc::sha256 h = fc::sha256::hash(reinterpret_cast<const char*>(&script[0]), script.size());
   bytes shash(h.data(), h.data() + h.data_size());
   add_data_to_script(result, shash);
   return result;
}

bytes hash_prevouts(const btc_tx& unsigned_tx)
{
   fc::sha256::encoder hasher;
   for(const auto& in: unsigned_tx.vin)
   {
      bytes data;
      in.prevout.to_bytes(data);
      hasher.write(reinterpret_cast<const char*>(&data[0]), data.size());
   }
   fc::sha256 res = fc::sha256::hash(hasher.result());
   return bytes(res.data(), res.data() + res.data_size());
}

bytes hash_sequence(const btc_tx& unsigned_tx)
{
   fc::sha256::encoder hasher;
   for(const auto& in: unsigned_tx.vin)
   {
      hasher.write(reinterpret_cast<const char*>(&in.nSequence), sizeof(in.nSequence));
   }
   fc::sha256 res = fc::sha256::hash(hasher.result());
   return bytes(res.data(), res.data() + res.data_size());
}

bytes hash_outputs(const btc_tx& unsigned_tx)
{
   fc::sha256::encoder hasher;
   for(const auto& out: unsigned_tx.vout)
   {
      bytes data;
      out.to_bytes(data);
      hasher.write(reinterpret_cast<const char*>(&data[0]), data.size());
   }
   fc::sha256 res = fc::sha256::hash(hasher.result());
   return bytes(res.data(), res.data() + res.data_size());
}

const secp256k1_context_t* btc_get_context() {
    static secp256k1_context_t* ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY | SECP256K1_CONTEXT_SIGN );
    return ctx;
}

bytes der_sign(const fc::ecc::private_key& priv_key, const fc::sha256& digest)
{
   fc::ecc::signature result;
   int size = result.size();
   FC_ASSERT( secp256k1_ecdsa_sign( btc_get_context(),
                                    (unsigned char*) digest.data(),
                                    (unsigned char*) result.begin(),
                                    &size,
                                    (unsigned char*) priv_key.get_secret().data(),
                                    secp256k1_nonce_function_rfc6979,
                                    nullptr));
   return bytes(result.begin(), result.begin() + size);
}

std::vector<bytes> signature_for_raw_transaction(const bytes& unsigned_tx,
                                                 std::vector<uint64_t> in_amounts,
                                                 const bytes& redeem_script,
                                                 const fc::ecc::private_key& priv_key)
{
   btc_tx tx;
   tx.fill_from_bytes(unsigned_tx);

   FC_ASSERT(tx.vin.size() == in_amounts.size(), "Incorrect input amounts data");

   std::vector<bytes> results;
   auto cur_amount = in_amounts.begin();
   // pre-calc reused values
   bytes hashPrevouts = hash_prevouts(tx);
   bytes hashSequence = hash_sequence(tx);
   bytes hashOutputs = hash_outputs(tx);
   // calc digest for every input according to BIP143
   // implement SIGHASH_ALL scheme
   for(const auto& in: tx.vin)
   {
      fc::sha256::encoder hasher;
      hasher.write(reinterpret_cast<const char*>(&tx.nVersion), sizeof(tx.nVersion));
      hasher.write(reinterpret_cast<const char*>(&hashPrevouts[0]), hashPrevouts.size());
      hasher.write(reinterpret_cast<const char*>(&hashSequence[0]), hashSequence.size());
      bytes data;
      in.prevout.to_bytes(data);
      hasher.write(reinterpret_cast<const char*>(&data[0]), data.size());
      bytes serializedScript;
      WriteBytesStream stream(serializedScript);
      stream.writedata(redeem_script);
      hasher.write(reinterpret_cast<const char*>(&serializedScript[0]), serializedScript.size());
      uint64_t amount = *cur_amount++;
      hasher.write(reinterpret_cast<const char*>(&amount), sizeof(amount));
      hasher.write(reinterpret_cast<const char*>(&in.nSequence), sizeof(in.nSequence));
      hasher.write(reinterpret_cast<const char*>(&hashOutputs[0]), hashOutputs.size());
      hasher.write(reinterpret_cast<const char*>(&tx.nLockTime), sizeof(tx.nLockTime));
      // add sigtype SIGHASH_ALL
      uint32_t sigtype = 1;
      hasher.write(reinterpret_cast<const char*>(&sigtype), sizeof(sigtype));

      fc::sha256 digest = fc::sha256::hash(hasher.result());
      //std::vector<char> res = priv_key.sign(digest);
      //bytes s_data(res.begin(), res.end());
      bytes s_data = der_sign(priv_key, digest);
      s_data.push_back(1);
      results.push_back(s_data);
   }
   return results;
}

bytes sign_pw_transfer_transaction(const bytes &unsigned_tx, std::vector<uint64_t> in_amounts, const bytes& redeem_script, const std::vector<fc::optional<fc::ecc::private_key> > &priv_keys)
{
   btc_tx tx;
   tx.fill_from_bytes(unsigned_tx);
   bytes dummy_data;
   for(auto key: priv_keys)
   {
      if(key)
      {
         std::vector<bytes> signatures = signature_for_raw_transaction(unsigned_tx, in_amounts, redeem_script, *key);
         FC_ASSERT(signatures.size() == tx.vin.size(), "Invalid signatures number");
         // push signatures in reverse order because script starts to check the top signature on the stack first
         for(unsigned int i = 0; i < tx.vin.size(); i++)
            tx.vin[i].scriptWitness.insert(tx.vin[i].scriptWitness.begin(), signatures[i]);
      }
      else
      {
         for(unsigned int i = 0; i < tx.vin.size(); i++)
            tx.vin[i].scriptWitness.push_back(dummy_data);
      }
   }

   for(auto& in: tx.vin)
   {
      in.scriptWitness.push_back(redeem_script);
   }

   tx.hasWitness = true;
   bytes ret;
   tx.to_bytes(ret);
   return ret;
}

bytes add_dummy_signatures_for_pw_transfer(const bytes& unsigned_tx,
                                           const bytes& redeem_script,
                                           unsigned int key_count)
{
   btc_tx tx;
   tx.fill_from_bytes(unsigned_tx);

   bytes dummy_data;
   for(auto& in: tx.vin)
   {
      for(unsigned i = 0; i < key_count; i++)
         in.scriptWitness.push_back(dummy_data);
      in.scriptWitness.push_back(redeem_script);
   }

   tx.hasWitness = true;
   bytes ret;
   tx.to_bytes(ret);
   return ret;
}

bytes partially_sign_pw_transfer_transaction(const bytes& partially_signed_tx,
                                   std::vector<uint64_t> in_amounts,
                                   const fc::ecc::private_key& priv_key,
                                   unsigned int key_idx)
{
   btc_tx tx;
   tx.fill_from_bytes(partially_signed_tx);
   FC_ASSERT(tx.vin.size() > 0);
   bytes redeem_script = tx.vin[0].scriptWitness.back();
   std::vector<bytes> signatures = signature_for_raw_transaction(partially_signed_tx, in_amounts, redeem_script, priv_key);
   FC_ASSERT(signatures.size() == tx.vin.size(), "Invalid signatures number");
   // push signatures in reverse order because script starts to check the top signature on the stack first
   unsigned witness_idx = tx.vin[0].scriptWitness.size() - 2 - key_idx;
   for(unsigned int i = 0; i < tx.vin.size(); i++)
      tx.vin[i].scriptWitness[witness_idx] = signatures[i];
   bytes ret;
   tx.to_bytes(ret);
   return ret;
}

}}
