#pragma once

#include <set>
#include <sys/resource.h>
#include "LSM-OPD/file/FileReaderCache.h"
#include "LSM-OPD/memory/RowMemoryManager.h"
#include "LSM-OPD/sstable/RelVersion.h"
#include "LSM-OPD/utils/ConcurrentList.h"
#include "LSM-OPD/profiler/ThreadProfilerContext.h"
#include "LSM-OPD/profiler/profiler.h"
#include "LSM-OPD/profiler/OperatorProfilerContext.h"

namespace LSMOPD
{
	class Transaction;
	class DB
	{
	public:
		DB() = delete;
		DB(const DB&) = delete;
		DB& operator=(const DB&) = delete;
		~DB();
		DB(std::shared_ptr<Options> _options, idx_t column_num);

		Transaction BeginRelTransaction();
		Transaction BeginReadOnlyRelTransaction();

		void ProgressRelVersion(VersionEdit* edit, time_t time,
			std::shared_ptr<relMemTable> size = nullptr, bool force_level = false);

		void StallWrite(int memtable);
		void ResumeWrite(int memtable);

		std::shared_ptr<Options> options;
		std::unique_ptr<FileReaderCache> ReaderCaches;

		std::unique_ptr<RelFileManager> relFiles;
		std::unique_ptr<rowMemoryManager> RowMemtable;

		bool Compacting() const {return working_compact_thread.load() > 0; };

	private:
		std::atomic<time_t> epoch_id;
		ConcurrentList<time_t> write_epoch_table;
		std::mutex version_mutex;

		std::atomic<RelVersion*> read_rel_version = NULL;
		RelVersion* current_rel_version = NULL;

		std::vector<std::shared_ptr<std::thread>> high_compact_thread;
		std::vector<std::shared_ptr<std::thread>> low_compact_thread;
		std::atomic<size_t> working_compact_thread = 0;
		std::atomic<bool> progressing_read_version = false;
		std::atomic<bool> write_stall[2] = {false, false};
		std::mutex write_stall_mutex;
		std::condition_variable write_stall_cv;

#ifdef RUN_PROFILER
		// profiler for every compaction thread
		std::vector<ThreadProfiler> compaction_profilers_;

		// DB Profiler
		DBProfiler db_profiler;
#endif

		bool close = false;
		//compaction loop gfor background thread
		void HighCompactLoop();
		void LowCompactLoop();
		void TryCompaction(idx_t level);
		void ProgressReadRelVersion();
		time_t get_read_time();
		RelVersion* get_read_rel_version();
		void check_write_stall();

		friend class Transaction;
	};
}