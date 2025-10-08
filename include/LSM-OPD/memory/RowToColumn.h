#pragma once
#include "LSM-OPD/memory/RowMemoryManager.h"
#include "LSM-OPD/memory/TupleEntry.h"
#include "LSM-OPD/utils/ConcurrentArray.h"
#include <string>
#include <vector>

namespace LSMOPD {

	// a class that use to temperary store the data from row format
	// technically don't have dictionary
	class TempColumn
	{
	public:
		TempColumn(relMemTable* row, size_t _size, size_t _column_num);
		~TempColumn();
		std::string* GetColumn(size_t column_id) {
			return data[column_id];
		}

	private:
		std::string** data;
		size_t size;
		size_t column_num;

	};
} // namespace LSMOPD