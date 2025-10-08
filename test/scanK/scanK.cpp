
#include "LSM-OPD/LSM-OPD.h"
#include <iostream>
#include <vector>
#include "catch.hpp"


std::string value_set[] = {"atarashi", "furui", "akai", "shiroi"};


using namespace LSMOPD;

int main() 
{
    for (auto &i: value_set) {
        i.resize(8, ' ');
    }
    std::shared_ptr<Options> MockOptions = std::make_shared<Options>();
    MockOptions->MEM_TABLE_MAX_SIZE = 4 * 1024;
    MockOptions->READ_BUFFER_SIZE = 64 * 1024;
    MockOptions->WRITE_BUFFER_SIZE = 64 * 1024;
    MockOptions->MAX_BLOCK_SIZE = 4 * 1024;

    DB x(MockOptions, 2);

    {
        std::vector<std::string> ans_sheet;
        for (int i = 0; i < 1024 * 32; i++) {
            Tuple t;
            int k = i & 3;
            std::string tmp = std::to_string(i);
            tmp.resize(16);
            t.row.push_back(tmp);
            t.row.push_back(value_set[k]);
            auto y = x.BeginRelTransaction();
            y.PutTuple(t, t.GetRow(0));
        }

        sleep(5);
        std::cout << "\nFinished insert" << std::endl;
        auto z = x.BeginRelTransaction();
        auto res = z.GetIter("3");
        for (int i = 0; i < 100; i++) {
            Tuple result;
            res.GetTuple(result);
            res.next();
            std::cout << result.GetKey() << std::endl;
        }
        std::cout << std::endl << "======================" << std::endl;
        auto z2 = x.BeginRelTransaction();
        res = z2.GetIter("3");
        for (int i = 0; i < 100; i++) {
            Tuple result;
            res.GetTuple(result);
            res.next();
            std::cout << result.GetKey() << std::endl;
        }
        auto z3 = x.BeginRelTransaction();
        res = z3.GetIter("");
        int cnt = 0;
        while (!res.end()) {
            Tuple result;
            res.GetTuple(result);
            if (result.row[1] == value_set[1]) cnt++;
            res.next();
        }
        std::cout << cnt << std::endl;
    }
}
