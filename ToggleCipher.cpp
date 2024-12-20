#include <vector>
#include <deque>
#include <unordered_map>
#include <map>

#include "ToggleCipher.hpp"

std::string ToggleCipher::encode(std::string text) {return encode_with_features(text, blank);}
std::string ToggleCipher::decode(std::string text) {return decode_with_features(text, blank);}

void ToggleCipher::set_feature(std::string key, CipherFeature val) {
    if (features.find(key) != features.end()) features[key] = val;
};

CipherFeature ToggleCipher::get_feature(std::string key) {
    if (features.find(key) != features.end()) return features[key];
    CipherFeature empty;
    empty.b = 0;
    return empty;
};

void ToggleCipher::reset_features() {
    for (auto &[k, v] : features) {
        features[k].i = 0;
        features[k].b = false;
        for (size_t i = 0; i < 26; i++) features[k].alphabet[i] = '\0';
    }
}

std::string ToggleCipher::cipher_type() {
    return "default";
}

// std::string SubstitutionCipher::encode_with_features(std::string text, CipherFeatureMap &cfm) {
//     // I think every encoding/decoding function should look like this.
//     // Basically, get the features you need, then use that information to actually do the cipher.
//     std::string substitution{
//         (const char*)(cfm.find("substitution") == cfm.end() ? &features["substitution"].alphabet : &cfm["substitution"].alphabet), 26};
//     std::string out;
//     for (int i = 0; i < text.length(); i++){
//         if (text[i] >= 'a' && text[i] <= 'z') {
//             out = out + substitution[text[i] - 'a'];
//         } else if (text[i] >= 'A' && text[i] <= 'Z') {
//             char add = substitution[text[i] - 'A'];
//             add = add - 32;
//             out = out + add;
//         } else {
//             out = out + text[i];
//         }
//     }
//     return out;
// }