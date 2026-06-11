#include "LSM-OPD/compress/ordered_dictionary.h"

namespace LSMOPD
{
    OrderedDictionary::OrderedDictionary(const std::vector<std::string>& data) {
		importData(data);
    }

    void OrderedDictionary::importData(const std::vector<std::string>& data) {

        std::set<std::string> uniqueStrings;

        uniqueStrings.insert(data.begin(), data.end());
        uniqueStrings.erase("");

        stringToIndex.clear();
        indexToString.clear();

        int index = 0;
        for (const auto& str : uniqueStrings) {
            stringToIndex[str] = index;
            indexToString.push_back(str);
            ++index;
        }
    }

    void OrderedDictionary::importData(const std::string* data, const size_t size) {

        std::set<std::string> uniqueStrings;
        for (size_t i = 0; i < size; ++i) {
            uniqueStrings.insert(data[i]);
        }
        uniqueStrings.erase("");

        stringToIndex.clear();
        indexToString.clear();

        int index = 0;
        for (const auto& str : uniqueStrings) {
            stringToIndex[str] = index;
            indexToString.push_back(str);
            ++index;
        }
    }


    int OrderedDictionary::getMapping(const std::string& str) const {
        auto it = stringToIndex.find(str);
        if (it != stringToIndex.end()) {
            return it->second;
        }
        return -1; 
    }

    std::string OrderedDictionary::getString(int index) const {
        if (index >= 0 && index < static_cast<int>(indexToString.size())) {
            return indexToString[index];
        }
        return ""; 
    }


    OrderedDictionary OrderedDictionary::merge(const OrderedDictionary& dict1, const OrderedDictionary& dict2) {
        OrderedDictionary mergedDict;

        auto it1 = dict1.indexToString.begin();
        auto it2 = dict2.indexToString.begin();

		mergedDict.stringToIndex.reserve(dict1.stringToIndex.size() + dict2.stringToIndex.size());

        while (it1 != dict1.indexToString.end() && it2 != dict2.indexToString.end()) {
            if (*it1 < *it2) {
                mergedDict.indexToString.push_back(*it1);
                ++it1;
            }
            else if (*it2 < *it1) {
                mergedDict.indexToString.push_back(*it2);
                ++it2;
            }
            else {
                mergedDict.indexToString.push_back(*it1);
                ++it1;
                ++it2;
            }
        }

        while (it1 != dict1.indexToString.end()) {
            mergedDict.indexToString.push_back(*it1);
            ++it1;
        }

        while (it2 != dict2.indexToString.end()) {
            mergedDict.indexToString.push_back(*it2);
            ++it2;
        }

        return mergedDict;
    }

    void OrderedDictionary::CompressData(idx_t* data, const std::string* original, size_t size) {
		for (size_t i = 0; i < size; ++i) {
			data[i] = getMapping(original[i]);
		}
        std::unordered_map<std::string, int>().swap(stringToIndex);
    }

    size_t OrderedDictionary::serialize(std::string &result) const {
        uint32_t size = static_cast<uint32_t>(indexToString.size());
        result.append(reinterpret_cast<const char*>(&size), sizeof(size));

        size_t maxlen = 0;
        for (const auto &str: indexToString) {
            maxlen = std::max(maxlen, str.size());
        }
        for (const auto &str: indexToString) {
            result.append(reinterpret_cast<const char*>(str.data()), str.size());
            if (str.size() < maxlen) {
                result.append(maxlen - str.size(), '\0');
            }
        }
        return maxlen;
    }

    void OrderedDictionary::deserialize(const std::string &data, size_t strSize) {
        size_t offset = 0;
        if (data.size() < sizeof(uint32_t)) {
            throw std::runtime_error("Invalid data: too short for size header");
        }
        uint32_t count;
        std::memcpy(&count, data.data() + offset, sizeof(count));
        offset += sizeof(count);

        indexToString.reserve(count);

        for (uint32_t i = 0; i < count; ++i) {
            indexToString.emplace_back(data.data() + offset, strSize);
            offset += strSize;
        }

        if (offset != data.size()) {
            throw std::runtime_error("Invalid data: extra bytes at end of buffer");
        }
    }

}// namespace LSMOPD