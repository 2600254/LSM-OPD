#pragma once

#include <atomic>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include "BloomFilter.h"
#include "BlockParser.h"
#include "RelFileMetaData.h"
#include "LSM-OPD/file/FileReader.h"
#include "LSM-OPD/utils/types.h"
#include "LSM-OPD/utils/utils.h"
#include "LSM-OPD/utils/Options.h"
#include "LSM-OPD/common/tuple.h"
#include "LSM-OPD/sstable/BloomFilter.h"
#include "LSM-OPD/compress/ordered_dictionary.h"

#pragma once

namespace LSMOPD {
    template<typename Key_t>
    struct RelFileMetaData;

    template<typename Key_t>
    struct BlockMetaT {
        Key_t key_min; // key_max;
        size_t offset_in_file;
        size_t block_size;
    };

    template<typename Key_t>
    class RelFileParser {
    public:
        RelFileParser(FileReader *_fileReader, std::shared_ptr<Options> _options, size_t _file_size,
                      RelFileMetaData<Key_t> *_meta): reader(_fileReader), options(_options),
                                                      file_size(_file_size) {
            auto key_size = Options::KEY_SIZE;
            key_min = _meta->key_min;
            key_max = _meta->key_max;
            key_num = _meta->key_num;
            col_num = _meta->col_num;
            block_count = _meta->block_count;
            block_meta_begin_pos = _meta->block_meta_begin_pos;
            size_t meta_size = key_size + 2 * sizeof(size_t);
            size_t now_meta_offset = block_meta_begin_pos;
            size_t tot_meta_size = (meta_size) * (block_count - 1) + meta_size;
            char blockinfobuf[tot_meta_size];
            if (!reader->fread(blockinfobuf, tot_meta_size, now_meta_offset)) {
                std::cout << "read fail begin" << std::endl;
                ++*(int *) NULL;
            }
            block_meta.resize(block_count);
            now_meta_offset = 0;
            for (idx_t i = 0; i < block_count; i++) {
                BlockMetaT<Key_t> meta{"", 0, 0};
                util::DecodeFixed(blockinfobuf + now_meta_offset, meta.key_min);
                util::DecodeFixed(blockinfobuf + now_meta_offset + key_size, meta.offset_in_file);
                util::DecodeFixed(blockinfobuf + now_meta_offset + key_size + sizeof(size_t), meta.block_size);
                block_meta[i] = std::move(meta);

                // now_meta_offset += meta_size + block_filter_size;
                now_meta_offset += meta_size;
            }
        }

        RelFileParser(RelFileParser &&x) : reader(x.reader), options(x.options), file_size(x.file_size),
                                           col_num(x.col_num), key_num(x.key_num), block_count(x.block_count),
                                           key_min(x.key_min), key_max(x.key_max),
                                           block_meta_begin_pos(x.block_meta_begin_pos), block_meta(x.block_meta) {
            x.valid = false;
        }

        ~RelFileParser() {
            if (valid) {
                reader->DecRef();
            }
        };

        Tuple GetTuple(Key_t key) {
            if constexpr (std::is_same_v<std::string, Key_t>) {
                key.resize(Options::KEY_SIZE);
            }
            for (idx_t i = 0; i < block_count; i++) {
                BlockMetaT<Key_t> &meta = block_meta[i];
                if (key < meta.key_min) {
                    continue;
                }
                if (i != block_count - 1 && key >= block_meta[i + 1].key_min) {
                    continue;
                }
                //if(meta.filter->exists(key)) {
                if (true) {
                    BlockParser<Key_t> block_parser(reader, options,
                                                    meta.offset_in_file, meta.block_size, col_num);
                    Tuple res = block_parser.GetTuple(key);
                    if (res.row.size() > 0) {
                        return res;
                    }
                }
            }
            return Tuple();
        }

        //copy the columns from blockparser, and free them
        void GetKeyCol(Key_t *keys, idx_t &key_num) {
            int idx = 0;
            for (idx_t i = 0; i < block_count; i++) {
                BlockMetaT<Key_t> &meta = block_meta[i];
                BlockParser<Key_t> block_parser(reader, options,
                                                meta.offset_in_file, meta.block_size, col_num);
                idx_t block_key_num = block_parser.key_num;
                block_parser.GetKeyCol(keys + idx);
                key_num += block_key_num;
                idx += block_key_num;
            }
        }

        void GetKeyCol(char *keys, idx_t &key_num) {
            int idx = 0;
            for (idx_t i = 0; i < block_count; i++) {
                BlockMetaT<Key_t> &meta = block_meta[i];
                BlockParser<Key_t> block_parser(reader, options,
                                                meta.offset_in_file, meta.block_size, col_num);
                idx_t block_key_num = block_parser.key_num;
                block_parser.GetKeyCol(keys + idx * options->KEY_SIZE);
                key_num += block_key_num;
                idx += block_key_num;
            }
        }

        void GetValCol(idx_t *vals, idx_t &val_num, idx_t col_id) {
            int idx = 0;
            for (idx_t i = 0; i < block_count; i++) {
                BlockMetaT<Key_t> &meta = block_meta[i];
                BlockParser<Key_t> block_parser(reader, options,
                                                meta.offset_in_file, meta.block_size, col_num);
                idx_t *block_vals = block_parser.GetValCol(col_id);
                idx_t block_key_num = block_parser.key_num;
                for (idx_t j = 0; j < block_key_num; j++) {
                    vals[idx++] = block_vals[j];
                }
                val_num += block_key_num;
            }
        }

        //temporarily only support one column (just for experiment)
        void GetKVTogether(Key_t *keys, idx_t &key_num, idx_t *vals, idx_t &val_num, idx_t col_id) {
            int idx = 0;
            for (idx_t i = 0; i < block_count; i++) {
                BlockMetaT<Key_t> &meta = block_meta[i];
                BlockParser<Key_t> block_parser(reader, options,
                                                meta.offset_in_file, meta.block_size, col_num);
                idx_t block_key_num = block_parser.key_num;
                block_parser.GetKeyCol(keys + idx);
                idx_t *block_vals = block_parser.GetValCol(col_id);
                for (idx_t j = 0; j < block_key_num; j++) {
                    vals[idx++] = block_vals[j];
                }
                val_num += block_key_num;
                key_num += block_key_num;
            }
        }

        static bool CompareKey(const Key_t &key, const BlockMetaT<Key_t> &meta) {
            return key < meta.key_min;
        }

        std::vector<Tuple> GetKTuple(idx_t &k, Key_t key) {
            idx_t &now_k = k;
            std::vector<Tuple> res;
            res.reserve(k);
            auto iter = upper_bound(block_meta.begin(), block_meta.end(), key, CompareKey);
            // if (iter == block_meta.end()) {
            //     return res;
            // }
            if (iter != block_meta.begin()) --iter;
            for (; iter != block_meta.end(); ++iter) {
                BlockMetaT<Key_t> &meta = *iter;
                if (iter != block_meta.end() - 1 && key >= (iter + 1)->key_min) continue;
                BlockParser<Key_t> block_parser(reader, options,
                                                meta.offset_in_file, meta.block_size, col_num);
                block_parser.GetKTuple(now_k, key, res);
                if (!now_k) break;
            }
            return res;
        }

        idx_t GetBlock(Key_t key, BlockParser<Key_t> *&block) {
            auto iter = upper_bound(block_meta.begin(), block_meta.end(), key, CompareKey);
            if (iter != block_meta.begin()) --iter;
            if (iter != block_meta.end() - 1 && key >= (iter + 1)->key_min) ++iter;
            BlockMetaT<Key_t> &meta = *iter;
            block = new BlockParser<Key_t>(reader, options, meta.offset_in_file, meta.block_size, col_num);
            return iter - block_meta.begin();
        }

        BlockParser<Key_t> *GetBlock(const idx_t &block_idx) {
            BlockMetaT<Key_t> &meta = block_meta[block_idx];
            return new BlockParser<Key_t>(reader, options, meta.offset_in_file, meta.block_size, col_num);
        }

        idx_t GetColumnNum() {
            return col_num;
        }

        idx_t GetKeyNum() {
            return key_num;
        }

    private:
        FileReader *reader;
        std::shared_ptr<Options> options;

        size_t file_size = 0;
        idx_t col_num = 0;
        idx_t key_num = 0;
        idx_t block_count = 0;
        Key_t key_min, key_max;
        size_t block_meta_begin_pos;
        size_t block_filter_size;
        size_t last_block_filter_size;
        idx_t block_func_num;

        std::vector<BlockMetaT<Key_t> > block_meta;

        bool valid = true;
    };
}
