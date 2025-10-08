#pragma once

#include <memory>
#include <vector>
#include <iostream>
#include "BloomFilter.h"
#include "LSM-OPD/file/FileWriter.h"
#include "LSM-OPD/utils/types.h"
#include "LSM-OPD/utils/utils.h"

namespace LSMOPD {
    template<typename Key_t>
    class RelFileBuilder;

    template<typename Key_t>
    class BlockBuilder {
    public:
        explicit BlockBuilder(std::shared_ptr<FileWriter> _fileWriter, std::shared_ptr<Options> _options):
            writer(_fileWriter),
            options(_options){}

        ~BlockBuilder() = default;

        void ArrangeBlockInfo(std::shared_ptr<BloomFilter> filter, Key_t *key, idx_t key_num, idx_t col_num,
                              size_t key_size = 0, size_t *col_size = nullptr) {
            Key_t key_max = key[0], key_min = key[0];
            for (idx_t i = 0; i < key_num; i++) {
                key_max = max(key_max, key[i]);
                key_min = min(key_min, key[i]);
            }
            SetKeyRange(key_min, key_max);
            std::string metadata;
            for (idx_t i = 0; i < col_num; i++) {
                util::PutFixed(metadata, col_size[i]);
            }
            util::PutFixed(metadata, key_num);
            util::PutFixed(metadata, col_num);
            util::PutFixed(metadata, key_size);
            writer->append(metadata.data(), metadata.size());

            writer->flush();
        }

        void SetKey(Key_t *keys, idx_t key_num = 0, size_t key_size = 0) {
            if (key_num) {
                std::string temp_data;
                for (idx_t i = 0; i < key_num; i++) {
                    keys[i].resize(key_size);
                    temp_data.append(keys[i]);
                }
                writer->append(temp_data.data(), temp_data.size());
            } else {
                return;
            }
        }

        size_t SetValueIdx(idx_t *vals, idx_t val_num = 0) {
            //vals can be compressed here, return the bitpacked size
            writer->append(reinterpret_cast<char *>(vals), (int32_t) val_num * sizeof(idx_t));
            return sizeof(idx_t);
        }

    private:
        std::shared_ptr<FileWriter> writer;
        std::shared_ptr<Options> options;
        Key_t key_min, key_max;

        void SetKeyRange(const Key_t &key_min, const Key_t &key_max) {
            this->key_min = key_min;
            this->key_max = key_max;
        }

        friend class RelFileBuilder<Key_t>;
    };
}
