#pragma once
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include "LSM-OPD/utils/types.h"
#include "LSM-OPD/utils/Options.h"
#include "LSM-OPD/memory/TupleEntry.h"
#include "LSM-OPD/common/tuple.h"
#include "LSM-OPD/sstable/Compaction.h"
#include "LSM-OPD/sstable/RelFileBuilder.h"
#include "LSM-OPD/memory/RowToColumn.h"
#include "LSM-OPD/compress/ordered_dictionary.h"
#include "LSM-OPD/memory/AnswerMerger.h"

namespace LSMOPD {
    typedef std::string tp_key;

    class DB;
    class RelVersion;

    class rowMemoryManager {
    public:
        rowMemoryManager() = delete;

        rowMemoryManager(const rowMemoryManager &) = delete;

        rowMemoryManager &operator=(const rowMemoryManager &) = delete;

        rowMemoryManager(DB *_db, idx_t _column_num);

        ~rowMemoryManager() = default;

        void PutTuple(const Tuple &tuple, const tp_key &key, time_t timestamp, bool tombstone = false);

        Tuple GetTuple(const tp_key &key, time_t timestamp, RelVersion *version);

        std::vector<Tuple> ScanTuples(const tp_key &start_key, const tp_key &end_key, time_t timestamp);

        void immute_memtable(std::shared_ptr<relMemTable> size_info);

        VersionEdit *RowMemtablePersistence(idx_t file_id, std::shared_ptr<relMemTable> size_info);

        void FilterByValueRange(time_t timestamp, const std::function<bool(const Tuple &)> &func, AnswerMerger &am, RelVersion *version);

        void GetKTuple(idx_t k, const std::string &key, time_t timestamp, std::vector<std::unique_ptr<Tuple>> &am, RelVersion *version);

        std::shared_ptr<relMemTable> GetCurrentTable() {
            return currentMemTable;
        }

    private:
        DB *db;
        std::shared_ptr<relMemTable> currentMemTable;
        size_t data_count_threshold = 1024; 
        size_t column_num = 0;
        std::atomic<size_t> memtable_cnt;
        void CheckAndImmute();
    };

    // value filter function factory
    std::function<bool(const Tuple &)> CreateValueFilterFunction(const idx_t column_idx, const std::string &value_min,
                                                           const std::string &value_max);
}
