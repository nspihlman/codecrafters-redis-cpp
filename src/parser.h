#include <string>
#include <vector>

std::string parse_string(const char*& buffer);

std::vector<std::string> parse_array(const char*& buffer, size_t length);

std::vector<std::string> parser(const char*& buffer, size_t length);

