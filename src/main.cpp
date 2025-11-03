#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <thread>
#include <vector>
#include <unordered_map>
#include <stack>
#include <cmath>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "user_set_value.h"


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

void process_set_message(int client_fd, std::vector<std::string>& commands, std::unordered_map<std::string, UserSetValue>& user_set_values){
  UserSetValue newValue = UserSetValue(commands[2]);
  if(commands.size() == 5 && commands[3] == "PX"){
    newValue.msExpiry = stoll(commands[4], NULL, 10);
  }
  else if (commands.size() == 5 && commands[3] == "EX"){
    newValue.msExpiry = stoll(commands[4], NULL, 10) * 1000;
  }
  user_set_values[commands[1]] = newValue;
  send(client_fd, "+OK\r\n", 5, 0);
}

void process_get_message(int client_fd, std::vector<std::string>& commands, std::unordered_map<std::string, UserSetValue>& user_set_values){
  if(user_set_values.find(commands[1]) == user_set_values.end()){
    send(client_fd, "$-1\r\n", 5, 0);
    return;
  }
  if(!user_set_values[commands[1]].stillValid()){
    user_set_values.erase(commands[1]);
    send(client_fd, "$-1\r\n", 5, 0);
    return;
  }
  std::string response = "$" + std::to_string(user_set_values[commands[1]].value.length()) + "\r\n" + user_set_values[commands[1]].value + "\r\n";
  send(client_fd, response.c_str(), response.length(), 0);
}

void process_rpush_message(int client_fd, std::vector<std::string>& commands){
  return;
}

void process_client_message(int client_fd, const char* buffer, size_t length, std::unordered_map<std::string, UserSetValue>& user_set_values){
    std::vector<std::string> commands = parser(buffer, length);
    if(commands[0] == "PING"){
      send(client_fd, "+PONG\r\n", 7, 0);
    }
    else if (commands[0] == "ECHO"){
      std::string response = "+" + commands[1] + "\r\n";
      send(client_fd, response.c_str(), response.length(), 0);
    }
    else if (commands[0] == "SET"){
      process_set_message(client_fd, commands, user_set_values);
    }
    else if (commands[0] == "GET"){
      process_get_message(client_fd, commands, user_set_values);
    }
    else if (commands[0] == "RPUSH"){
      process_rpush_message(client_fd, commands);
    }
}

void talk_to_client(int client_fd){
  char buffer[1024];
  std::unordered_map<std::string, UserSetValue> user_set_values;
  while(true){
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if(bytes_read <=0){
      close(client_fd);
      break;
    }
    process_client_message(client_fd, buffer, bytes_read, user_set_values);
  }
}

int main(int argc, char **argv) {
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(6379);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 6379\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  std::cout << "Waiting for a client to connect...\n";

  while(true){
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    std::cout << "Client connected\n";
    std::thread(talk_to_client, client_fd).detach();
    }
  close(server_fd);

  return 0;
}
