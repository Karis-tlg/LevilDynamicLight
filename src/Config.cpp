#include "Config.h"
#include <fstream>

Config Config::load(const std::string& path) {
    Config cfg;
    std::ifstream f(path);
    if (f.good()) {
        nlohmann::json js;
        f >> js;
        if (js.contains("item_allow_offhand"))
            cfg.item_allow_offhand = js["item_allow_offhand"].get<std::vector<std::string>>();
        if (js.contains("refresh_tick"))
            cfg.refresh_tick = js["refresh_tick"].get<int>();
    }
    return cfg;
}

void Config::save(const std::string& path) const {
    nlohmann::json js;
    js["item_allow_offhand"] = item_allow_offhand;
    js["refresh_tick"] = refresh_tick;
    std::ofstream f(path);
    f << js.dump(4);
}
