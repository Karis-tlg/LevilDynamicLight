#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct Config {
    std::vector<std::string> item_allow_offhand;
    int refresh_tick = 1;

    static Config load(const std::string& path);
    void save(const std::string& path) const;
};
