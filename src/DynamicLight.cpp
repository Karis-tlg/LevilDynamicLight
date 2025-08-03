#include "DynamicLight.h"
#include <ll/api/service/Bedrock.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/scheduler/Scheduler.h>
#include <ll/api/command/CommandRegistrar.h>
#include <mc/world/actor/player/Player.h>
#include <mc/network/packet/UpdateBlockPacket.h>
#include <mc/network/NetworkBlockPosition.h>
#include <ll/api/logger/Logger.h>
#include <filesystem>
#include <cmath>

struct LightRecord {
    std::vector<int> pos;
    int dim;
    int runtime_id;
    int glow_level;
};
std::unordered_map<std::string, LightRecord> player_lights;

void sendLightBlock(Player* player, int x, int y, int z, int runtime_id) {
    UpdateBlockPacket pkt;
    pkt.mPos = NetworkBlockPosition{x, y, z};
    pkt.mRuntimeId = runtime_id;
    pkt.mLayer = 0;         // Block layer thường là 0
    pkt.mUpdateFlags = 3;   // All, giống python
    pkt.sendTo(*player);
}

int get_air_runtime_id() {
    auto block_data = ll::service::getServer().create_block_data("minecraft:air", nullptr);
    return block_data.runtime_id;
}

// Helper lấy runtime_id của light_block
int get_light_block_runtime_id(int level) {
    std::string block_id = "minecraft:light_block_" + std::to_string(level);
    auto block_data = ll::service::getServer().create_block_data(block_id, nullptr);
    return block_data.runtime_id;
}

void DynamicLight::load() {
    plugin_dir = getSelf().getConfigDir();
    config = Config::load(plugin_dir + "/../config/config.json");
    lang.load(plugin_dir + "/../langs");

    glowing_items = {
        {"minecraft:torch", 14},
        {"minecraft:soul_torch", 10},
        {"minecraft:redstone_torch", 7},
        {"minecraft:shroomlight", 15},
        {"minecraft:glow_berries", 14},
        {"minecraft:glowstone", 15},
        {"minecraft:lit_pumpkin", 15},
        {"minecraft:campfire", 15},
        {"minecraft:soul_campfire", 10},
        {"minecraft:end_rod", 14},
        {"minecraft:lantern", 15},
        {"minecraft:soul_lantern", 10},
        {"minecraft:sea_lantern", 15},
        {"minecraft:ochre_froglight", 15},
        {"minecraft:pearlescent_froglight", 15},
        {"minecraft:verdant_froglight", 15},
        {"minecraft:crying_obsidian", 10},
        {"minecraft:beacon", 15},
        {"minecraft:lava_bucket", 15},
        {"minecraft:ender_chest", 7},
        {"minecraft:glow_lichen", 7},
        {"minecraft:enchanting_table", 7},
        {"minecraft:small_amethyst_bud", 1},
        {"minecraft:large_amethyst_bud", 4},
        {"minecraft:amethyst_cluster", 5},
        {"minecraft:brown_mushroom", 1},
        {"minecraft:sculk_catalyst", 6},
        {"minecraft:conduit", 15},
        {"minecraft:medium_amethyst_bud", 2},
        {"minecraft:dragon_egg", 1},
        {"minecraft:magma", 3}
    };
}

void DynamicLight::enable() {
    auto& reg = ll::command::CommandRegistrar::getInstance();
    reg.getOrCreateCommand("offhand", "Switch items to offhand")
        .overload()
        .execute([this](const ll::command::CommandOrigin& origin, ll::command::CommandOutput& out) {
            handle_offhand(origin);
        });

    reg.getOrCreateCommand("ud", "Open main form of DynamicLight")
        .overload()
        .execute([this](const ll::command::CommandOrigin& origin, ll::command::CommandOutput& out) {
            handle_ud(origin);
        });

    // Tick mỗi refresh_tick
    ll::service::getScheduler().scheduleRepeatingTask([this]() { handle_tick(); }, config.refresh_tick);

    getSelf().getLogger().info("DynamicLight enabled.");
}

void DynamicLight::disable() {
    int air_runtime_id = get_air_runtime_id();
    auto& server = ll::service::getServer();
    for (auto* p : server.getPlayers()) {
        auto it = player_lights.find(p->getName());
        if (it != player_lights.end()) {
            sendLightBlock(p, it->second.pos[0], it->second.pos[1], it->second.pos[2], air_runtime_id);
            player_lights.erase(it);
        }
    }
}

void DynamicLight::handle_tick() {
    auto& server = ll::service::getServer();
    int air_runtime_id = get_air_runtime_id();

    for (auto* p : server.getPlayers()) {
        if (!p) continue;

        // Lấy item main hand và offhand
        auto* item_main = p->getCarriedItem();  // ItemStack*
        auto offhand_item = p->getOffhandSlot(); // ItemStack (by value hoặc const ref, tuỳ API)

        std::string glow_id;
        int glow_level = 0;

        // Ưu tiên main hand. Nếu không có thì check offhand
        if (item_main && glowing_items.count(item_main->getTypeName())) {
            glow_id = item_main->getTypeName();
            glow_level = glowing_items[glow_id];
        } else if (glowing_items.count(offhand_item.getTypeName())) {
            glow_id = offhand_item.getTypeName();
            glow_level = glowing_items[glow_id];
        }

        auto& name = p->getName();

        if (!glow_id.empty()) {
            // Player đang cầm item phát sáng ở main hoặc offhand
            std::vector<int> pos = {
                static_cast<int>(std::floor(p->getPosition().x)),
                static_cast<int>(std::floor(p->getPosition().y) + 1),
                static_cast<int>(std::floor(p->getPosition().z))
            };

            int dim_id = p->getDimensionId();
            int runtime_id = get_light_block_runtime_id(glow_level);

            bool need_update = true;
            auto it = player_lights.find(name);
            if (it != player_lights.end()) {
                if (it->second.pos == pos && it->second.dim == dim_id && it->second.runtime_id == runtime_id) {
                    need_update = false;
                } else {
                    sendLightBlock(p, it->second.pos[0], it->second.pos[1], it->second.pos[2], air_runtime_id);
                }
            }

            if (need_update) {
                sendLightBlock(p, pos[0], pos[1], pos[2], runtime_id);
                player_lights[name] = {pos, dim_id, runtime_id, glow_level};
            }
        } else {
            auto it = player_lights.find(name);
            if (it != player_lights.end()) {
                sendLightBlock(p, it->second.pos[0], it->second.pos[1], it->second.pos[2], air_runtime_id);
                player_lights.erase(it);
            }
        }
    }
}

void DynamicLight::handle_offhand(const ll::command::CommandOrigin& origin) {
    auto player = dynamic_cast<mc::Player*>(origin.getEntity());
    if (!player) {
        origin.getOutput().error("This command can only be used by a player.");
        return;
    }

    auto main_item = player->getCarriedItem();     // Trả về ItemStack*
    auto offhand_item = player->getOffhandSlot();  // Trả về ItemStack

    if (!main_item) {
        player->sendMessage(lang.get(player->getLocale(), "switch.message.fail_1"));
        return;
    }

    // Kiểm tra item hợp lệ (trong danh sách allow_offhand của config)
    if (std::find(config.item_allow_offhand.begin(), config.item_allow_offhand.end(), main_item->getTypeName()) == config.item_allow_offhand.end()) {
        player->sendMessage(lang.get(player->getLocale(), "switch.message.fail_2"));
        // Gợi ý các item hợp lệ
        if (!config.item_allow_offhand.empty()) {
            player->sendMessage(lang.get(player->getLocale(), "switch.message.fail_3"));
            for (const auto& id : config.item_allow_offhand)
                player->sendMessage("- " + id);
        }
        return;
    }

    // Swap item giữa main hand và offhand
    // Lưu ý: offhand_item có thể trả về ItemStack, main_item là ItemStack*
    // Nếu getOffhandSlot trả về ItemStack*, hãy đổi lại kiểu biến cho đúng!
    player->setCarriedItem(offhand_item);
    player->setOffhandSlot(*main_item);

    player->sendMessage(lang.get(player->getLocale(), "switch.message.success"));
}

void DynamicLight::handle_ud(const ll::command::CommandOrigin& origin) {
    auto player = dynamic_cast<mc::Player*>(origin.getEntity());
    if (!player) {
        origin.getOutput().error("Only player can use this!");
        return;
    }
    ll::form::ModalForm form(
        "Dynamic Light",
        lang.get(player->getLocale(), "form.content"),
        lang.get(player->getLocale(), "form.button"),
        "",
        [this](mc::Player& p, bool ok) {
            if (ok) {
                config = Config::load(plugin_dir + "/../config/config.json");
                p.sendMessage(lang.get(p.getLocale(), "reload.message.success"));
            }
        }
    );
    form.sendTo(*player);
}
