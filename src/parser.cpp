#include "parser.h"
#include <vector>

std::string parse_string(const char*& buffer){
//string_start is the index in the buffer that the $ is found
  char* end;
  long string_length = strtol(buffer+1, &end, 10);
  const char* stringStart = end + 2;
  buffer = stringStart + string_length + 2; // now buffer is pointing to the next $ if there is one, or a nullptr
  return std::string(stringStart, string_length);
}


std::vector<std::string> parse_array(const char*& buffer, size_t length){
  char* end;
  std::vector<std::string> arrayValues;
  long array_len = std::strtol(buffer+1, &end, 10);
  buffer = end + 2; // buffer now points to the first $
  while(array_len > 0){
    --array_len;
    arrayValues.push_back(parse_string(buffer));
  }
  return arrayValues;
}

std::vector<std::string> parser(const char*& buffer, size_t length){
  if (buffer[0] == '*'){
    return parse_array(buffer, length);
  }
  if (buffer[0] == '$'){
    return {parse_string(buffer)};
  }
  return {""};
}