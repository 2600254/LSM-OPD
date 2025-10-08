#define CATCH_CONFIG_MAIN 

#include "LSM-OPD/LSM-OPD.h"
#include <iostream>
#include <vector>
#include <string>
#include "catch.hpp"
#include <gperftools/profiler.h>


std::string value_set[] = { "atarashi", "furui", "akai", "shiroi" };



using namespace LSMOPD;


TEST_CASE("FILE VALUE SCAN TEST", "[AP]") {
    {
        std::shared_ptr<Options> MockOptions = std::make_shared<Options>();
        MockOptions->MEM_TABLE_MAX_SIZE = 3354624;
        MockOptions->MAX_BLOCK_SIZE = 65536;
        // MockOptions->READ_BUFFER_SIZE = 64 * 1024;
        // MockOptions->WRITE_BUFFER_SIZE = 64 * 1024;
        // MockOptions->MAX_BLOCK_SIZE = 64 * 1024;

        DB x(MockOptions, 2);
        const int total = 1024 * 1024 * 64 - 5;
        const int num_threads = 8;
        const int chunk_size = total / num_threads;
        std::vector<std::thread> threads;
        for (int i = 0; i < num_threads; i++) {
            int start = i * chunk_size;
            int end = (i == num_threads - 1) ? total : start + chunk_size;
            threads.emplace_back([&](){
                for (int j = start; j < end; j++) {
                    Tuple t;
                    t.row.push_back(std::to_string(j));
                    t.row.push_back(std::to_string(j % (total / 100)));
                    auto y = x.BeginRelTransaction();
                    y.PutTuple(t, t.GetRow(0));
                }
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }

        std::cout << "\nFinished insert" << std::endl;

        while (x.Compacting()) { sleep(1); }
        for(int i = 0; i < 10; ++i)
        {
            auto z = x.BeginRelTransaction();
            auto begin = std::rand() % (total / 100);
            std::string a = std::to_string(begin), b = std::to_string(begin + 1);
            std::vector<Tuple> ans;
            ProfilerStart(std::string("APTest"+std::to_string(i)+".prof").c_str());
            z.GetTuplesFromRange(1, a, b, ans);
            ProfilerStop();
        }

    }
}
