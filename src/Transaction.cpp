#include "LSM-OPD/db/Transaction.h"

namespace LSMOPD {
    Transaction::Transaction(time_t _write_epoch, time_t _read_epoch,
                             DB *db, RelVersion *_version, time_t pos) : write_epoch(_write_epoch),
                                                                         read_epoch(_read_epoch), db(db), time_pos(pos),
                                                                         rel_version(_version) {
    }

    Transaction::Transaction(Transaction &&txn) : write_epoch(txn.write_epoch), read_epoch(txn.read_epoch),
                                                  db(txn.db), time_pos(txn.time_pos), rel_version(txn.rel_version) {
        txn.valid = false;
    }

    Transaction::~Transaction() {
        if (valid) {
            if (rel_version) rel_version->DecRef();
            if (write_epoch != MAXTIME) {
                db->write_epoch_table.erase(time_pos);
                if (rel_version) db->ProgressReadRelVersion();
            }
#ifdef RUN_PROFILER
            db->db_profiler.AddThreadProfiler(profiler);
#endif
        }
    }

    void Transaction::PutTuple(const Tuple &tuple, const tp_key &key) {
        if (write_epoch == MAXTIME) {
            return;
        }
        db->check_write_stall();
        // START_OPERATOR_PROFILER();
        db->RowMemtable->PutTuple(tuple, key, write_epoch);
        // END_OPERATOR_PROFILER("PutTuple");
    }

    void Transaction::DelTuple(const tp_key &key) {
        if (write_epoch == MAXTIME) {
            return;
        }
        db->check_write_stall();
        // START_OPERATOR_PROFILER();
        Tuple x;
        x.row.push_back(key);
        db->RowMemtable->PutTuple(x, key, write_epoch, true);
        // END_OPERATOR_PROFILER("DelTuple");
    }

    Tuple Transaction::GetTuple(const tp_key &key) {
        // START_OPERATOR_PROFILER();
        Tuple x = db->RowMemtable->GetTuple(key, read_epoch, rel_version);
        if (!x.row.empty()) {
            // END_OPERATOR_PROFILER("GetTuple");
            return x;
        }
        RelVersionIterator iter(rel_version, key, key);
        while (!iter.End()) {
            if (key >= iter.GetFile()->key_min && key <= iter.GetFile()->key_max) {
                if (iter.GetFile()->bloom_filter.exists(key)) {

                    RelFileParser<std::string>* parser = iter.GetFile()->parser;
                    Tuple found = parser->GetTuple(key);
                    if (!found.row.empty()) {
                        for (size_t i = 1; i < found.row.size(); i++) {
                            idx_t col_id = *((idx_t *) found.row[i].data());
                            found.row[i] = static_cast<RelFileMetaData<std::string> *>(iter.GetFile())->GetSingleValueFromDict(col_id);
                        }
                        iter.GetFile()->ReleaseDict();
                        // END_OPERATOR_PROFILER("GetTuple");
                        return found;
                    }
                }
            }
            iter.next();
        }
        // END_OPERATOR_PROFILER("GetTuple");
        return Tuple();
    }

    void Transaction::ScanTuples() {
        START_OPERATOR_PROFILER();

        AnswerMerger am;

        auto files = rel_version->FileIndex;
        for (auto cur_level: files) {
            for (auto cur_file: cur_level) {
                auto reader = db->ReaderCaches->find(cur_file);
                auto parser = RelFileParser<std::string>(reader, db->options, cur_file->file_size, cur_file);
                RowGroup cur_row_group(db, cur_file);
                cur_row_group.GetKeyData();
                cur_row_group.GetAllColData();
                cur_row_group.MaterializeAll(am);
            }
        }
        END_OPERATOR_PROFILER("ScanTuples");
    }

    std::vector<std::unique_ptr<Tuple>> Transaction::ScanKTuples(idx_t k, std::string key) {
        // START_OPERATOR_PROFILER();

        std::vector<std::unique_ptr<Tuple>> am;
        db->RowMemtable->GetKTuple(k, key, read_epoch, am, rel_version);

        std::string KEY_MAX;
        for (unsigned int i = 0; i < db->options->KEY_SIZE; i++) KEY_MAX += ((char) 127);
        RelVersionIterator iter(rel_version, key, KEY_MAX);
        idx_t now_level_k = k;
        std::vector<std::unique_ptr<Tuple>> res;
        while (!iter.End()) {
            auto cur_file = iter.GetFile();
            auto rel_file = static_cast<RelFileMetaData<std::string> *>(cur_file);
            RelFileParser<std::string>* parser = cur_file->parser;
            auto an = parser->GetKTuple(now_level_k, key);
            std::shared_ptr<std::vector<OrderedDictionary> > dict;
            for (auto x: an) {
                if (!x.row.empty()) {
                    if (dict == nullptr) {
                        dict = rel_file->GetDict();
                    }
                    for (size_t i = 1; i < x.row.size(); i++) {
                        idx_t col_id = *((idx_t *) x.row[i].data());
                        x.row[i] = dict->at(i - 1).getString(col_id);
                    }
                    res.emplace_back(std::make_unique<Tuple>(std::move(x)));
                }
            }
            if(!now_level_k){
                iter.nextlevel();
                now_level_k = k;
                std::vector<std::unique_ptr<Tuple>> as;
                size_t i = 0, j = 0;
                while (i < res.size() && j < am.size() && as.size() < k) {
                    auto x = res[i]->row[0] <=> am[j]->row[0];
                    if (x < 0) {
                        as.emplace_back(std::move(res[i]));
                        i++;
                    } else if (x > 0) {
                        as.emplace_back(std::move(am[j]));
                        j++;
                    } else {
                        // if equal, we prefer the one in nowa
                        as.emplace_back(std::move(am[j]));
                        i++;
                        j++;
                    }
                }
                while (i < res.size() && as.size() < k) {
                    as.emplace_back(std::move(res[i]));
                    i++;
                }
                while (j < am.size() && as.size() < k) {
                    as.emplace_back(std::move(am[j]));
                    j++;
                }
                am.swap(as);
                res.clear();
            } 
            else iter.next();
        }
        // END_OPERATOR_PROFILER("ScanKTuples");
        return am;
    }

    ScanIter Transaction::GetIter(std::string key) {
        return ScanIter(rel_version, db->RowMemtable->GetCurrentTable(), key);
    }
}
