#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <math.h>

namespace LSMOPD
{
	using idx_t = uint32_t;
	using time_t = uint64_t;
	using filter_t = uint64_t;
	const unsigned long singel_filter_allocation_size = sizeof(size_t) + sizeof(idx_t);
	const idx_t NONEINDEX = UINT32_MAX;
	//constexpr static auto TIMEOUT = std::chrono::milliseconds(1);
	const time_t MAXTIME = UINT64_MAX;

	using tuple_t = uint32_t;
	using tuple_property_t = double_t;
}