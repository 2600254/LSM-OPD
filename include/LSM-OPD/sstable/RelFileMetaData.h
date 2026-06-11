#pragma once

#include <atomic>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include "BloomFilter.h"
#include "BlockParser.h"
#include "LSM-OPD/file/FileReader.h"
#include "LSM-OPD/utils/types.h"
#include "LSM-OPD/utils/utils.h"
#include "LSM-OPD/utils/Options.h"
#include "LSM-OPD/common/tuple.h"
#include "LSM-OPD/sstable/BloomFilter.h"
#include "LSM-OPD/compress/ordered_dictionary.h"

#pragma once

namespace LSMOPD
{
    template <typename Key_t>
    struct RelFileMetaData
    {
        std::string file_name;
        idx_t level;
        idx_t file_id;
        std::atomic<idx_t> ref = 0;
        size_t file_size;
        bool deletion = false;
        bool merging = false;
        std::atomic<FileReader *> reader = NULL;
        BloomFilter bloom_filter;
        idx_t reader_pos = -1;
        size_t id = 0;
        using DictList = std::vector<OrderedDictionary>;

        std::atomic<std::weak_ptr<DictList> > dictionary;
        Key_t key_min, key_max;
        idx_t key_num, col_num;

        idx_t block_count;
        size_t block_meta_begin_pos;
        size_t dict_begin_pos;
        size_t single_val_size;
        size_t block_filter_size;
        size_t last_block_filter_size;
        idx_t block_func_num;
        RelFileParser<Key_t> *parser = nullptr;
        std::atomic<int> dict_ref = 0;
        std::atomic<bool> dict_loading = false;
        bool no_overlap_in_next_level = true;

        std::shared_ptr<DictList> GetDict() {
            while (true) {
                auto ans = dictionary.load(std::memory_order_acquire).lock();
                if (ans != nullptr) {
                    return ans;
                }

                bool expected = false;
                if (dict_loading.compare_exchange_strong(expected, true,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_relaxed)) {
                    try {
                        auto loaded = std::make_shared<DictList>();
                        loaded->resize(1);
                        if (parser != nullptr) {
                            OrderedDictionary dict;
                            parser->GetDict(dict);
                            (*loaded)[0] = std::move(dict);
                        }
                        dictionary.store(std::weak_ptr<DictList>(loaded), std::memory_order_release);
                        dict_loading.store(false, std::memory_order_release);
                        dict_loading.notify_all();
                        return loaded;
                    } catch (...) {
                        dict_loading.store(false, std::memory_order_release);
                        dict_loading.notify_all();
                        throw;
                    }
                }

                dict_loading.wait(true, std::memory_order_acquire);
            }
        }

        void SetDict(const std::shared_ptr<DictList> &dict) {
            dictionary.store(std::weak_ptr<DictList>(dict), std::memory_order_release);
        }

        void SetDict(const std::weak_ptr<DictList> &dict) {
            dictionary.store(dict, std::memory_order_release);
        }

        std::weak_ptr<DictList> GetDictWeak() const {
            return dictionary.load(std::memory_order_acquire);
        }

        std::string GetSingleValueFromDict(idx_t idx) {
            auto x = dictionary.load(std::memory_order_acquire).lock();
            if (x != nullptr) {
                return x->at(0).getString(idx);
            }
            return parser->GetSingleValueFromDict(idx);
        }

        void ReleaseDict() {
        }

        RelFileMetaData() = default;

        RelFileMetaData(RelFileMetaData &&x) : file_name(x.file_name), level(x.level),
                                               file_id(x.file_id), ref(x.ref.load()),
                                               file_size(x.file_size), deletion(x.deletion), reader(x.reader.load()),
                                               bloom_filter(std::move(x.bloom_filter)), id(x.id),
                                               key_min(x.key_min), key_max(x.key_max),
                                               key_num(x.key_num), col_num(x.col_num),
                                               block_count(x.block_count), block_meta_begin_pos(x.block_meta_begin_pos),
                                               dict_begin_pos(x.dict_begin_pos), single_val_size(x.single_val_size),
                                               block_filter_size(x.block_filter_size),
                                               last_block_filter_size(x.last_block_filter_size),
                                               block_func_num(x.block_func_num),
                                               parser(x.parser), dict_ref(x.dict_ref.load()) {
            dictionary.store(x.dictionary.load(std::memory_order_acquire), std::memory_order_release);
        }

        RelFileMetaData(const RelFileMetaData &x) : file_name(x.file_name), level(x.level),
                                                    file_id(x.file_id), ref(x.ref.load()),
                                                    file_size(x.file_size), deletion(x.deletion), reader(x.reader.load()),
                                                    bloom_filter(x.bloom_filter), id(x.id), key_min(x.key_min),
                                                    key_max(x.key_max), key_num(x.key_num), col_num(x.col_num), block_count(x.block_count),
                                                    block_meta_begin_pos(x.block_meta_begin_pos),
                                                    dict_begin_pos(x.dict_begin_pos),
                                                    single_val_size(x.single_val_size),
                                                    block_filter_size(x.block_filter_size),
                                                    last_block_filter_size(x.last_block_filter_size),
                                                    block_func_num(x.block_func_num),
                                                    parser(x.parser), dict_ref(x.dict_ref.load()) {
            dictionary.store(x.dictionary.load(std::memory_order_acquire), std::memory_order_release);
        }

        RelFileMetaData(idx_t _level, idx_t _file_id, Key_t _key_min, Key_t _key_max,
                        idx_t _key_num, idx_t _col_num) : level(_level), file_id(_file_id), id((size_t)level << 48 | (size_t)file_id),
                                                          key_min(_key_min), key_max(_key_max), key_num(_key_num),
                                                          col_num(_col_num)
        {
            if constexpr (std::is_same_v<Key_t, std::string>)
            {
                file_name = std::to_string(_level) + std::string(key_min.c_str()) + "-" + std::string(key_max.c_str()) + std::to_string(_file_id);
            }
            else
            {
                file_name = std::to_string(_level) + std::to_string(key_min) + std::to_string(key_max) + std::to_string(_file_id);
            }
        }
    };
}
