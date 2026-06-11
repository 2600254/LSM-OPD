#pragma once
#include <set>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include "LSM-OPD/utils/types.h"

namespace LSMOPD {

    class OrderedDictionary {
    public:
        
        OrderedDictionary() = default;
        ~OrderedDictionary() = default;
        OrderedDictionary(const OrderedDictionary &other) = default;

        explicit OrderedDictionary(const std::vector<std::string>& data);

        void importData(const std::vector<std::string>& data);

        void importData(const std::string* data, const size_t size);

        int getMapping(const std::string& str) const;

        void CompressData(idx_t* data, const std::string* original, size_t size);

        std::string getString(int index) const;

        static OrderedDictionary merge(const OrderedDictionary& dict1, const OrderedDictionary& dict2);
    
        size_t serialize(std::string &result) const;

        void deserialize(const std::string &dict_serial, size_t strSize);
      
        int getCount() {
            return indexToString.size();
        }

		std::unordered_map<std::string, int> getAllMapping() const {
			return stringToIndex;
		}

		const std::vector<std::string>& getAllStrings() const {
			return indexToString;
		}

    private:
        
        std::unordered_map<std::string, int> stringToIndex;

        std::vector<std::string> indexToString;
    };


}