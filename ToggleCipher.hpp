#pragma once

#include <vector>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <string>

union CipherFeature {
    int i;
    bool b;
    char alphabet[26];
};

typedef std::unordered_map<std::string, CipherFeature> CipherFeatureMap;

// Cipher that allows for toggling features on and off
struct ToggleCipher {
    CipherFeatureMap features; // The cipher keeps a specific feature setting.
    std::string name;
    ToggleCipher() {name = "";};
    ToggleCipher(std::string n) {name = n;};

    // If you were to want to encode/decode with your own feature settings, you can do that here.
    // Any nonspecified features use whatever is in the features map, at least in theory.
    virtual std::string encode_with_features(std::string text, CipherFeatureMap &cfm) {return text;};
    virtual std::string decode_with_features(std::string text, CipherFeatureMap &cfm) {return text;};

    // These encode/decode normally using the given feature settings.
    CipherFeatureMap blank;
    virtual std::string encode(std::string text);
    virtual std::string decode(std::string text);

    // Lets you change features.
    virtual void set_feature(std::string key, CipherFeature val);
    virtual CipherFeature get_feature(std::string key);
    virtual void reset_features();
};

// struct SubstitutionCipher : ToggleCipher {
//     SubstitutionCipher() {
//         std::string alpha = "abcdefghijklmnopqrstuvwxyz";
//         for (int i = 0; i < 26; i++) features["substitution"].alphabet[i] = alpha[i];
//     };

//     std::string encode_with_features(std::string text, CipherFeatureMap &cfm);

//     std::string decode_with_features(std::string text, CipherFeatureMap &cfm);
// };