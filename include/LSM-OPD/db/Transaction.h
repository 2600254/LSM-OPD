#pragma once

#include <string>
#include <unordered_map>
#include "DB.h"
#include "LSM-OPD/common/ScanIter.h"
#include "LSM-OPD/common/tuple.h"
#include "LSM-OPD/sstable/RelVersion.h"
#include "LSM-OPD/sstable/BloomFilter.h"
#include "LSM-OPD/memory/RowMemoryManager.h"
#include "LSM-OPD/memory/RowGroup.h"
#include "LSM-OPD/memory/AnswerMerger.h" 
#include "LSM-OPD/profiler/profiler.h"
#include "LSM-OPD/profiler/OperatorProfilerContext.h"

namespace LSMOPD
{
	class Transaction
	{
	public:
		Transaction() = delete;
		Transaction& operator=(const Transaction&) = delete;
		Transaction(const Transaction& txn) = delete;

		Transaction(time_t _write_epoch, time_t _read_epoch,
			DB* db, RelVersion* _version, time_t pos = -1);

		Transaction(Transaction&& txn);
		~Transaction();

		// OLTP operation
        void PutTuple(const Tuple &tuple, const tp_key &key);
        void DelTuple(const tp_key &key);
        Tuple GetTuple(const tp_key &key);


        // OLAP operation
		void GetTuplesFromRange(idx_t col_id, const std::string &left_bound, const std::string &right_bound, std::vector<Tuple> &answer) {
			START_OPERATOR_PROFILER();

			AnswerMerger am;
			// add in-memory data into am here;
			auto vf = CreateValueFilterFunction(col_id, left_bound, right_bound);
			db->RowMemtable->FilterByValueRange(read_epoch, vf, am, rel_version);

			auto& files = rel_version->FileIndex;

			for (auto& cur_level : files) {
				for (auto& cur_file : cur_level) {
					RowGroup cur_row_group(db, cur_file);
					cur_row_group.GetKeyData();
					cur_row_group.GetAllColData();
					// minus 1 because the first column is key
					cur_row_group.ApplyRangeFilter(col_id - 1, left_bound, right_bound, am);
				}
			}
			answer.reserve(am.answers.size());
			for (const auto& x : am.answers) {
				answer.emplace_back(std::move(x.second));
			}
			END_OPERATOR_PROFILER("GetTuplesFromRange");
		}

        void ScanTuples();
		ScanIter GetIter(std::string key);

		std::vector<std::unique_ptr<Tuple>> ScanKTuples(idx_t k, std::string key);

	private:
		time_t write_epoch;
		time_t read_epoch;
		DB* db;
		size_t time_pos;

		RelVersion* rel_version;
#ifdef RUN_PROFILER
		ThreadProfiler profiler;
#endif	
		bool valid = true;
	};
}
