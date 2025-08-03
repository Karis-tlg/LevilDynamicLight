#pragma once

#include <string>
#include <unordered_map>

class Lang {
public:
    bool load(const std::string& dir);
    std::string get(const std::string& lang, const std::string& key) const;

private:
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> langs;
};