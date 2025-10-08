#include "LSM-OPD/memory/RowMemoryManager.h"
#include "LSM-OPD/sstable/RelVersion.h"
#include "LSM-OPD/db/DB.h"

namespace LSMOPD {
    rowMemoryManager::rowMemoryManager(DB *_db, idx_t _column_num) : db(_db), column_num(_column_num), memtable_cnt(1) {
        currentMemTable = std::make_shared<relMemTable>(0, column_num);
    }

    void rowMemoryManager::PutTuple(const Tuple &tuple, const tp_key &key, time_t timestamp, bool tombstone) {
        std::shared_lock<std::shared_mutex> lock(currentMemTable->mutex);
        while(!currentMemTable->PutTuple(tuple, key, timestamp, tombstone));
        CheckAndImmute();
    }

    Tuple rowMemoryManager::GetTuple(const tp_key &key, time_t timestamp, RelVersion *version) {
        auto x = currentMemTable;
        while (x)  {
            //std::shared_lock<std::shared_mutex> lock(x->mutex);
            auto now_res = x->GetTuple(key, timestamp);
            if (!now_res.row.empty()) {
                return now_res;
            }
            if (x == version->size_entry) break;
            x = x->next.lock();
        }
        return Tuple();
    }

    void rowMemoryManager::GetKTuple(idx_t k, const tp_key &key, time_t timestamp, std::vector<std::unique_ptr<Tuple>> &am, RelVersion *version) {
        auto x = currentMemTable;
        while (x)  {
            x->GetKTuple(k, key, timestamp, am);
            if (x == version->size_entry) break;
            x = x->next.lock();
        }
    }

    std::vector<Tuple> rowMemoryManager::ScanTuples(const tp_key &start_key, const tp_key &end_key, time_t timestamp) {
        std::shared_lock<std::shared_mutex> lock(currentMemTable->mutex);

        auto x = currentMemTable;
        while (x) {
            auto now_res = x->ScanTuples(start_key, end_key, timestamp);
            x = x->next.lock();
        }
        return std::vector<Tuple>();
    }


    void rowMemoryManager::CheckAndImmute() {
        if (currentMemTable->current_data_count >= db->options->MEM_TABLE_MAX_SIZE) 
            if (!currentMemTable->immutable.exchange(true)) {
                immute_memtable(currentMemTable);
                memtable_cnt.fetch_add(1, std::memory_order_relaxed);
                if (memtable_cnt.load() > db->options->MAX_MEMTABLE_NUM) {
                    db->StallWrite(0);
                }
            }
        return;
    }

    void rowMemoryManager::immute_memtable(std::shared_ptr<relMemTable> memtable) {
        auto new_memtable = std::make_shared<relMemTable>(0, memtable->column_num, memtable);
        currentMemTable = new_memtable;
        memtable->sema.release(1024);
        RelCompaction<std::string> x;
        x.target_level = 0;
        x.relPersistence = memtable;
        x.key_min = memtable->min_key;
        db->relFiles->AddCompaction(x);
    }


    VersionEdit *rowMemoryManager::RowMemtablePersistence(idx_t file_id, std::shared_ptr<relMemTable> memtable) {
        std::unique_lock<std::shared_mutex> lock(memtable->mutex);
        std::string key_min = memtable->min_key, key_max = memtable->max_key;
        size_t total_size = memtable->tuple_index->size();
        auto temp_file_metadata = new RelFileMetaData(0, file_id, key_min, key_max, total_size,
                                                      memtable->column_num - 1);
        std::string file_name = temp_file_metadata->file_name;
        auto fw = std::make_shared<FileWriter>(db->options->STORAGE_DIR + "/" + file_name, false);
        RelFileBuilder<std::string> rfb(fw, db->options);
        TempColumn tmp(memtable.get(), total_size, memtable->column_num);
        idx_t **data = new idx_t *[memtable->column_num];
        std::vector<OrderedDictionary *> dicts(memtable->column_num - 1);
        for (size_t i = 0; i < memtable->column_num - 1; i++) {
            data[i] = new idx_t[total_size];
            temp_file_metadata->dictionary.emplace_back(OrderedDictionary());
            auto nsiz = temp_file_metadata->dictionary.size();
            auto &now_dict = temp_file_metadata->dictionary[nsiz - 1];
            now_dict.importData(tmp.GetColumn(i + 1), total_size);
            now_dict.CompressData(data[i], tmp.GetColumn(i + 1), total_size);
        }
        rfb.ArrangeRelFileInfo(tmp.GetColumn(0), total_size, db->options->KEY_SIZE, memtable->column_num - 1,
                               data);
        temp_file_metadata->bloom_filter = BloomFilter(total_size, db->options->FALSE_POSITIVE);
        for (size_t i = 0; i < total_size; i++) {
            temp_file_metadata->bloom_filter.insert(tmp.GetColumn(0)[i]);
        }
        temp_file_metadata->block_count = rfb.GetBlockCount();
        temp_file_metadata->block_meta_begin_pos = rfb.GetBlockMetaBeginPos();
        for (size_t i = 0; i < memtable->column_num - 1; i++) {
            delete[] data[i];
        }
        delete[] data;

        memtable_cnt.fetch_add(-1, std::memory_order_relaxed);
        if (memtable_cnt.load() <= db->options->MAX_MEMTABLE_NUM) {
            db->ResumeWrite(0);
        }

        auto vedit = new VersionEdit();
        temp_file_metadata->file_size = fw->file_size();
        vedit->EditFileList.push_back(temp_file_metadata);
        return vedit;
    }


    void rowMemoryManager::FilterByValueRange(time_t timestamp, const std::function<bool(const Tuple &)> &func,
                                              AnswerMerger &am, RelVersion *version) {
        auto x = currentMemTable;
        while (x) {
            x->FilterByValueRange(timestamp, func, am);
            if (x == version->size_entry) break;
            x = x->next.lock();
        }
    }

    //template<typename func>
    std::function<bool(const Tuple &)> CreateValueFilterFunction(const idx_t column_idx, const std::string &value_min,
                                                           const std::string &value_max) {
        return [value_min, value_max, column_idx](const Tuple &tuple) {
            if (tuple.row[column_idx] >= value_min && tuple.row[column_idx] <= value_max) {
                return true;
            }
            return false;
        };
    }
}
