#pragma once

#include <sidechain/types.hpp>

namespace sidechain {

inline void write_compact_size(bytes& vec, size_t size) {
   bytes sb;
   sb.reserve(2);
   if (size < 253) {
      sb.insert(sb.end(), static_cast<uint8_t>(size));
   } else if (size <= std::numeric_limits<unsigned short>::max()) {
      uint16_t tmp = htole16(static_cast<uint16_t>(size));
      sb.insert(sb.end(), static_cast<uint8_t>(253));
      sb.insert(sb.end(), reinterpret_cast<const char*>(tmp), reinterpret_cast<const char*>(tmp) + sizeof(tmp));
   } else if (size <= std::numeric_limits<unsigned int>::max()) {
      uint32_t tmp = htole32(static_cast<uint32_t>(size));
      sb.insert(sb.end(), static_cast<uint8_t>(254));
      sb.insert(sb.end(), reinterpret_cast<const char*>(tmp), reinterpret_cast<const char*>(tmp) + sizeof(tmp));
   } else {
      uint64_t tmp = htole64(static_cast<uint64_t>(size));
      sb.insert(sb.end(), static_cast<uint8_t>(255));
      sb.insert(sb.end(), reinterpret_cast<const char*>(tmp), reinterpret_cast<const char*>(tmp) + sizeof(tmp));
   }
   vec.insert(vec.end(), sb.begin(), sb.end());
}

template<typename Stream>
inline void pack_compact_size(Stream& s, size_t size) {
   if (size < 253) {
      fc::raw::pack(s, static_cast<uint8_t>(size));
   } else if (size <= std::numeric_limits<unsigned short>::max()) {
      fc::raw::pack(s, static_cast<uint8_t>(253));
      fc::raw::pack(s, htole16(static_cast<uint16_t>(size)));
   } else if (size <= std::numeric_limits<unsigned int>::max()) {
      fc::raw::pack(s, static_cast<uint8_t>(254));
      fc::raw::pack(s, htole32(static_cast<uint32_t>(size)));
   } else {
      fc::raw::pack(s, static_cast<uint8_t>(255));
      fc::raw::pack(s, htole64(static_cast<uint64_t>(size)));
   }
}

template<typename Stream>
inline uint64_t unpack_compact_size(Stream& s) {
   uint8_t size;
   uint64_t size_ret;

   fc::raw::unpack(s, size);

   if (size < 253) {
      size_ret = size;
   } else if (size == 253) {
      uint16_t tmp;
      fc::raw::unpack(s, tmp);
      size_ret = le16toh(tmp);
      if (size_ret < 253)
         FC_THROW_EXCEPTION(fc::parse_error_exception, "non-canonical unpack_compact_size()");
   } else if (size == 254) {
      uint32_t tmp;
      fc::raw::unpack(s, tmp);
      size_ret = le32toh(tmp);
      if (size_ret < 0x10000u)
         FC_THROW_EXCEPTION(fc::parse_error_exception, "non-canonical unpack_compact_size()");
   } else {
      uint32_t tmp;
      fc::raw::unpack(s, tmp);
      size_ret = le64toh(tmp);
      if (size_ret < 0x100000000ULL)
         FC_THROW_EXCEPTION(fc::parse_error_exception, "non-canonical unpack_compact_size()");
   }

   if (size_ret > 0x08000000)
      FC_THROW_EXCEPTION(fc::parse_error_exception, "unpack_compact_size(): size too large");

   return size_ret;
}

template<typename Stream>
inline void pack(Stream& s, const std::vector<char>& v) {
   pack_compact_size(s, v.size());
   if (!v.empty())
      s.write(v.data(), v.size());
}

template<typename Stream>
inline void unpack(Stream& s, std::vector<char>& v) {
   const auto size = unpack_compact_size(s);
   v.resize(size);
   if (size)
      s.read(v.data(), size);
}

template<typename Stream, typename T>
inline void pack(Stream& s, const T& val) {
   fc::raw::pack(s, val);
}

template<typename Stream, typename T>
inline void unpack(Stream& s, T& val) {
   fc::raw::unpack(s, val);
}

}
