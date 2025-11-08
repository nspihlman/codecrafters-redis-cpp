#ifndef USER_DATA_H
#define USER_DATA_H

#include <string>
#include <chrono>
#include <unordered_map>
#include <deque>
#include <mutex>
#include <condition_variable>


class UserSetValue{
public:
  std::string value;
  int64_t msExpiry = -1;
  std::chrono::steady_clock::time_point inserted = std::chrono::steady_clock::now();

  UserSetValue();
  UserSetValue(std::string value);
  UserSetValue(std::string value, int64_t msExpiry);

  bool stillValid();
};

class BlockingClient{
public:
  std::condition_variable cv;
  bool ready = false;
};


class UserData{
public:
  std::unordered_map<std::string, UserSetValue> user_set_values;
  std::unordered_map<std::string, std::deque<std::string>> user_lists;
  std::unordered_map<std::string, std::deque<std::shared_ptr<BlockingClient>>> blocked_clients;
  std::unordered_map<std::string, std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>>> user_streams;
};

#endif