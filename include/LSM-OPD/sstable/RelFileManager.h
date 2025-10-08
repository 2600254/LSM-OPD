#pragma once
#include <algorithm>
#include <condition_variable>
#include <map>
#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <queue>
#include <filesystem>
#include "BloomFilter.h"
#include "Compaction.h"
#include "LSM-OPD/file/FileWriter.h"
#include "LSM-OPD/utils/Options.h"
#include "LSM-OPD/sstable/RelFileParser.h"
#include "LSM-OPD/sstable/RelFileBuilder.h"
#include "LSM-OPD/sstable/RelVersion.h"
#include "LSM-OPD/common/tuple.h"
#include "LSM-OPD/compress/ordered_dictionary.h"

namespace LSMOPD {
    class DB;

    class RelFileManager {
    public:
        RelFileManager() = default;

        RelFileManager(const RelFileManager &) = default;

        RelFileManager &operator=(const RelFileManager &) = default;

        RelFileManager(DB *_db);

        ~RelFileManager() = default;

        void AddCompaction(RelCompaction<std::string> &compaction, bool high = true);

        VersionEdit *MergeRelFile(RelCompaction<std::string> &compaction);

        idx_t GetFileID() {
            return ++id;
        };

    private:
        DB *db;
		std::mutex HighCompactionCVMutex;
        std::condition_variable HighCompactionCV;
        std::queue<RelCompaction<std::string>> HighCompactionList;
		std::mutex LowCompactionCVMutex;
        std::condition_variable LowCompactionCV;
        std::queue<RelCompaction<std::string>> LowCompactionList;
        std::vector< //level
            std::vector< //key-min
                idx_t> > FileNumList;
        std::atomic<int> id = 0;
        friend class DB;
    };
}
