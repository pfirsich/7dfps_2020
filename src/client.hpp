#pragma once

#include <string>
#include <vector>

#include <glwx.hpp>

#include "ecs.hpp"
#include "graphics.hpp"
#include "net.hpp"
#include "shipsystem.hpp"
#include "sound.hpp"
#include "terminaldata.hpp"
#include "util.hpp"

class Client {
public:
    Client() = default;

    // this blocks until the window is closed or the player ends the game
    bool run(std::optional<HostPort> hostPort, uint32_t gameCode);

private:
    struct MoveState {
    };

    struct TerminalState {
        ecs::EntityHandle terminalEntity;
        std::string systemName;
        int currentHistoryIndex = 0;
    };

    using PlayerState = std::variant<MoveState, TerminalState>;

    uint32_t showConnectCodeMenu(std::optional<HostPort>& hostPort);
    void showError(const std::string& message);

    void resized(size_t width, size_t height);

    void processSdlEvents();
    void processEnetEvents();
    void update(float dt);
    void sendUpdate();
    void receive(uint8_t channelId, const enet::Packet& packet);
    void draw();
    void addPlayer(PlayerId id);
    void handleInteractions();

    template <MessageType MsgType>
    bool send(Channel channel, const Message<MsgType>& message)
    {
        return sendMessage(serverPeer_, channel, frameCounter_, message);
    }

    template <MessageType MsgType>
    void processMessage(uint32_t frameNumber, ReadBuffer& buffer)
    {
        Message<MsgType> message;
        if (!deserialize(buffer, message)) {
            printErr("Could not decode message of type {}", static_cast<uint8_t>(MsgType));
            return;
        }
        processMessage(frameNumber, message);
    }

    void stopTerminalInteraction();
    void scrollTerminal(float amount);
    void terminalHistory(int offset);

    ecs::EntityHandle findTerminal(const std::string& system);

    void processMessage(uint32_t frameNumber, const Message<MessageType::ServerHello>& message);
    void processMessage(
        uint32_t frameNumber, const Message<MessageType::ServerPlayerStateUpdate>& message);
    void processMessage(
        uint32_t frameNumber, const Message<MessageType::ServerInteractTerminal>& message);
    void processMessage(
        uint32_t frameNumber, const Message<MessageType::ServerUpdateTerminalOutput>& message);
    void processMessage(
        uint32_t frameNumber, const Message<MessageType::ServerAddTerminalHistory>& message);
    void processMessage(uint32_t frameNumber, const Message<MessageType::ClientPlaySound>& message);
    void processMessage(
        uint32_t frameNumber, const Message<MessageType::ServerUpdateInputEnabled>& message);
    void processMessage(
        uint32_t frameNumber, const Message<MessageType::ServerUpdateShipState>& message);

    SoLoud::handle playEntitySound(const std::string& name, const std::string entityName,
        float volume = 1.0f, float playbackSpeed = 1.0f);
    SoLoud::handle playEntitySound(const std::string& name, ecs::EntityHandle entity,
        float volume = 1.0f, float playbackSpeed = 1.0f);
    void playNetSound(const std::string& name, const glm::vec3& position);

    ENetPeer* serverPeer_ = nullptr;
    enet::Host host_;
    glwx::Window window_;
    ecs::World world_;
    Frustum frustum_;
    PlayerState state_;
    ShipState shipState_;
    float nextStepSound_ = 0.0f;
    std::unordered_map<PlayerId, ecs::EntityHandle> players_; // excludes self
    std::unordered_map<ShipSystem::Name, TerminalData> terminalData_;
    std::vector<std::shared_ptr<Mesh>> playerMeshes_;
    std::unique_ptr<Skybox> skybox_;
    ecs::EntityHandle player_;
    ecs::EntityHandle hitMarker_;
    float time_ = 0.0f;
    uint32_t frameCounter_ = 0;
    PlayerId playerId_ = InvalidPlayerId;
    bool started_ = false;
    bool running_ = false;
};
