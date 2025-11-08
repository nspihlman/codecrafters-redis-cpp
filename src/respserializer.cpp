#include <vector>
#include <string>
#include <sys/socket.h>
#include "respserializer.h"

std::string RespSerializer::simpleString(const std::string& str){
    return "+" + str + "\r\n";
}

std::string RespSerializer::bulkString(const std::string& str){
    if(str.size() == 0){ return "$-1\r\n";}
    return "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n";
}

std::string RespSerializer::array(const std::vector<std::string>& strs){
    std::string value = "*" + std::to_string(strs.size()) + "\r\n";
    for(int i = 0; i < strs.size(); ++i){
        value += bulkString(strs[i]);
    }
  return value;
}

std::string RespSerializer::integer(size_t value){
    return ":" + std::to_string(value) + "\r\n";
}

void RespSerializer::sendRespMessage(int fd, const std::string& resp){
    send(fd, resp.c_str(), resp.size(), 0);
    return;
}