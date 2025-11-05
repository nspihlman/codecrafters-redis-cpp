#ifndef LOCKMANAGER_H
#define LOCKMANAGER_H

#include <mutex>
#include <string>
#include <unordered_map>
#include <memory>

class LockManager{
public:
  std::mutex global_mtx;
  std::unordered_map<std::string, std::shared_ptr<std::mutex>> user_lists_locks;
  
  std::shared_ptr<std::mutex> get_user_lists_lock(const std::string& key);
};

#endif