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
#include <algorithm>
#include <netdb.h>
#include "user_data.h"
#include "parser.h"

void send_user_list_length(int client_fd, std::string list_name, UserData& user_data){
  std::string list_length= ":" + std::to_string(user_data.user_lists[list_name].size()) + "\r\n";
  send(client_fd, list_length.c_str(), list_length.size(), 0);
  return;
}

void send_bulk_string(int client_fd, std::string str){
  if(str.size() == 0){
    send(client_fd, "$-1\r\n", 5, 0);
    return;
  }
  std::string response = "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n";
  send(client_fd, response.c_str(), response.size(), 0);
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

void process_rpush_message(int client_fd, std::vector<std::string>& commands, UserData& user_data){
  if(user_data.user_lists.find(commands[1]) == user_data.user_lists.end()){
    user_data.user_lists[commands[1]] = {};
  }
  for(int i = 2; i < commands.size(); ++i){
    user_data.user_lists[commands[1]].push_back(commands[i]);
  }
  send_user_list_length(client_fd, commands[1], user_data);
}

int_fast64_t convert_indexes(int index, size_t list_length){
  if(index >= 0) {
    return index;
  }
  int positive_index = list_length + index;
  if(positive_index < 0){
    return 0;
  }
  return positive_index;
}

void process_lrange_message(int client_fd, std::vector<std::string>& commands, UserData& user_data){
  if(user_data.user_lists.find(commands[1]) == user_data.user_lists.end()){
    send(client_fd, "*0\r\n", 4, 0);
    return;
  }
  int start = convert_indexes(stoll(commands[2]), user_data.user_lists[commands[1]].size());
  int stop = convert_indexes(stoll(commands[3]), user_data.user_lists[commands[1]].size());
  if(user_data.user_lists.find(commands[1]) == user_data.user_lists.end() || start >= user_data.user_lists[commands[1]].size() || start > stop){
    send(client_fd, "*0\r\n", 4, 0);
    return;
  }
  if(stop >= user_data.user_lists[commands[1]].size()){
    stop = user_data.user_lists[commands[1]].size() - 1;
  }
  std::string response = "*" + std::to_string(stop - start + 1) + "\r\n";
  for(start; start <= stop; ++start){
    response += "$" + std::to_string(user_data.user_lists[commands[1]][start].size()) + "\r\n";
    response += user_data.user_lists[commands[1]][start] + "\r\n";
  }
  send(client_fd, response.c_str(), response.size(), 0);
}

void process_lpush_message(int client_fd, std::vector<std::string>& commands, UserData& user_data){
  if(user_data.user_lists.find(commands[1]) == user_data.user_lists.end()){
    user_data.user_lists[commands[1]] = {};
  }
  for(int i = 2; i < commands.size(); ++i){
    user_data.user_lists[commands[1]].push_front(commands[i]);
  }
  send_user_list_length(client_fd, commands[1], user_data);
}

void process_lpop_message(int client_fd, std::vector<std::string>& commands, UserData& user_data){
  if(user_data.user_lists.find(commands[1]) == user_data.user_lists.end() || user_data.user_lists[commands[1]].size() == 0){
    send_bulk_string(client_fd, "");
    return;
  }
  send_bulk_string(client_fd, user_data.user_lists[commands[1]].front());
  user_data.user_lists[commands[1]].pop_front();
}

void process_client_message(int client_fd, const char* buffer, size_t length, UserData& user_data){
    std::vector<std::string> commands = parser(buffer, length);
    if(commands[0] == "PING"){
      send(client_fd, "+PONG\r\n", 7, 0);
    }
    else if (commands[0] == "ECHO"){
      std::string response = "+" + commands[1] + "\r\n";
      send(client_fd, response.c_str(), response.length(), 0);
    }
    else if (commands[0] == "SET"){
      process_set_message(client_fd, commands, user_data.user_set_values);
    }
    else if (commands[0] == "GET"){
      process_get_message(client_fd, commands, user_data.user_set_values);
    }
    else if (commands[0] == "RPUSH"){
      process_rpush_message(client_fd, commands, user_data);
    }
    else if (commands[0] == "LRANGE"){
      process_lrange_message(client_fd, commands, user_data);
    }
    else if (commands[0] == "LPUSH"){
      process_lpush_message(client_fd, commands, user_data);
    }
    else if(commands[0] == "LLEN") {
      send_user_list_length(client_fd, commands[1], user_data);
    }
    else if(commands[0] == "LPOP"){
      process_lpop_message(client_fd, commands, user_data);
    }
}

void talk_to_client(int client_fd){
  char buffer[1024];
  UserData user_data;
  while(true){
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if(bytes_read <=0){
      close(client_fd);
      break;
    }
    process_client_message(client_fd, buffer, bytes_read, user_data);
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
