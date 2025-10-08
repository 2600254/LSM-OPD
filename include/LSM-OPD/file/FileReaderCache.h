#pragma once

#include <atomic>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>
#include "FileReader.h"
#include "LSM-OPD/sstable/RelFileMetaData.h"
#include "LSM-OPD/utils/Options.h"

namespace LSMOPD
{
	class FileReaderCache
	{
	public:
		FileReaderCache(idx_t _max_size, std::string _prefix);
		FileReader* find(RelFileMetaData<std::string>* file_data);
		void deletecache(RelFileMetaData<std::string>* file_data);
	private:
		std::vector<RelFileMetaData<std::string>*> cache;
		std::vector<std::atomic<bool>> cache_deleting;
		std::atomic<idx_t> size = 0;
		idx_t max_size;
		std::string prefix;
		bool no_cache = false;
	};
}