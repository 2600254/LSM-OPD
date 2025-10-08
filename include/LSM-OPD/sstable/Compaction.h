#pragma once

#include <vector>
#include "LSM-OPD/memory/TupleEntry.h"
#include "LSM-OPD/sstable/RelFileMetaData.h"

namespace LSMOPD
{
	template<typename Key_t>
	struct RelCompaction {
		std::vector<RelFileMetaData<std::string>*> file_list;
		idx_t target_level;
		idx_t file_id;
		Key_t key_min;
		std::shared_ptr< relMemTable > relPersistence;
	};
}