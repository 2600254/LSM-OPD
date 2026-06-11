#pragma once
#include <unordered_map>
#include "LSM-OPD/compress/ordered_dictionary.h"
#include <utility>
#include <vector>
#include "LSM-OPD/sstable/BlockParser.h"
#include "LSM-OPD/sstable/RelVersion.h"
#include "LSM-OPD/memory/RowMemoryManager.h"

namespace LSMOPD {
    class ScanIter {
    public:
        ScanIter(RelVersion *rel_version,  std::shared_ptr<relMemTable> x, std::string key);
        ScanIter(const ScanIter &) = delete;
        ScanIter &operator=(const ScanIter &) = delete;
        ScanIter(ScanIter &&other) noexcept = default;
        ScanIter &operator=(ScanIter &&other) noexcept = default;
        
        ~ScanIter();

        void next();

        bool end() const {
            return is_end;
        }

        void GetNow();

        void GetTuple(Tuple &res);

    private:
        struct BlockPointer {
            bool is_end = false;
            idx_t level, file_idx;
            idx_t block_idx, idx;
            BlockParser<std::string> *block = nullptr;
            std::string now_key;

            BlockPointer() = default;
            BlockPointer(const BlockPointer &) = delete;
            BlockPointer &operator=(const BlockPointer &) = delete;
            BlockPointer(BlockPointer &&other) noexcept
                : is_end(other.is_end), level(other.level), file_idx(other.file_idx),
                  block_idx(other.block_idx), idx(other.idx), block(other.block),
                  now_key(std::move(other.now_key)) {
                other.block = nullptr;
                other.is_end = true;
            }
            BlockPointer &operator=(BlockPointer &&other) noexcept {
                if (this != &other) {
                    if (block) delete block;
                    is_end = other.is_end;
                    level = other.level;
                    file_idx = other.file_idx;
                    block_idx = other.block_idx;
                    idx = other.idx;
                    block = other.block;
                    now_key = std::move(other.now_key);
                    other.block = nullptr;
                    other.is_end = true;
                }
                return *this;
            }

            ~BlockPointer() {
                if (block) delete block;
            }

            void next(const RelVersion *rel_version) {
                idx++;
                if (idx >= block->GetKeyNum()) {
                    idx = 0;
                    block_idx++;
                    delete block;
                    block = nullptr;
                    if (block_idx >= rel_version->FileIndex[level][file_idx]->block_count) {
                        block_idx = 0;
                        file_idx++;
                        if (level == 0 || file_idx >= rel_version->FileIndex[level].size()) {
                            is_end = true;
                            return;
                        }
                    }
                    block = rel_version->FileIndex[level][file_idx]->parser->GetBlock(block_idx);
                }
                now_key = block->GetKeyWithIdx(idx);
            }

            bool end() const {
                return is_end;
            }
        };

        struct MemPointer {
            std::string now_key;
            RelSkipList::Accessor accessor;
            RelSkipList::Accessor::iterator iter;
            bool is_end = false;

            MemPointer(std::shared_ptr<relMemTable> mem_table, const std::string &key): accessor(mem_table->tuple_index) {
                iter = accessor.lower_bound(std::make_pair(key, 0));
                if (iter == accessor.end()) {
                    is_end = true;
                }
                else now_key = iter->first;
            }

            void next() {
                if (is_end) return;
                ++iter;
                if (iter != accessor.end()) {
                    now_key = iter->first;
                } else
                    is_end = true;
            }

            bool end() const {
                return is_end;
            }
        };

        std::vector<BlockPointer> block_ptr_list;
        std::vector<MemPointer> mem_ptr_list;

        int now_least_type = 0; // 0 indicated invalid, 1 indicated in mem-list, 2 in block-list
        int now_least_pos = -1; // pos in the list

        int mem_ptr_num = 0;
        int block_ptr_num = 0;
        RelVersion *rel_version;
        rowMemoryManager *memory_manager;

        std::vector<std::shared_ptr<relMemTable> > hold;
        bool is_end = false;
    };
} // LSMOPD
