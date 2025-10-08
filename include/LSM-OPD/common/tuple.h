#pragma once
#include "LSM-OPD/utils/ConcurrentArray.h"
#include <string>
#include "LSM-OPD/utils/types.h"


namespace LSMOPD {

    typedef std::string tp_key;

    struct Tuple {

        std::vector <std::string> row;

        std::string GetRow(const int &col) {
            return row[col];
        }

        std::string GetKey() const {
            if (row.empty()) {
                return "";
            } else {
                return row[0];
            }
        }
    };
}
