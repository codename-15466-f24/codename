#include "string_parsing.hpp"
#include "data_path.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

// usually delim will be " "
std::vector<std::string> split_by_delim(std::string &s, std::string delim) {
    // primarily adapted from https://stackoverflow.com/questions/14265581/parse-split-a-string-in-c-using-string-delimiter-standard-c
    
    std::vector<std::string> tokens;
    if (s.empty()) return tokens;

    // get rid of possible newline
    size_t len = s.length();
    if (s[len - 1] == '\n') {
        s.erase(len - 1);
    }

    size_t prev = 0, pos = 0;
    while (pos < len && prev < s.length()) {
        pos = s.find(delim, prev);
        if (pos == std::string::npos) pos = len;
        std::string token = s.substr(prev, pos - prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    }
    return tokens;
}

// usually delim will be " "
// The special character is assumed to be " itself (for quoted statements)
// I'm not offering additional compatibility here because we need to "escape" inner quotations
std::vector<std::string> parse_script_line(std::string &s, std::string delim) {
    std::vector<std::string> tokens;
    if (s.empty()) return tokens;

    // get rid of possible newline
    size_t len = s.length();
    if (s[len - 1] == '\n') {
        s.erase(len - 1);
    }

    if (s[len - 1] == '\r') {
        s.erase(len - 1);
    }

    size_t prev = 0, pos = 0;
    while (pos < len && prev < s.length()) {
        if (s[prev] == '"') {
            ++prev;
            pos = s.find('"', prev);
            while (pos != std::string::npos && pos >= 0 && s[pos - 1] == '\\') {
                // the backslash indicates escaping the quotation mark
                pos = s.find(delim, pos + 1);
            }
            if (pos == std::string::npos) {
                std::cerr << "warning: unclosed quotation" << std::endl;
                pos = len;
            }
            std::string token = s.substr(prev, pos - prev);
            if (!token.empty()) tokens.push_back(token);
            prev = pos + delim.length() + 1; // +1 for the quotation mark itself
        }
        else {
            pos = s.find(delim, prev);
            if (pos == std::string::npos) pos = len;
            std::string token = s.substr(prev, pos - prev);
            if (!token.empty()) tokens.push_back(token);
            prev = pos + delim.length();
        }
    }
    return tokens;
}

std::vector<std::string> lines_from_file(std::string &file) {
    std::ifstream infile(data_path("script/" + file));
    std::vector<std::string> lines;
    std::string next_line;
	while (std::getline(infile, next_line)) {
        lines.push_back(next_line);
    }
    return lines;
}