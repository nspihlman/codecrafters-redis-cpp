#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "user_data.h"
#include "lockmanager.h"

class CommandHandler {
public:
    CommandHandler(UserData& user_data, LockManager& lockmanager);
    
    void process(int client_fd, const char* buffer, size_t length);

private:
    UserData& user_data_;
    LockManager& lockmanager_;
    
    using Handler = std::function<void(int, std::vector<std::string>&)>;
    
    std::unordered_map<std::string, Handler> command_handlers_;
    
    // Initialize the command handler map
    void initialize_handlers();
    
    // String commands
    void handle_ping(int client_fd, std::vector<std::string>& commands);
    void handle_echo(int client_fd, std::vector<std::string>& commands);
    void handle_set(int client_fd, std::vector<std::string>& commands);
    void handle_get(int client_fd, std::vector<std::string>& commands);
    
    // List commands
    void handle_rpush(int client_fd, std::vector<std::string>& commands);
    void handle_lpush(int client_fd, std::vector<std::string>& commands);
    void handle_lrange(int client_fd, std::vector<std::string>& commands);
    void handle_llen(int client_fd, std::vector<std::string>& commands);
    void handle_lpop(int client_fd, std::vector<std::string>& commands);
    void handle_blpop(int client_fd, std::vector<std::string>& commands);

    //Stream commands
    void handle_type(int client_fd, std::vector<std::string>& commands);
    
    // Helper functions
    int_fast64_t convert_indexes(int index, size_t list_length);
};

#endif
