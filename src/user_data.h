#include <string>
#include <chrono>
#include <unordered_map>
#include <deque>


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


class UserData{
public:
  std::unordered_map<std::string, UserSetValue> user_set_values;
  std::unordered_map<std::string, std::deque<std::string>> user_lists;
};