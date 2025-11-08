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
  std::unordered_map<std::string, std::shared_ptr<std::mutex>> user_streams_locks;
  
  std::shared_ptr<std::mutex> get_user_lists_lock(const std::string& key);
  std::shared_ptr<std::mutex> get_user_streams_lock(const std::string& key);
};

#endif