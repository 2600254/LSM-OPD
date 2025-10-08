#pragma once

#include <algorithm>
#include <list>
#include <vector>
#include "RelFileMetaData.h"
#include "LSM-OPD/sstable/Compaction.h"
#include "LSM-OPD/memory/TupleEntry.h"

namespace LSMOPD
{
    class DB;
	struct VersionEdit
	{
		std::vector<RelFileMetaData<std::string>*> EditFileList;
	};
    class RelVersion
    {
    public:
        RelVersion(DB* _db);
        RelVersion(RelVersion* _prev, VersionEdit* edit, time_t time);
        ~RelVersion();

        RelCompaction<std::string>* GetCompaction(idx_t level);
        bool AddRef();
        void DecRef();
        void AddSizeEntry(std::shared_ptr < relMemTable > x);

        std::vector<      //level
            std::vector<
            RelFileMetaData<std::string>*>> FileIndex;
        RelVersion* next;
        time_t epoch;
        time_t next_epoch;

    private:
        std::vector<size_t> FileTotalSize;
        std::atomic<idx_t> ref = 1;
        std::shared_ptr < relMemTable > size_entry = NULL;
        DB* db;

        friend class RelVersionIterator;
        friend class rowMemoryManager;
        friend class ScanIter;
    };

    class RelVersionIterator
    {
    public:
        RelVersionIterator(RelVersion* _version, std::string _key_min, std::string _key_max);
        ~RelVersionIterator() = default;
        RelFileMetaData<std::string>* GetFile() const;
        bool End() const { return end; }
        void next();
        void nextscankfile();
        void nextlevel();
        idx_t GetLevel() const{
            return level;
        }
        idx_t GetIdx() const{
            return idx;
        }
    private:
        RelVersion* version;
        std::string key_min, key_max;
        idx_t level = -1;
        idx_t idx = 0;
        bool end = false;
        bool findsrc();
    };

    bool RelFileCompareWithPair(RelFileMetaData<std::string>* lhs, const std::pair<std::string, idx_t> &rhs);

    bool RelFileCompareWithPairUpper(const std::pair<std::string, idx_t>& rhs, RelFileMetaData<std::string>* lhs);

	bool RelFileCompare(RelFileMetaData<std::string>* lhs, RelFileMetaData<std::string>* rhs);
}
