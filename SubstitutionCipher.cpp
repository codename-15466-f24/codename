#include "SubstitutionCipher.hpp"

#include <vector>
#include <deque>
#include <unordered_map>
#include <map>
#include <iostream>

#include "string_parsing.hpp"
#include "ToggleCipher.hpp"

std::string SubstitutionCipher::encode_with_features(std::string text, CipherFeatureMap &cfm) {
    // I think every encoding/decoding function should look like this.
    // Basically, get the features you need, then use that information to actually do the cipher.
    char key[26];
    if (cfm.find("substitution") == cfm.end()) {
        for (size_t i = 0; i < 26; i++) {
            key[i] = features["substitution"].alphabet[i];
        }
    } else {
        for (size_t i = 0; i < 26; i++) {
            key[i] = cfm["substitution"].alphabet[i];
        }
    }

    std::string res = "";
    for (size_t i = 0; i < text.length(); i++) {
        if (text[i] >= 'a' && text[i] <= 'z' && key[text[i] - 'a'] >= 'a' && key[text[i] - 'a'] <= 'z') {
            res = res + key[text[i] - 'a'];
        } else if (text[i] >= 'A' && text[i] <= 'Z' && key[text[i] - 'A'] >= 'a' && key[text[i] - 'A'] <= 'z') {
            char add = key[text[i] - 'A'];
            add = add - 32;
            res = res + add;
        } else {
            res = res + text[i];
        }
    }
    return res;
}

bool SubstitutionCipher::valid_permutation(std::string key) {
    int counts[26];
    for (char c: key) counts[c - 'a']++;
    for (size_t i = 0; i < 26; i++) {
        if (counts[i] != 1) return false;
    }
    return true;
}

std::string SubstitutionCipher::invert_alphabet(std::string key) {
    std::string res = "abcdefghijklmnopqrstuvwxyz";
    for (size_t i = 0; i < 26; i++) {
        if (0 <= key[i] - 'a' && key[i] - 'a' < 26)
            res[key[i] - 'a'] = 'a' + (char)i;
    }
    return res;
}

std::string SubstitutionCipher::decode_with_features(std::string text, CipherFeatureMap &cfm) {
    char key[27];
    if (cfm.find("substitution") == cfm.end()) {
        for (size_t i = 0; i < 26; i++) {
            key[i] = features["substitution"].alphabet[i];
        }
    } else {
        for (size_t i = 0; i < 26; i++) {
            key[i] = cfm["substitution"].alphabet[i];
        }
    }
    key[26] = '\0';
    std::string alpha(key, 26);
    std::string inv_key = invert_alphabet(alpha);
    CipherFeatureMap cfm_dec;
    for (size_t i = 0; i < 26; i++) {
        cfm_dec["substitution"].alphabet[i] = inv_key[i];
    }
    return encode_with_features(text, cfm_dec); // identical here
}

void SubstitutionCipher::reset_features() {
    for (size_t i = 0; i < 26; i++) {
        features["substitution"].alphabet[i] = default_alphabet[i];
    }
}

std::string SubstitutionCipher::cipher_type() {
    return "substitution";
}