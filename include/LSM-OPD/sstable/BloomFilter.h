#pragma once

#include <cmath>
#include <vector>
#include <string>
#include "LSM-OPD/utils/types.h"
#include "LSM-OPD/utils/utils.h"

namespace LSMOPD
{
	class BloomFilter
	{
	public:
		BloomFilter() = default;
		~BloomFilter() = default;
		BloomFilter(idx_t keys_num, double false_positive);
		std::string& data();
		void insert(const std::string& dst);
		void create_from_data(int32_t func_num, std::string& bits);
		bool exists(const std::string& dst);
		int32_t get_func_num()const { return hash_func_num; }
	private:
		int32_t bits_per_key = 0;
		int32_t hash_func_num = 0;
		std::string bits_array;
	};

	template<typename key_type>
	idx_t transferKeyToHash(key_type key) {
		return 1;
	}
}