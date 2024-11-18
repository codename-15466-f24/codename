#pragma once

#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>

#include "ToggleCipher.hpp"

struct CaesarCipher : ToggleCipher {
    int default_shift = 0;
    std::string alphabet = "abcdefghijklmnopqrstuvwxyz";

    CaesarCipher() {
        features["shift"].i = 0;
    };
    CaesarCipher(std::string n) {
        name = n;
        features["shift"].i = 0;
    };
    CaesarCipher(std::string n, int shift) {
        name = n;
        features["shift"].i = shift;
        default_shift = shift;
    };

    virtual std::string encode_with_features(std::string text, CipherFeatureMap &cfm) override;

    virtual std::string decode_with_features(std::string text, CipherFeatureMap &cfm) override;

    virtual void reset_features() override;
};
