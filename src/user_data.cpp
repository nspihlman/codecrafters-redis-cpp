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

UserStreamValue::UserStreamValue(std::string stream_id){
    int splt = stream_id.find("-");
    ms_time = stol(stream_id.substr(0, splt));
    seq_num = stol(stream_id.substr(splt+1));
    pairs = {};
}
