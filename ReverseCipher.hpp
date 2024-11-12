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
    ReverseCipher(std::string n) {
        name = n;
        features["flip"].b = true;
    };

    virtual std::string encode_with_features(std::string text, CipherFeatureMap &cfm) override;

    virtual std::string decode_with_features(std::string text, CipherFeatureMap &cfm) override;
};
