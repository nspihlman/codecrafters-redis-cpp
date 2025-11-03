#include <string>
#include <chrono>


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