#include "Lang.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

bool Lang::load(const std::string& dir) {
    using namespace std::filesystem;
    for (auto& file : directory_iterator(dir)) {
        if (!file.path().has_extension() || file.path().extension() != ".json") continue;
        std::ifstream f(file.path());
        if (!f.good()) continue;
        nlohmann::json js; f >> js;
        std::string lang = file.path().stem().string();
        for (auto& [k, v] : js.items()) {
            langs[lang][k] = v.get<std::string>();
        }
    }
    return true;
}

std::string Lang::get(const std::string& lang, const std::string& key) const {
    auto it = langs.find(lang);
    if (it != langs.end() && it->second.count(key))
        return it->second.at(key);
    // fallback to en_US
    auto en = langs.find("en_US");
    if (en != langs.end() && en->second.count(key))
        return en->second.at(key);
    return key;
}
