#pragma once

#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>

#include "ToggleCipher.hpp"

struct ReverseCipher : ToggleCipher {
    ReverseCipher() {
        features["flip"].b = true;
    };

    std::string encode_with_features(std::string text, CipherFeatureMap &cfm);

    std::string decode_with_features(std::string text, CipherFeatureMap &cfm);
};
