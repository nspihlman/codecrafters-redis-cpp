#ifndef RESPSRL_H
#define RESPSRL_H

#include <string>
#include <vector>

class RespSerializer {
public:
  static std::string simpleString(const std::string& str);
  static std::string bulkString(const std::string& str);
  static std::string simpleError(const std::string& str);
  static std::string array(const std::vector<std::string>& strs);
  static std::string integer(size_t value);
  static void sendRespMessage(int fd, const std::string& resp);
};


#endif