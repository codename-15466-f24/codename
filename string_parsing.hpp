#pragma once

#include <string>
#include <vector>
#include <deque>

std::vector<std::string> split_by_delim(std::string &s, std::string delim);

std::vector<std::string> parse_script_line(std::string &s, std::string delim);

std::vector<std::string> lines_from_file(std::string &file);