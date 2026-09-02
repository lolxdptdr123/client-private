#pragma once
#include <string>
#include <vector>

struct ConfigInfo {
    std::string id;
    std::string name;
    std::string description;
    std::string created;
    std::string modified;
    bool loaded = false;
};

namespace ConfigManager {
    std::string Directory();
    std::vector<ConfigInfo> List();
    bool SaveNew(const char* name, const char* description);
    bool Update(const std::string& id);
    bool Load(const std::string& id);
    bool Delete(const std::string& id);
    bool Rename(const std::string& id, const char* newName);
    const std::string& CurrentId();
    bool IsLoading();
}
