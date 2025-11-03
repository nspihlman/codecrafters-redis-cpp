#include "user_set_value.h"

class UserSetValue{
public:
  std::string value;
  int64_t msExpiry = -1; // expiry time in milliseconds. -1 denotes a UserSetValue with no expiry defined
  std::chrono::steady_clock::time_point inserted = std::chrono::steady_clock::now();

  UserSetValue() : value("") {}
  UserSetValue(std::string value) : value(value) {}
  UserSetValue(std::string value, int64_t msExpiry) : value(value), msExpiry(msExpiry) {}

  bool stillValid(){
    if(msExpiry == -1){
      return true;
    }
    return (std::chrono::steady_clock::now() - inserted) < std::chrono::milliseconds(msExpiry);
  }
};