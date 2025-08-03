#pragma once

#include "Config.h"
#include "Lang.h"
#include <ll/api/mod/Mod.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/io/Logger.h>
#include <mc/world/actor/player/Player.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

struct PlayerLightInfo {
    std::vector<int> pos;
    int dim;
    int runtime_id;
    int light_level;
};

class DynamicLight : public ll::mod::Mod {
public:
    DynamicLight();
    ~DynamicLight();

private:
    void load();
    void enable();
    void disable();
    void handle_tick();
    void handle_offhand(const CommandOrigin& origin, CommandOutput& out);
    void handle_ud(const CommandOrigin& origin, CommandOutput& out);

    Config config;
    Lang lang;
    std::string plugin_dir;

    std::unique_ptr<ll::event::ListenerPtr> tick_listener;
    std::unordered_map<std::string, int> glowing_items;
    std::unordered_map<std::string, PlayerLightInfo> player_lights;
};
