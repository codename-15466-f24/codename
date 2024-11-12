#include "ReverseCipher.hpp"

#include <vector>
#include <deque>
#include <unordered_map>
#include <map>
#include <iostream>

#include "string_parsing.hpp"
#include "ToggleCipher.hpp"

std::string ReverseCipher::encode_with_features(std::string text, CipherFeatureMap &cfm) {
    // I think every encoding/decoding function should look like this.
    // Basically, get the features you need, then use that information to actually do the cipher.
    bool flip = (cfm.find("flip") == cfm.end()) ? features["flip"].b : cfm["flip"].b;
    if (!flip) return text;

    std::string res = text;
    size_t start_to_flip = 0;
    bool in_alpha_part = false;
    for (size_t i = 0; i < text.length(); i++) {
        if (isalpha(text[i])) {
            if (!in_alpha_part) start_to_flip = i;
            in_alpha_part = true;
        }
        else {
            if (in_alpha_part) {
                for (size_t j = 0; j < i - start_to_flip; j++) {
                    res[i - j - 1] = text[start_to_flip + j];
                }
            }
            in_alpha_part = false;
        }
    }
    if (in_alpha_part) {
        for (size_t j = 0; j < text.length() - start_to_flip; j++) {
            res[text.length() - j - 1] = text[start_to_flip + j];
        }
    }
    return res;
}

std::string ReverseCipher::decode_with_features(std::string text, CipherFeatureMap &cfm) {
    return encode_with_features(text, cfm); // identical here
}