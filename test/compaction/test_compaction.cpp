#define CATCH_CONFIG_MAIN 

#include "LSM-OPD/LSM-OPD.h"
#include <iostream>
#include <vector>
#include "catch.hpp"
#include <gperftools/profiler.h>

std::string value_set[] = {"atarashi", "furui", "ckai", "shiroi", "ccc", "ddd", "eee", "fff", "ggg", "hhh"};


using namespace LSMOPD;

std::shared_ptr<Options> MockOptions = std::make_shared<Options>();
DB x(MockOptions, 2);
void worker(int start, int end, std::vector<std::string>& ans_sheet) {
    for (int i = start; i < end; i++) {
        Tuple t;
        int k = i % 9;
        std::string tmp = std::to_string(i);
        tmp.resize(16);
        t.row.push_back(tmp);
        t.row.push_back(value_set[k]);

        { auto y = x.BeginRelTransaction(); y.PutTuple(t, t.GetRow(0)); }
        { auto y = x.BeginRelTransaction(); y.PutTuple(t, t.GetRow(0)); }

        ans_sheet[i] = value_set[k];
    }
}

void checker(int start, int end, std::vector<std::string>& ans_sheet) {
    for (int i = start; i < end; i++) {
        auto z = x.BeginReadOnlyRelTransaction();
        std::string tmp = std::to_string(i);
        tmp.resize(16);
        auto t = z.GetTuple(tmp);
        REQUIRE(std::string(t.GetRow(0).c_str()) == std::to_string(i));
        REQUIRE(std::string(t.GetRow(1).c_str()) == ans_sheet[i]);
    }
}

TEST_CASE("FILE IO Test", "[compaction]") {
    for (auto &i: value_set) {
        i.resize(8, ' ');
    }
    ProfilerStart("fileIOTest.prof");

    // MockOptions->MEM_TABLE_MAX_SIZE = 4 * 1024;
    // MockOptions->READ_BUFFER_SIZE = 64 * 1024;
    // MockOptions->WRITE_BUFFER_SIZE = 64 * 1024;
    // MockOptions->MAX_BLOCK_SIZE = 4 * 1024;

    std::vector<std::string> ans_sheet;
    const int total = 1024 * 1024 * 32 - 5;
    ans_sheet.resize(total);  // 预分配空间

    const int num_threads = 8;
    const int chunk_size = total / num_threads;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; i++) {
        int start = i * chunk_size;
        int end = (i == num_threads - 1) ? total : start + chunk_size;
        threads.emplace_back(worker, start, end, std::ref(ans_sheet));
    }

    for (auto& t : threads) {
        t.join();
    }
    threads.clear();
    // std::vector<std::string> ans_sheet;
    // for (int i = 0; i < 1024 * 1024 * 64 - 5; i++) {
    //     Tuple t;
    //     int k = i % 9;
    //     std::string tmp = std::to_string(i);
    //     tmp.resize(16);
    //     t.row.push_back(tmp);
    //     t.row.push_back(value_set[k]);
    //
    //     //{
    //     //    t.row.push_back(value_set[k]);
    //     //    t.row.push_back(value_set[k]);
    //     //    t.row.push_back(value_set[k]);
    //     //    t.row.push_back(value_set[k]);
    //     //    t.row.push_back(value_set[k]);
    //     //    t.row.push_back(value_set[k]);
    //     //    t.row.push_back(value_set[k]);
    //     //    t.row.push_back(value_set[k]);
    //     //    t.row.push_back(value_set[k]);
    //     //}
    //
    //
    //     {auto y = x.BeginRelTransaction();
    //     y.PutTuple(t, t.GetRow(0));
    //     }
    //     {auto y = x.BeginRelTransaction();
    //     y.PutTuple(t, t.GetRow(0));
    //     }
    //     ans_sheet.push_back(value_set[k]);
    // }

    //for (int i = 0; i < 1024 * 32; i++) {
    //    Tuple t;
    //    int k = rand() & 3;
    //    t.row.push_back(std::to_string(i));
    //    t.row.push_back(value_set[k]);
    //    auto y = x.BeginRelTransaction();
    //    y.PutTuple(t, t.GetRow(0), 1.0);
    //    ans_sheet.push_back(value_set[k]);
    //}
    //sleep(5);
    while (x.Compacting()) {
    	//std::cout << "Compacting..." << std::endl;
    	std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "\nFinished insert" << std::endl;
    
    for (int i = 0; i < num_threads; i++) {
        int start = i * chunk_size;
        int end = (i == num_threads - 1) ? total : start + chunk_size;
        threads.emplace_back(checker, start, end, std::ref(ans_sheet));
    }
    for (auto& t : threads) {
        t.join();
    }

    // std::cout<<k.GetTuple("0").col_num<<std::endl;
    // for (int i = 0; i < 64; i++) {
    //     z.GetTuplesFromRange(1, "a", "b");
    // }
    ProfilerStop();
}
