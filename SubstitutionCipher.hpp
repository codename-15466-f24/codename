#pragma once

#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>

#include "ToggleCipher.hpp"

struct SubstitutionCipher : ToggleCipher {
    std::string default_alphabet = "abcdefghijklmnopqrstuvwxyz";

    SubstitutionCipher() {
        for (size_t i = 0; i < 26; i++) {
            features["substitution"].alphabet[i] = 'a' + (char)i;
        }
    };
    SubstitutionCipher(std::string input_name) {
        name = input_name;
        for (size_t i = 0; i < 26; i++) {
            features["substitution"].alphabet[i] = 'a' + (char)i;
        }
    };
    SubstitutionCipher(std::string input_name, std::string key) {
        name = input_name;
        for (size_t i = 0; i < 26; i++) {
            features["substitution"].alphabet[i] = key[i];
            default_alphabet[i] = key[i];
        }
    };

    virtual std::string encode_with_features(std::string text, CipherFeatureMap &cfm) override;

    virtual std::string decode_with_features(std::string text, CipherFeatureMap &cfm) override;

    virtual void reset_features() override;

    // is this a permutation of "abcdefghijklmnopqrstuvwxyz"?
    bool valid_permutation(std::string key);

    // assumes encoding of "abcdefghijklmnopqrstuvwxyz" -> "[key]"
    // constructs the decoder such that "abcdefghijklmnopqrstuvwxyz" -> "[decoder]" does the opposite
    std::string invert_alphabet(std::string key);

    virtual std::string cipher_type() override;
};
