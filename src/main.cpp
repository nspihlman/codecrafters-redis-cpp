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
#include "respserializer.h"
#include "lockmanager.h"



void process_set_message(int client_fd, std::vector<std::string>& commands, std::unordered_map<std::string, UserSetValue>& user_set_values){
  UserSetValue newValue = UserSetValue(commands[2]);
  if(commands.size() == 5 && commands[3] == "PX"){
    newValue.msExpiry = stoll(commands[4], NULL, 10);
  }
  else if (commands.size() == 5 && commands[3] == "EX"){
    newValue.msExpiry = stoll(commands[4], NULL, 10) * 1000;
  }
  user_set_values[commands[1]] = newValue;
  RespSerializer::sendRespMessage(client_fd, "+OK\r\n");
}

void process_get_message(int client_fd, std::vector<std::string>& commands, std::unordered_map<std::string, UserSetValue>& user_set_values){
  if(user_set_values.find(commands[1]) == user_set_values.end()){
    RespSerializer::sendRespMessage(client_fd, RespSerializer::bulkString(""));
    return;
  }
  if(!user_set_values[commands[1]].stillValid()){
    user_set_values.erase(commands[1]);
    RespSerializer::sendRespMessage(client_fd, RespSerializer::bulkString(""));
    return;
  }
  RespSerializer::sendRespMessage(client_fd, RespSerializer::bulkString(user_set_values[commands[1]].value));
}

void process_rpush_message(int client_fd, std::vector<std::string>& commands, UserData& user_data, LockManager& lockmanager){
  std::string key = commands[1];
  auto lock = lockmanager.get_user_lists_lock(key);
  std::unique_lock<std::mutex> guard(*lock);
  if(user_data.user_lists.find(key) == user_data.user_lists.end()){
    user_data.user_lists[key] = {};
  }
  for(int i = 2; i < commands.size(); ++i){
    user_data.user_lists[key].push_back(commands[i]);
  }
  if(!user_data.blocked_clients[key].empty()){
    auto blocked_client = user_data.blocked_clients[key].front();
    user_data.blocked_clients[key].pop_front();

    blocked_client->ready = true;
    blocked_client->cv.notify_one();
  }
  RespSerializer::sendRespMessage(client_fd, RespSerializer::integer(user_data.user_lists[key].size()));
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

void process_lrange_message(int client_fd, std::vector<std::string>& commands, UserData& user_data, LockManager& lockmanager){
  std::string key = commands[1];
  auto lock = lockmanager.get_user_lists_lock(key);
  std::unique_lock<std::mutex> guard(*lock);
  if(user_data.user_lists.find(key) == user_data.user_lists.end()){
    RespSerializer::sendRespMessage(client_fd, RespSerializer::array({}));
    return;
  }

  int start = convert_indexes(stoll(commands[2]), user_data.user_lists[key].size());
  int stop = convert_indexes(stoll(commands[3]), user_data.user_lists[key].size());
  
  if(user_data.user_lists.find(key) == user_data.user_lists.end() || start >= user_data.user_lists[key].size() || start > stop){
    RespSerializer::sendRespMessage(client_fd, RespSerializer::array({}));
    return;
  }
  if(stop >= user_data.user_lists[key].size()){
    stop = user_data.user_lists[key].size() - 1;
  }
  std::vector<std::string> strs;
  for(start; start <= stop; ++start){
    strs.push_back(user_data.user_lists[key][start]);
  }
  RespSerializer::sendRespMessage(client_fd, RespSerializer::array(strs));
}

void process_lpush_message(int client_fd, std::vector<std::string>& commands, UserData& user_data, LockManager& lockmanager){
  std::string key = commands[1];
  auto lock = lockmanager.get_user_lists_lock(key);
  std::unique_lock<std::mutex> guard(*lock);
  if(user_data.user_lists.find(key) == user_data.user_lists.end()){
    user_data.user_lists[key] = {};
  }
  for(int i = 2; i < commands.size(); ++i){
    user_data.user_lists[key].push_front(commands[i]);
  }
  if(!user_data.blocked_clients[key].empty()){
    auto blocked_client = user_data.blocked_clients[key].front();
    user_data.blocked_clients[key].pop_front();

    blocked_client->ready = true;
    blocked_client->cv.notify_one();
  }
  RespSerializer::sendRespMessage(client_fd, RespSerializer::integer(user_data.user_lists[key].size()));
}

void process_lpop_message(int client_fd, std::vector<std::string>& commands, UserData& user_data, LockManager& lockmanager){
  std::string key = commands[1];
  auto lock = lockmanager.get_user_lists_lock(key);
  std::unique_lock<std::mutex> guard(*lock);
  if(user_data.user_lists.find(key) == user_data.user_lists.end() || user_data.user_lists[key].size() == 0){
    RespSerializer::sendRespMessage(client_fd, RespSerializer::bulkString(""));
    return;
  }
  if(commands.size() > 2){
    std::vector<std::string> strs;
    int ele_to_pop = stoll(commands[2]);
    for(int i = 0; i <  ele_to_pop; ++i){
      strs.push_back(user_data.user_lists[key].front());
      user_data.user_lists[key].pop_front();
    }
    RespSerializer::sendRespMessage(client_fd, RespSerializer::array(strs));
    return;
  }
  RespSerializer::sendRespMessage(client_fd, RespSerializer::bulkString(user_data.user_lists[key].front()));
  user_data.user_lists[key].pop_front();
}

void process_blpop_message(int client_fd, std::vector<std::string>& commands, UserData& user_data, LockManager& lockmanager) {
  std::string key = commands[1];
  double timeout = std::stod(commands[2]);

  auto lock = lockmanager.get_user_lists_lock(key);
  std::unique_lock<std::mutex> guard(*lock);

  if(!user_data.user_lists[key].empty()){
    RespSerializer::sendRespMessage(client_fd, RespSerializer::array({key, user_data.user_lists[key].front()}));
    user_data.user_lists[key].pop_front();
    return;
  }
  auto blocked_client = std::make_shared<BlockingClient>();
  user_data.blocked_clients[key].push_back(blocked_client);
  if(timeout == 0){
    blocked_client->cv.wait(guard, [&] { return blocked_client->ready;});
  }
  else {
    auto timeout_duration = std::chrono::duration<double>(timeout);
    blocked_client->cv.wait_for(guard, timeout_duration, [&] { return blocked_client->ready;});
  }
  if(user_data.user_lists[key].empty()){
    RespSerializer::sendRespMessage(client_fd, RespSerializer::array({}));
    return;
  }

  RespSerializer::sendRespMessage(client_fd, RespSerializer::array({key, user_data.user_lists[key].front()}));
  user_data.user_lists[key].pop_front();
}

void process_client_message(int client_fd, const char* buffer, size_t length, UserData& user_data, LockManager& lockmanager){
    std::vector<std::string> commands = parser(buffer, length);
    if(commands[0] == "PING"){
      RespSerializer::sendRespMessage(client_fd, "+PONG\r\n");
    }
    else if (commands[0] == "ECHO"){
      RespSerializer::sendRespMessage(client_fd, "+" + commands[1] + "\r\n");
    }
    else if (commands[0] == "SET"){
      process_set_message(client_fd, commands, user_data.user_set_values);
    }
    else if (commands[0] == "GET"){
      process_get_message(client_fd, commands, user_data.user_set_values);
    }
    else if (commands[0] == "RPUSH"){
      process_rpush_message(client_fd, commands, user_data, lockmanager);
    }
    else if (commands[0] == "LRANGE"){
      process_lrange_message(client_fd, commands, user_data, lockmanager);
    }
    else if (commands[0] == "LPUSH"){
      process_lpush_message(client_fd, commands, user_data, lockmanager);
    }
    else if(commands[0] == "LLEN") {
      RespSerializer::sendRespMessage(client_fd, RespSerializer::integer(user_data.user_lists[commands[1]].size()));
    }
    else if(commands[0] == "LPOP"){
      process_lpop_message(client_fd, commands, user_data, lockmanager);
    }
    else if(commands[0] == "BLPOP"){
      process_blpop_message(client_fd, commands, user_data, lockmanager);
    }
}

void talk_to_client(int client_fd, UserData& user_data, LockManager& lockmanager){
  char buffer[1024];
  while(true){
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if(bytes_read <=0){
      close(client_fd);
      break;
    }
    process_client_message(client_fd, buffer, bytes_read, user_data, lockmanager);
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

  UserData user_data;
  LockManager lockmanager;
  while(true){
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    std::cout << "Client connected\n";
    std::thread(talk_to_client, client_fd, std::ref(user_data), std::ref(lockmanager)).detach();
    }
  close(server_fd);

  return 0;
}
