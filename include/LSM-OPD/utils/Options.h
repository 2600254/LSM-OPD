#pragma once

#include <string>
#include "types.h"

namespace LSMOPD
{
#ifndef OPTION
#define OPTION

struct Options
{
	std::string STORAGE_DIR = "./output/db_storage";
	size_t MEM_TABLE_MAX_SIZE = 3354624;
	size_t READ_BUFFER_SIZE = 1024 * 1024;
	size_t WRITE_BUFFER_SIZE = 1024 * 1024;
	size_t MAX_FILE_READER_CACHE_SIZE = 0;
	size_t MAX_WORKER_THREAD = 16;
	double FALSE_POSITIVE = 0.01;
	size_t MAX_LEVEL = 5;
	size_t LEVEL_SIZE_RITIO = 10;
	size_t MAX_BLOCK_SIZE = 65536;
	size_t ZERO_LEVEL_FILES = 2;
	size_t NUM_OF_HIGH_COMPACTION_THREAD = 4;
	size_t NUM_OF_LOW_COMPACTION_THREAD = 4;
	size_t MAX_MEMTABLE_NUM = 2;
	inline static constexpr size_t KEY_SIZE = 16;
};


#endif // OPTION

}