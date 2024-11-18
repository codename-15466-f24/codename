#include "CaesarCipher.hpp"

#include <vector>
#include <deque>
#include <unordered_map>
#include <map>
#include <iostream>

#include "string_parsing.hpp"
#include "ToggleCipher.hpp"

std::string CaesarCipher::encode_with_features(std::string text, CipherFeatureMap &cfm) {
    // I think every encoding/decoding function should look like this.
    // Basically, get the features you need, then use that information to actually do the cipher.
    int shift = (cfm.find("shift") == cfm.end()) ? features["shift"].i : cfm["shift"].i;

    std::string res = "";
    for (size_t i = 0; i < text.length(); i++) {
        if (text[i] >= 'a' && text[i] <= 'z') {
            res = res + alphabet[(text[i] - 'a' + shift) % 26];
        } else if (text[i] >= 'A' && text[i] <= 'Z') {
            char add = alphabet[(text[i] - 'A' + shift) % 26];
            add = add - 32;
            res = res + add;
        } else {
            res = res + text[i];
        }
    }
    return res;
}

std::string CaesarCipher::decode_with_features(std::string text, CipherFeatureMap &cfm) {
    int shift = (cfm.find("shift") == cfm.end()) ? features["shift"].i : cfm["shift"].i;
    CipherFeatureMap cfm_reverse;
    cfm_reverse["shift"].i = -shift;
    return encode_with_features(text, cfm_reverse); // identical here
}

void CaesarCipher::reset_features() {
    features["shift"].b = default_shift;
}