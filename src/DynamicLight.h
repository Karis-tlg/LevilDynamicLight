#pragma once

#include "Config.h"
#include "Lang.h"
#include <ll/api/mod/Mod.h>
#include <ll/api/event/player/PlayerJoinEvent.h>
#include <ll/api/event/player/PlayerUseItemEvent.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/command/CommandRegistrar.h>
#include <mc/server/commands/CommandOrigin.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/io/Logger.h>
#include <mc/world/actor/player/Player.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

// Struct chứa thông tin ánh sáng tạm cho mỗi player
struct PlayerLightInfo {
    std::vector<int> pos; // {x, y, z}
    int dim;              // ID dimension, hoặc pointer tuỳ LeviLamina
    int runtime_id;
    int light_level;
};

class DynamicLight : public ll::mod::Mod {
public:
    void load() override;
    void enable() override;
    void disable() override;

private:
    // Hàm chính xử lý theo tick
    void handle_tick();
    // Lệnh /offhand
    void handle_offhand(const ll::command::CommandOrigin& origin);
    // Lệnh /ud (giao diện reload config)
    void handle_ud(const ll::command::CommandOrigin& origin);

    // Config
    Config config;
    // Quản lý ngôn ngữ
    Lang lang;
    // Thư mục plugin (tự lấy bằng getSelf().getConfigDir() khi load)
    std::string plugin_dir;

    // Listener cho các sự kiện cần thiết
    std::unique_ptr<ll::event::ListenerPtr> tick_listener;
    std::unique_ptr<ll::event::ListenerPtr> join_listener;
    std::unique_ptr<ll::event::ListenerPtr> use_item_listener;

    // Map vật phẩm phát sáng: id (minecraft:xxx) → mức sáng (level)
    std::unordered_map<std::string, int> glowing_items;

    // Map trạng thái block sáng cho từng player: name (hoặc uuid) → thông tin block sáng
    std::unordered_map<std::string, PlayerLightInfo> player_lights;
};
