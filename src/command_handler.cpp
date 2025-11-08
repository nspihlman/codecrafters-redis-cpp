#include "command_handler.h"
#include "parser.h"
#include "respserializer.h"
#include <memory>

CommandHandler::CommandHandler(UserData& user_data, LockManager& lockmanager)
    : user_data_(user_data), lockmanager_(lockmanager) {
    initialize_handlers();
}

void CommandHandler::initialize_handlers() {
    command_handlers_ = {
        {"PING", [this](int fd, std::vector<std::string>& cmds) { handle_ping(fd, cmds); }},
        {"ECHO", [this](int fd, std::vector<std::string>& cmds) { handle_echo(fd, cmds); }},
        {"SET", [this](int fd, std::vector<std::string>& cmds) { handle_set(fd, cmds); }},
        {"GET", [this](int fd, std::vector<std::string>& cmds) { handle_get(fd, cmds); }},
        {"RPUSH", [this](int fd, std::vector<std::string>& cmds) { handle_rpush(fd, cmds); }},
        {"LPUSH", [this](int fd, std::vector<std::string>& cmds) { handle_lpush(fd, cmds); }},
        {"LRANGE", [this](int fd, std::vector<std::string>& cmds) { handle_lrange(fd, cmds); }},
        {"LLEN", [this](int fd, std::vector<std::string>& cmds) { handle_llen(fd, cmds); }},
        {"LPOP", [this](int fd, std::vector<std::string>& cmds) { handle_lpop(fd, cmds); }},
        {"BLPOP", [this](int fd, std::vector<std::string>& cmds) { handle_blpop(fd, cmds); }}
    };
}

void CommandHandler::process(int client_fd, const char* buffer, size_t length) {
    std::vector<std::string> commands = parser(buffer, length);
    
    auto it = command_handlers_.find(commands[0]);
    if (it != command_handlers_.end()) {
        // Command found, execute its handler
        it->second(client_fd, commands);
    } else {
        // Unknown command - send error response
        RespSerializer::sendRespMessage(client_fd, "-ERR unknown command '" + commands[0] + "'\r\n");
    }
}

void CommandHandler::handle_ping(int client_fd, std::vector<std::string>& commands) {
    RespSerializer::sendRespMessage(client_fd, "+PONG\r\n");
}

void CommandHandler::handle_echo(int client_fd, std::vector<std::string>& commands) {
    RespSerializer::sendRespMessage(client_fd, "+" + commands[1] + "\r\n");
}

void CommandHandler::handle_set(int client_fd, std::vector<std::string>& commands) {
    UserSetValue newValue = UserSetValue(commands[2]);
    if(commands.size() == 5 && commands[3] == "PX"){
        newValue.msExpiry = stoll(commands[4], NULL, 10);
    }
    else if (commands.size() == 5 && commands[3] == "EX"){
        newValue.msExpiry = stoll(commands[4], NULL, 10) * 1000;
    }
    user_data_.user_set_values[commands[1]] = newValue;
    RespSerializer::sendRespMessage(client_fd, "+OK\r\n");
}

void CommandHandler::handle_get(int client_fd, std::vector<std::string>& commands) {
    if(user_data_.user_set_values.find(commands[1]) == user_data_.user_set_values.end()){
        RespSerializer::sendRespMessage(client_fd, RespSerializer::bulkString(""));
        return;
    }
    if(!user_data_.user_set_values[commands[1]].stillValid()){
        user_data_.user_set_values.erase(commands[1]);
        RespSerializer::sendRespMessage(client_fd, RespSerializer::bulkString(""));
        return;
    }
    RespSerializer::sendRespMessage(client_fd, RespSerializer::bulkString(user_data_.user_set_values[commands[1]].value));
}

void CommandHandler::handle_rpush(int client_fd, std::vector<std::string>& commands) {
    std::string key = commands[1];
    auto lock = lockmanager_.get_user_lists_lock(key);
    std::unique_lock<std::mutex> guard(*lock);
    if(user_data_.user_lists.find(key) == user_data_.user_lists.end()){
        user_data_.user_lists[key] = {};
    }
    for(int i = 2; i < commands.size(); ++i){
        user_data_.user_lists[key].push_back(commands[i]);
    }
    if(!user_data_.blocked_clients[key].empty()){
        auto blocked_client = user_data_.blocked_clients[key].front();
        user_data_.blocked_clients[key].pop_front();

        blocked_client->ready = true;
        blocked_client->cv.notify_one();
    }
    RespSerializer::sendRespMessage(client_fd, RespSerializer::integer(user_data_.user_lists[key].size()));
}

void CommandHandler::handle_lpush(int client_fd, std::vector<std::string>& commands) {
    std::string key = commands[1];
    auto lock = lockmanager_.get_user_lists_lock(key);
    std::unique_lock<std::mutex> guard(*lock);
    if(user_data_.user_lists.find(key) == user_data_.user_lists.end()){
        user_data_.user_lists[key] = {};
    }
    for(int i = 2; i < commands.size(); ++i){
        user_data_.user_lists[key].push_front(commands[i]);
    }
    if(!user_data_.blocked_clients[key].empty()){
        auto blocked_client = user_data_.blocked_clients[key].front();
        user_data_.blocked_clients[key].pop_front();

        blocked_client->ready = true;
        blocked_client->cv.notify_one();
    }
    RespSerializer::sendRespMessage(client_fd, RespSerializer::integer(user_data_.user_lists[key].size()));
}

int_fast64_t CommandHandler::convert_indexes(int index, size_t list_length) {
    if(index >= 0) {
        return index;
    }
    int positive_index = list_length + index;
    if(positive_index < 0){
        return 0;
    }
    return positive_index;
}

void CommandHandler::handle_lrange(int client_fd, std::vector<std::string>& commands) {
    std::string key = commands[1];
    auto lock = lockmanager_.get_user_lists_lock(key);
    std::unique_lock<std::mutex> guard(*lock);
    if(user_data_.user_lists.find(key) == user_data_.user_lists.end()){
        RespSerializer::sendRespMessage(client_fd, RespSerializer::array({}));
        return;
    }

    int start = convert_indexes(stoll(commands[2]), user_data_.user_lists[key].size());
    int stop = convert_indexes(stoll(commands[3]), user_data_.user_lists[key].size());
    
    if(user_data_.user_lists.find(key) == user_data_.user_lists.end() || start >= user_data_.user_lists[key].size() || start > stop){
        RespSerializer::sendRespMessage(client_fd, RespSerializer::array({}));
        return;
    }
    if(stop >= user_data_.user_lists[key].size()){
        stop = user_data_.user_lists[key].size() - 1;
    }
    std::vector<std::string> strs;
    for(start; start <= stop; ++start){
        strs.push_back(user_data_.user_lists[key][start]);
    }
    RespSerializer::sendRespMessage(client_fd, RespSerializer::array(strs));
}

void CommandHandler::handle_llen(int client_fd, std::vector<std::string>& commands) {
    RespSerializer::sendRespMessage(client_fd, RespSerializer::integer(user_data_.user_lists[commands[1]].size()));
}

void CommandHandler::handle_lpop(int client_fd, std::vector<std::string>& commands) {
    std::string key = commands[1];
    auto lock = lockmanager_.get_user_lists_lock(key);
    std::unique_lock<std::mutex> guard(*lock);
    if(user_data_.user_lists.find(key) == user_data_.user_lists.end() || user_data_.user_lists[key].size() == 0){
        RespSerializer::sendRespMessage(client_fd, RespSerializer::bulkString(""));
        return;
    }
    if(commands.size() > 2){
        std::vector<std::string> strs;
        int ele_to_pop = stoll(commands[2]);
        for(int i = 0; i <  ele_to_pop; ++i){
            strs.push_back(user_data_.user_lists[key].front());
            user_data_.user_lists[key].pop_front();
        }
        RespSerializer::sendRespMessage(client_fd, RespSerializer::array(strs));
        return;
    }
    RespSerializer::sendRespMessage(client_fd, RespSerializer::bulkString(user_data_.user_lists[key].front()));
    user_data_.user_lists[key].pop_front();
}

void CommandHandler::handle_blpop(int client_fd, std::vector<std::string>& commands) {
    std::string key = commands[1];
    double timeout = std::stod(commands[2]);

    auto lock = lockmanager_.get_user_lists_lock(key);
    std::unique_lock<std::mutex> guard(*lock);

    if(!user_data_.user_lists[key].empty()){
        RespSerializer::sendRespMessage(client_fd, RespSerializer::array({key, user_data_.user_lists[key].front()}));
        user_data_.user_lists[key].pop_front();
        return;
    }
    auto blocked_client = std::make_shared<BlockingClient>();
    user_data_.blocked_clients[key].push_back(blocked_client);
    if(timeout == 0){
        blocked_client->cv.wait(guard, [&] { return blocked_client->ready;});
    }
    else {
        auto timeout_duration = std::chrono::duration<double>(timeout);
        blocked_client->cv.wait_for(guard, timeout_duration, [&] { return blocked_client->ready;});
    }
    if(user_data_.user_lists[key].empty()){
        RespSerializer::sendRespMessage(client_fd, "*-1\r\n");
        return;
    }

    RespSerializer::sendRespMessage(client_fd, RespSerializer::array({key, user_data_.user_lists[key].front()}));
    user_data_.user_lists[key].pop_front();
}
