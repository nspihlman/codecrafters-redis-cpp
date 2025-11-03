#include "user_data.h"


UserSetValue::UserSetValue() : value("") {}
UserSetValue::UserSetValue(std::string value) : value(value) {}
UserSetValue::UserSetValue(std::string value, int64_t msExpiry) : value(value), msExpiry(msExpiry) {}

bool UserSetValue::stillValid(){
    if(msExpiry == -1){
        return true;
    }
    return (std::chrono::steady_clock::now() - inserted) < std::chrono::milliseconds(msExpiry);
}
