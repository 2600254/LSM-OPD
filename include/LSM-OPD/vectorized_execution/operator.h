#pragma once
#include "LSM-OPD/compress/ordered_dictionary.h"
#include "LSM-OPD/utils/types.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>

namespace LSMOPD {

    template <typename Func>
    void onesiderange(OrderedDictionary* dict, const idx_t* data, int count, Func* check, sul::dynamic_bitset<>& result) {
        
        bool f = check(dict->getString(0));
        int l=0, r=dict->getCount()-1;
        while(l<=r) {
            int mid=(l+r)>>1;
            if (check(dict->getString(mid)) == f) l = mid+1;
            else r = mid-1;
        }
        
        for(int i=0; i<count; i++){
            result[i] = (!((data[i]<l)^f));
        }
    }

    template <typename Func>
    void rangefilter(OrderedDictionary* dict, const idx_t* data, int count, Func left_bound, Func right_bound, sul::dynamic_bitset<>& result) {

        const std::vector<std::string>& strvec = dict->getAllStrings();
        auto l_it = std::lower_bound(strvec.begin(), strvec.end(), left_bound);
		auto r_it = std::upper_bound(strvec.begin(), strvec.end(), right_bound);

		idx_t LeftBound = l_it - strvec.begin();
		idx_t RightBound = r_it - strvec.begin() - 1;

        if (RightBound < 0 || LeftBound >= (idx_t)dict->getCount()) {
            for (int i = 0; i < count; i++) {
				result[i] = false;
            }
            return;
        }

        for(int i=0; i<count; i++){
            result[i] = (LeftBound<=data[i] && data[i]<=RightBound);
        }
    }

    template<typename Func>
    void RangeFilter(Vector &res, Func left_bound, Func right_bound) {
        idx_t cnt = res.GetCount();
        sul::dynamic_bitset<>res_bitmap(2048);
        rangefilter(res.GetDict(), res.GetData(), cnt, left_bound, right_bound, res_bitmap);
        res.Slice(res_bitmap);
    }

}