#pragma once
#include<unordered_map>
#include<string>
#include "LSM-OPD/common/tuple.h"


namespace LSMOPD {
    struct AnswerMerger {
        AnswerMerger() = default;
        ~AnswerMerger() = default;
        // if update = 0, the current Tuple will be eliminated, otherwise, it will replace the answer
        bool has_answer(const std::string &key) const {
            return answers.contains(key);
        }
        void insert_answer(const std::string &key, Tuple &&x, bool update = false) {
            if(answers.contains(key)) {
                if(update) {
                    answers[key] = x;
                }
                return;
            }
            answers[key] = x;
        }

        std::unordered_map<std::string, Tuple> answers;
    };
}
