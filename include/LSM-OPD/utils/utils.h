#pragma once

#include <bit>
#include <math.h>
#include <memory>
#include <string>
#include "Options.h"
#include "types.h"
#include <cstring>
namespace LSMOPD
{
	namespace util
	{
		template<typename T>
		inline void PutFixed(std::string& dst, T val)
		{
			if constexpr (std::is_same_v<T, std::string>) {
				size_t copy_len = Options::KEY_SIZE;
				val.resize(copy_len);
				dst.append(val);
			}
			else {
				uint32_t n = sizeof(val);
				dst.append((char*)(&val), n);
			}
		}
		template<typename T>
		inline void DecodeFixed(const char* data, T& val)
		{
			if constexpr (std::is_same_v<T, std::string>) {
				val = std::string(data, Options::KEY_SIZE);
				
			}
			else {
				val = *(reinterpret_cast<const T*>(data));
			}

		}
		inline uint32_t murmur_hash2(const void* key, uint32_t len) {
			// 'm' and 'r' are mixing constants generated offline.
			// They're not really 'magic', they just happen to work well.
			static constexpr uint32_t seed = 0xbc451d34;
			static constexpr uint32_t m = 0x5bd1e995;
			static constexpr uint32_t r = 24;

			// Initialize the hash to a 'random' value

			uint32_t h = seed ^ len;

			// Mix 4 bytes at a time into the hash

			const uint8_t* data = (const unsigned char*)key;

			while (len >= 4) {
				uint32_t k = *(uint32_t*)data;

				k *= m;
				k ^= k >> r;
				k *= m;

				h *= m;
				h ^= k;

				data += 4;
				len -= 4;
			}

			// Handle the last few bytes of the input array

			switch (len) {
			case 3:
				h ^= data[2] << 16;
			case 2:
				h ^= data[1] << 8;
			case 1:
				h ^= data[0];
				h *= m;
			};

			// Do a few final mixes of the hash to ensure the last few
			// bytes are well-incorporated.

			h ^= h >> 13;
			h *= m;
			h ^= h >> 15;

			return h;
		}

		template<typename A, typename B>
		inline A fast_pow(A a, B b)
		{
			A ans = 1;
			for (; b; b >>= 1, a = a * a)
				if (b & 1)
					ans *= a;
			return ans;
		}
		inline size_t highbit(size_t x)
		{
			return std::bit_width(x) - 1;
		}
		template<typename T>
		inline T GetDecodeFixed(const char* data)
		{
			if constexpr (std::is_same_v<T, std::string>) {
				return std::string(data, Options::KEY_SIZE);
			}
			else
				return *(reinterpret_cast<const T*>(data));
		}
	}
}