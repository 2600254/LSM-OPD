#pragma once
#include <memory>
#include "LSM-OPD/sstable/RelFileParser.h"
#include "LSM-OPD/utils/types.h"
#include "LSM-OPD/common/vector.h"
#include "LSM-OPD/memory/AnswerMerger.h"
#include "LSM-OPD/vectorized_execution/operator.h"

namespace LSMOPD {
    class DB;

    //RowGroup holds the real memory when doing AP ops
    //needs to malloc & free the memory manually
    class RowGroup {
    public:
        RowGroup(DB* _db,  RelFileMetaData<std::string>* _file);

        ~RowGroup() {
            if (keys != nullptr) free(keys);
            if (scan_pos != nullptr) free(scan_pos);
            if (parser != nullptr) delete parser;
            for(auto x : cols) {
                if(x != nullptr) free(x);
            }
        }

        void GetKeyData() {
            idx_t tmp = 0;
            parser->GetKeyCol(keys, tmp);
        }

        void GetColData(idx_t col_id) {
            idx_t tmp = 0;
            parser->GetValCol(cols[col_id], tmp, col_id);
        }

        void GetAllColData() {
            for (idx_t i = 0; i < col_num; i++) {
                GetColData(i);
            }
        }

        bool Scan(idx_t col_id, Vector &result);
        void Materialize(Vector &result, AnswerMerger &am);

        template<typename Func>
        void ApplyRangeFilter(idx_t col_id, Func left_bound, Func right_bound, AnswerMerger &am) {
            Vector res;
            while (Scan(col_id, res)) {
                RangeFilter(res, left_bound, right_bound);
                Materialize(res, am);
            }
        }

        void MaterializeAll(AnswerMerger &am);

    private:
        DB* db;
        idx_t key_num, col_num;
        RelFileMetaData<std::string>* file;
        idx_t* scan_pos;
        char* keys = nullptr;
        std::vector<idx_t*> cols;
        RelFileParser<std::string>* parser;

        OrderedDictionary* dict(idx_t col_id) {
            return &file->dictionary[col_id];
        }

        friend class DB;
    };
}