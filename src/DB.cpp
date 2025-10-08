#include "LSM-OPD/db/DB.h"
#include <cassert>
#include "LSM-OPD/db/Transaction.h"

namespace LSMOPD {
    DB::DB(std::shared_ptr<Options> _options, idx_t column_num) : options(_options),
                                                                  epoch_id(1),
                                                                  write_epoch_table(_options->MAX_WORKER_THREAD)
#ifdef RUN_PROFILER
                                                                  ,compaction_profilers_(_options->NUM_OF_HIGH_COMPACTION_THREAD + _options->NUM_OF_LOW_COMPACTION_THREAD)
#endif
                                                                  {
        if (options->MAX_FILE_READER_CACHE_SIZE != 0) {
            ReaderCaches = std::make_unique<FileReaderCache>(
                options->MAX_FILE_READER_CACHE_SIZE, options->STORAGE_DIR + "/");
        } else {
            struct rlimit rlim;
            if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
                perror("getrlimit failed");
                exit(-1);
            }
            rlim.rlim_cur = rlim.rlim_max;
            if (setrlimit(RLIMIT_NOFILE, &rlim) != 0) {
                perror("setrlimit failed");
                exit(-1);
            }
            ReaderCaches = std::make_unique<FileReaderCache>(0, options->STORAGE_DIR + "/");
        }
        RowMemtable = std::make_unique<rowMemoryManager>(this, column_num);
        relFiles = std::make_unique<RelFileManager>(this);
        read_rel_version = current_rel_version = new RelVersion(this);
        for (idx_t i = 0; i < options->NUM_OF_HIGH_COMPACTION_THREAD; ++i) {
            high_compact_thread.push_back(std::make_shared<std::thread>(
                [this, i] { 
                    PROFILER_CONTEXT_THREAD(compaction_profilers_[i], HighCompactLoop);
                }));
        }

        for (idx_t i = 0; i < options->NUM_OF_LOW_COMPACTION_THREAD; ++i) {
            low_compact_thread.push_back(std::make_shared<std::thread>(
                [this, i] { 
					PROFILER_CONTEXT_THREAD(compaction_profilers_[options->NUM_OF_HIGH_COMPACTION_THREAD + i], LowCompactLoop);
                }));
        }
    }

    DB::~DB() {
        close = true;
        if (relFiles != nullptr) {
            relFiles->HighCompactionCV.notify_all();
            for (auto &i: high_compact_thread)
                if (i->joinable())
                    i->join();
            relFiles->LowCompactionCV.notify_all();
            for (auto &i: low_compact_thread)
                if (i->joinable())
                    i->join();
            read_rel_version.load()->DecRef();
            current_rel_version->DecRef();
        }
#ifdef RUN_PROFILER
        for (auto profiler : compaction_profilers_) {
			db_profiler.AddThreadProfiler(profiler);
        }
        db_profiler.PrintSummary();
#endif
        //std::cout << "closed" << std::endl;
    }

    Transaction DB::BeginRelTransaction() {
        time_t local_write_epoch_id = epoch_id.fetch_add(1, std::memory_order_relaxed);
        size_t pos = write_epoch_table.insert(local_write_epoch_id);
        time_t local_read_epoch_id;
        local_read_epoch_id = write_epoch_table.find_min() - 1;
        return Transaction(local_write_epoch_id, local_read_epoch_id, this,
                           get_read_rel_version(), pos);
    }

    Transaction DB::BeginReadOnlyRelTransaction() {
        time_t local_epoch_id = get_read_time();
        return Transaction(MAXTIME, local_epoch_id, this, get_read_rel_version());
    }

    void DB::HighCompactLoop() {
        GET_THREAD_PROFILER();
        while (true) {
            if (close)
                return;
            TryCompaction(0);
            std::unique_lock<std::mutex> lock(relFiles->HighCompactionCVMutex);
            if (!relFiles->HighCompactionList.empty()) {
                working_compact_thread.fetch_add(1, std::memory_order_relaxed);
                RelCompaction<std::string> x(relFiles->HighCompactionList.front());
                relFiles->HighCompactionList.pop();
                lock.unlock();
                VersionEdit *edit;
                time_t time = 0;
                x.file_id = relFiles->GetFileID();
                if (x.relPersistence != nullptr) {
                    //persistence
                    START_OPERATOR_PROFILER();
                    edit = RowMemtable->RowMemtablePersistence(x.file_id, x.relPersistence);
					END_LOCAL_OPERATOR_PROFILER("RowMemtablePersistence");
                    time = x.relPersistence->max_time;
                } else {
                    START_OPERATOR_PROFILER();
                    edit = relFiles->MergeRelFile(x);
                    END_LOCAL_OPERATOR_PROFILER("MergeRelFile");
                }
                ProgressRelVersion(edit, time, x.relPersistence);
                delete edit;
                working_compact_thread.fetch_add(-1, std::memory_order_relaxed);
                ProgressReadRelVersion();
            } else {
                relFiles->HighCompactionCV.wait_for(lock, std::chrono::milliseconds(200));
            }
        }
    }
    
    void DB::LowCompactLoop() {
        GET_THREAD_PROFILER();
        while (true) {
            if (close)
                return;
            TryCompaction(rand() % std::max(current_rel_version->FileIndex.size(), (size_t)1));
            std::unique_lock<std::mutex> lock(relFiles->LowCompactionCVMutex);
            if (!relFiles->LowCompactionList.empty()) {
                working_compact_thread.fetch_add(1, std::memory_order_relaxed);
                RelCompaction<std::string> x(relFiles->LowCompactionList.front());
                relFiles->LowCompactionList.pop();
                lock.unlock();
                VersionEdit *edit;
                x.file_id = relFiles->GetFileID();
                START_OPERATOR_PROFILER();
                edit = relFiles->MergeRelFile(x);
                END_LOCAL_OPERATOR_PROFILER("MergeRelFile");

                ProgressRelVersion(edit, 0, x.relPersistence);
                delete edit;
                working_compact_thread.fetch_add(-1, std::memory_order_relaxed);
                ProgressReadRelVersion();
            } else {
                relFiles->LowCompactionCV.wait_for(lock, std::chrono::milliseconds(200));
            }
        }
    }

    void DB::TryCompaction(idx_t level) {
        std::unique_lock<std::mutex> version_lock(version_mutex);
        auto compact = current_rel_version->GetCompaction(
            level);
        if (compact != NULL) {
            relFiles->AddCompaction(*compact);
            delete compact;
        }
        version_lock.unlock();
    }

    void DB::ProgressRelVersion(VersionEdit *edit, time_t time,
                                std::shared_ptr<relMemTable> size, bool force_leveling) {
        std::unique_lock<std::mutex> version_lock(version_mutex);
        RelVersion *tmp = current_rel_version;
        tmp->AddSizeEntry(size);
        current_rel_version = new RelVersion(tmp, edit, time);
        auto compact = current_rel_version->GetCompaction(edit->EditFileList[0]->level);
        if (compact != NULL) {
            relFiles->AddCompaction(*compact, compact->target_level == 1);
            delete compact;
        }
        version_lock.unlock();
    }

    void DB::ProgressReadRelVersion() {
        while (true) {
            time_t nrt = get_read_time();
            RelVersion *tail = read_rel_version.load();
            if (tail->next_epoch == (time_t) -1 || tail->next_epoch >= nrt)
                break;
            if (read_rel_version.compare_exchange_weak(tail, tail->next)) {
                tail->DecRef();
            }
        }
    }

    void DB::StallWrite(int memtable) {
        write_stall[memtable].store(true);
    }
	void DB::ResumeWrite(int memtable) {
        write_stall[memtable].store(false);
        std::unique_lock<std::mutex> lock(write_stall_mutex);
        write_stall_cv.notify_all();
    }

    time_t DB::get_read_time() {
        if (write_epoch_table.empty())
            return epoch_id.load(std::memory_order_acquire);
        else
            return write_epoch_table.find_min() - 1;
    }

    RelVersion *DB::get_read_rel_version() {
        RelVersion *version;
        while (true) {
            version = read_rel_version.load();
            if (version->AddRef())
                return version;
        }
    }
    
    void DB::check_write_stall() {
        if (write_stall[0].load() || write_stall[1].load()) {
            std::unique_lock<std::mutex> lock(write_stall_mutex);
            write_stall_cv.wait(lock, [&] { return !write_stall[0].load() && !write_stall[1].load(); });
        }
    }
}
