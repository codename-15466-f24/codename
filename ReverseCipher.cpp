#include "ReverseCipher.hpp"

#include <vector>
#include <deque>
#include <unordered_map>
#include <map>

#include "ToggleCipher.hpp"

std::string ReverseCipher::encode_with_features(std::string text, CipherFeatureMap &cfm) {
    // I think every encoding/decoding function should look like this.
    // Basically, get the features you need, then use that information to actually do the cipher.
    bool flip = (cfm.find("flip") == cfm.end()) ? features["flip"].b : cfm["flip"].b;
    if (!flip) return text;
    std::string res = text;
    for (size_t i = 0; i < text.length(); i++) res[text.length() - i - 1] = text[i];
    return res;
}

std::string ReverseCipher::decode_with_features(std::string text, CipherFeatureMap &cfm) {
    return encode_with_features(text, cfm); // identical here
}