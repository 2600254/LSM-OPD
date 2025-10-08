#include "LSM-OPD/sstable/RelVersion.h"
#include "LSM-OPD/db/DB.h"

namespace LSMOPD {
    RelVersion::RelVersion(DB *_db) : next(nullptr), epoch(0), next_epoch(-1), ref(2), db(_db) {
    }

    RelVersion::RelVersion(RelVersion *_prev, VersionEdit *edit, time_t time) : next(nullptr),
        epoch(std::max(_prev->epoch, time)), next_epoch(-1), size_entry(_prev->size_entry),
        db(_prev->db) {
        FileIndex = _prev->FileIndex;
        FileTotalSize = _prev->FileTotalSize;
        for (auto i: edit->EditFileList) {
            if (i->deletion) {
                auto x = FileIndex[i->level].begin();
                if(i->level == 0) {
                    while(x != FileIndex[i->level].end() && (*x)->file_id != i->file_id) {
                        x++;
                    }
                } else {
                    x = std::lower_bound(FileIndex[i->level].begin(),
                                              FileIndex[i->level].end(), i, RelFileCompare);
                }
                FileIndex[i->level].erase(x);
                FileTotalSize[i->level] -= i->file_size;
            } else {
                if (FileIndex.size() <= i->level)
                    FileIndex.resize(i->level + 5),
                    FileTotalSize.resize(i->level + 5);
                auto f = new RelFileMetaData(*i);
                if (i->level == 0) {
                    FileIndex[i->level].push_back(f);
                    FileTotalSize[i->level] += i->file_size;
                } else {
                    auto x = std::upper_bound(FileIndex[i->level].begin(),
                    FileIndex[i->level].end(), i, RelFileCompare);
                    FileIndex[i->level].insert(x, f);
                    FileTotalSize[i->level] += i->file_size;
                }
                f->parser = new RelFileParser<std::string>(db->ReaderCaches->find(f), db->options, f->file_size, f);
                if (FileIndex[i->level+1].empty())
                    f->no_overlap_in_next_level = true;
                else {
                    auto x = std::upper_bound(FileIndex[i->level + 1].begin(),
                        FileIndex[i->level + 1].end(), i, RelFileCompare);
                    if (x != FileIndex[i->level + 1].end()) {
                        if (!(f->key_max < (*x)->key_min))
                            f->no_overlap_in_next_level = false;
                    }
                    if (x != FileIndex[i->level + 1].begin()) {
                        x--;
                        if (!(f->key_min > (*x)->key_max))
                            f->no_overlap_in_next_level = false;
                    }
                }
                if (i->level !=0)
                {
                    auto x = std::upper_bound(FileIndex[i->level - 1].begin(),
                        FileIndex[i->level - 1].end(), i, RelFileCompare);
                    if (x != FileIndex[i->level - 1].begin()) 
                        x--;
                    for (;x != FileIndex[i->level - 1].end(); x++) {
                        if (!(f->key_min > (*x)->key_max || f->key_max < (*x)->key_min))
                            (*x)->no_overlap_in_next_level = false;
                        if (f->key_max < (*x)->key_min)
                            break;
                    }
                }
            }
        }
        if(FileIndex[0].size() > db->options->ZERO_LEVEL_FILES) {
            db->StallWrite(1);
        }
        else {
            db->ResumeWrite(1);
        }
        
        for (auto &i: FileIndex)
            for (auto &j: i)
                j->ref.fetch_add(1, std::memory_order_relaxed);
        _prev->next = this;
        _prev->next_epoch = epoch;
    }

    RelVersion::~RelVersion() {
        size_entry = nullptr;
        for (auto &i: FileIndex)
            for (auto &j: i) {
                auto r = j->ref.fetch_add(-1);
                if (r == 1) {
                    unlink((db->options->STORAGE_DIR + "/"
                            + j->file_name).c_str());
                    delete j->parser;
                    delete j->reader;
                    db->ReaderCaches->deletecache(j);
                    delete j;
                }
            }
    }

    RelCompaction<std::string> *RelVersion::GetCompaction(idx_t level) {
        // the level which added a new file
        if(level >= FileIndex.size())
            return NULL;
        RelCompaction<std::string> *c = NULL;
        std::vector<RelFileMetaData<std::string> *>down_file_meta;
        if (level == 0) {
            if (FileIndex[level].size() == 0)
                return c;
            if (FileIndex[level][0]->merging)
                return c;
            for (auto& x : FileIndex[level]) {
                if (!x->merging) {
                    down_file_meta.push_back(x);
                }
            }
        } else {
            if (level == db->options->MAX_LEVEL - 1) {
                return c;
            }
            if (FileIndex[level].size() <= util::fast_pow(db->options->LEVEL_SIZE_RITIO, level - 1) * 4) {
                return c;
            }
            bool all_merging = true;
            for (auto& x : FileIndex[level]) {
                if (!x->merging) {
                    all_merging = false;
                    break;
                }
            }
            if (all_merging) return c;
            int down_fileid = -1;
            for (size_t x = 0; x < FileIndex[level].size(); x++) {
                if (FileIndex[level][x]->no_overlap_in_next_level && !FileIndex[level][x]->merging) {
                    down_fileid = x;
                    break;
                }
            }
            if (down_fileid == -1) {
                do {
                    down_fileid = rand() % FileIndex[level].size();
                } while (FileIndex[level][down_fileid]->merging);
            }
            down_file_meta.push_back(FileIndex[level][down_fileid]);
        }

        std::string key_min = down_file_meta[0]->key_min;
        std::string key_max = down_file_meta[0]->key_max;
        for (auto& x : down_file_meta) {
            if (x->key_min < key_min)
                key_min = x->key_min;
            if (x->key_max > key_max)
                key_max = x->key_max;
        }
        //specialize the first down file
        if (FileIndex.size() == level + 1 || FileIndex[level + 1].size() == 0) {
            c = new RelCompaction<std::string>();
            c->key_min = key_min;
            c->target_level = level + 1;
            for (auto& x : down_file_meta) {
                c->file_list.push_back(x);
                x->merging = true;
            }
            return c;
        }

        //find the last file, whose key_min < current key_min
        auto iter1 = std::lower_bound(FileIndex[level + 1].begin(),
                                      FileIndex[level + 1].end(),
                                      std::make_pair(key_min, (idx_t) 0),
                                      RelFileCompareWithPair);
        if (iter1 != FileIndex[level + 1].begin()) iter1--;

        //find the first file, whose key_min > current key_max
        //thus, the file previous is the last one to compact
        auto iter2 = std::lower_bound(FileIndex[level + 1].begin(),
                                      FileIndex[level + 1].end(),
                                      std::make_pair(key_max, (idx_t) 0),
                                      RelFileCompareWithPair);
        if (iter2 != FileIndex[level + 1].end()) iter2++;
        for (auto i = iter1; i != iter2; ++i)
            if ((*i)->merging)
                return c;
        if (c == nullptr)
            c = new RelCompaction<std::string>();
        for (auto& x : down_file_meta) {
            c->file_list.push_back(x);
            x->merging = true;
        }
        c->key_min = min(key_min,(*iter1)->key_min);
        c->target_level = level + 1;
        for (auto i = iter1; i != iter2; ++i)
            if (!(*i)->merging) {
                c->file_list.push_back(*i);
                (*i)->merging = true;
            }
        return c;
    }

    bool RelVersion::AddRef() {
        idx_t k;
        do {
            k = ref.load();
            if (k == 0)
                return false;
        } while (!ref.compare_exchange_weak(k, k + 1));
        return true;
    }

    void RelVersion::DecRef() {
        auto k = ref.fetch_add(-1);
        if (k == 1)
            delete this;
    }

    void RelVersion::AddSizeEntry(std::shared_ptr<relMemTable> x) {
        if(x != nullptr)
            size_entry = x;
    }

    RelVersionIterator::RelVersionIterator(RelVersion *_version, std::string _key_min, std::string _key_max)
        : version(_version), key_min(_key_min), key_max(_key_max) {
        nextlevel();
    }

    RelFileMetaData<std::string> *RelVersionIterator::GetFile() const {
        if (end)
            return nullptr;
        return version->FileIndex[level][idx];
    }

    void RelVersionIterator::next() {
        if (end)
            return;
        if (level == 0 && idx < version->FileIndex[level].size() - 1) {
            for (++idx; idx < version->FileIndex[level].size(); idx++) {
                auto &x = version->FileIndex[level][idx];
                if (x->key_min > key_max
                    || x->key_max < key_min) {
                    continue;
                }
                return;
            }
        }
        if (idx == version->FileIndex[level].size() - 1 ||
            version->FileIndex[level][++idx]->key_min > key_max)
            nextlevel();
    }
    
    void RelVersionIterator::nextscankfile() {
        if (end)
            return;
        if (level == 0 && idx < version->FileIndex[level].size() - 1) {
            ++idx;
            return;
        }
        nextlevel();
    }

    void RelVersionIterator::nextlevel() {
        ++level;
        while (level < version->FileIndex.size() && !findsrc()) {
            ++level;
        }
        if (level == version->FileIndex.size())
            end = true;
    }

    bool RelVersionIterator::findsrc() {
        if (version->FileIndex[level].empty())
            return false;
        if (level == 0) {
            for (idx = 0; idx < version->FileIndex[level].size(); idx++) {
                auto &x = version->FileIndex[level][idx];
                if (x->key_min > key_max
                    || x->key_max < key_min) {
                    continue;
                }
                return true;
            }
            return false;
        }
        auto x = std::lower_bound(version->FileIndex[level].begin(),
                                  version->FileIndex[level].end(),
                                  std::make_pair(key_min, 0),
                                  RelFileCompareWithPair);
        if (x == version->FileIndex[level].begin())
        {
            if ((*x)->key_min <= key_max)
                idx = 0;
            else
                return false;
        }
        else if (x == version->FileIndex[level].end())
        {
            if(key_min <= (*(x - 1))->key_max)
                idx = x - version->FileIndex[level].begin() - 1;
            else
                return false;
        }
        else
        {
            if((*(x - 1))->key_max < key_min)
            {
                if ((*x)->key_min > key_max)
                    return false;
                else
                    idx = x - version->FileIndex[level].begin();
            }
            else
                idx = x - version->FileIndex[level].begin() - 1;
        }
        return true;
    }

    bool RelFileCompareWithPair(RelFileMetaData<std::string> *lhs, const std::pair<std::string, idx_t> &rhs) {
        return lhs->key_min < rhs.first;
    }

    bool RelFileCompareWithPairUpper(const std::pair<std::string, idx_t> &rhs, RelFileMetaData<std::string> *lhs) {
        return lhs->key_min < rhs.first;
    }

    bool RelFileCompare(RelFileMetaData<std::string> *lhs, RelFileMetaData<std::string> *rhs) {
        return lhs->key_min < rhs->key_min;
    }
}
