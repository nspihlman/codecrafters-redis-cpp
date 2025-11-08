#include "lockmanager.h"

std::shared_ptr<std::mutex> LockManager::get_user_lists_lock(const std::string& key) {
    std::scoped_lock guard(global_mtx);
    auto& ptr = user_lists_locks[key];
    if(!ptr){
        ptr = std::make_shared<std::mutex>();
    }
    return ptr;
}

std::shared_ptr<std::mutex> LockManager::get_user_streams_lock(const std::string& key) {
    std::scoped_lock guard(global_mtx);
    auto& ptr = user_streams_locks[key];
    if(!ptr){
        ptr = std::make_shared<std::mutex>();
    }
    return ptr;
}
