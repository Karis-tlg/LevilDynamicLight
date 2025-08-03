#include "DynamicLight.h"
#include "Config.h"
#include "Lang.h"

#include <ll/api/event/world/LevelTickEvent.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/form/ModalForm.h>
#include <ll/api/service/Server.h>
#include <ll/api/io/Logger.h>

#include <mc/world/actor/player/Player.h>
#include <mc/network/packet/UpdateBlockPacket.h>
#include <mc/network/NetworkBlockPosition.h>
#include <mc/server/commands/CommandOrigin.h>
#include <mc/server/commands/CommandOutput.h>

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>
#include <cmath>

static std::string default_locale = "en_US";

// Helper ghi ánh sáng
void sendLightBlock(Player* player, int x, int y, int z, int runtime_id) {
    UpdateBlockPacket pkt;
    pkt.mPos = NetworkBlockPosition{x, y, z};
    pkt.mRuntimeId = runtime_id;
    pkt.mLayer = 0;
    pkt.mUpdateFlags = 3;
    pkt.sendTo(*player);
}

// Lấy runtime id của air block
int get_air_runtime_id() {
    auto block_data = ll::service::Server::getInstance().create_block_data("minecraft:air", nullptr);
    return block_data.runtime_id;
}

// Lấy runtime id của light block với level cho trước
int get_light_block_runtime_id(int level) {
    std::string block_id = "minecraft:light_block_" + std::to_string(level);
    auto block_data = ll::service::Server::getInstance().create_block_data(block_id, nullptr);
    return block_data.runtime_id;
}

DynamicLight::DynamicLight() : Mod(Manifest("DynamicLight", "1.0.0")) {
    onLoad([this](Mod&) {
        this->load();
        return true;
    });
    onEnable([this](Mod&) {
        this->enable();
        return true;
    });
    onDisable([this](Mod&) {
        this->disable();
        return true;
    });
}

DynamicLight::~DynamicLight() = default;

void DynamicLight::load() {
    plugin_dir = getConfigDir().string();
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
        .execute([this](const CommandOrigin& origin, CommandOutput& out) {
            handle_offhand(origin, out);
        });

    reg.getOrCreateCommand("ud", "Open main form of DynamicLight")
        .overload()
        .execute([this](const CommandOrigin& origin, CommandOutput& out) {
            handle_ud(origin, out);
        });

    tick_listener = ll::event::EventBus::getInstance().emplaceListener<ll::event::world::LevelTickEvent>(
        [this](ll::event::world::LevelTickEvent&) {
            handle_tick();
        }
    );
    getLogger().info("DynamicLight enabled.");
}

void DynamicLight::disable() {
    int air_runtime_id = get_air_runtime_id();
    auto& server = ll::service::Server::getInstance();
    for (auto* p : server.getPlayers()) {
        auto it = player_lights.find(p->getName());
        if (it != player_lights.end()) {
            sendLightBlock(p, it->second.pos[0], it->second.pos[1], it->second.pos[2], air_runtime_id);
            player_lights.erase(it);
        }
    }
}

void DynamicLight::handle_tick() {
    auto& server = ll::service::Server::getInstance();
    int air_runtime_id = get_air_runtime_id();

    for (auto* p : server.getPlayers()) {
        if (!p) continue;

        auto* item_main = p->getCarriedItem();
        auto offhand_item = p->getOffhandSlot();

        std::string glow_id;
        int glow_level = 0;

        if (item_main && glowing_items.count(item_main->getTypeName())) {
            glow_id = item_main->getTypeName();
            glow_level = glowing_items[glow_id];
        } else if (glowing_items.count(offhand_item.getTypeName())) {
            glow_id = offhand_item.getTypeName();
            glow_level = glowing_items[glow_id];
        }

        auto& name = p->getName();

        if (!glow_id.empty()) {
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

void DynamicLight::handle_offhand(const CommandOrigin& origin, CommandOutput& out) {
    auto player = dynamic_cast<Player*>(origin.getEntity());
    if (!player) {
        out.error("This command can only be used by a player.");
        return;
    }
    // TODO: Nếu API có getLocale(), hãy lấy đúng locale của player
    std::string plocale = default_locale;

    auto main_item = player->getCarriedItem();
    auto offhand_item = player->getOffhandSlot();

    if (!main_item) {
        player->sendMessage(lang.get(plocale, "switch.message.fail_1"));
        return;
    }
    if (std::find(config.item_allow_offhand.begin(), config.item_allow_offhand.end(), main_item->getTypeName()) == config.item_allow_offhand.end()) {
        player->sendMessage(lang.get(plocale, "switch.message.fail_2"));
        if (!config.item_allow_offhand.empty()) {
            player->sendMessage(lang.get(plocale, "switch.message.fail_3"));
            for (const auto& id : config.item_allow_offhand)
                player->sendMessage("- " + id);
        }
        return;
    }
    player->setCarriedItem(offhand_item);
    player->setOffhandSlot(*main_item);

    player->sendMessage(lang.get(plocale, "switch.message.success"));
}

void DynamicLight::handle_ud(const CommandOrigin& origin, CommandOutput& out) {
    auto player = dynamic_cast<Player*>(origin.getEntity());
    if (!player) {
        out.error("Only player can use this!");
        return;
    }
    std::string plocale = default_locale; // TODO: Nếu có getLocale() thì thay ở đây

    ll::form::ModalForm form;
    form.setTitle("Dynamic Light")
        .setContent(lang.get(plocale, "form.content"))
        .setUpperButton(lang.get(plocale, "form.button"))
        .setLowerButton("Close");
    form.sendTo(*player, [this, plocale](Player& p, std::optional<bool> ok, ll::form::FormCancelReason) {
        if (!ok) return;
        if (*ok) {
            config = Config::load(plugin_dir + "/../config/config.json");
            p.sendMessage(lang.get(plocale, "reload.message.success"));
        }
    });
}