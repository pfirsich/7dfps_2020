#pragma once

#include <string>
#include <vector>

#include <glwx.hpp>

#include "ecs.hpp"
#include "graphics.hpp"
#include "net.hpp"
#include "shipsystem.hpp"

class Client {
public:
    Client() = default;

    // this blocks until the window is closed or the player ends the game
    bool run(const std::string& host, Port port);

private:
    struct MoveState {
    };

    struct TerminalState {
        ecs::EntityHandle terminalEntity;
        std::string systemName;
        std::string terminalInput;
        glm::vec3 startPos; // this sucks
    };

    using PlayerState = std::variant<MoveState, TerminalState>;

    struct TerminalData {
        std::string input;
        std::string output;
    };

    void resized(size_t width, size_t height);

    void processSdlEvents();
    void processEnetEvents();
    void update(float dt);
    void sendUpdate();
    void receive(const enet::Packet& packet);
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
            fmt::print(
                stderr, "Could not decode message of type {}\n", static_cast<uint8_t>(MsgType));
            return;
        }
        processMessage(frameNumber, message);
    }

    ecs::EntityHandle findTerminal(const std::string& system);

    void processMessage(uint32_t frameNumber, const Message<MessageType::ServerHello>& message);
    void processMessage(
        uint32_t frameNumber, const Message<MessageType::ServerPlayerStateUpdate>& message);
    void processMessage(
        uint32_t frameNumber, const Message<MessageType::ServerInteractTerminal>& message);
    void processMessage(
        uint32_t frameNumber, const Message<MessageType::ServerUpdateTerminalOutput>& message);

    void playSound(const std::string& name, const std::string entityName);

    ENetPeer* serverPeer_ = nullptr;
    enet::Host host_;
    glwx::Window window_;
    ecs::World world_;
    PlayerState state_;
    std::unordered_map<PlayerId, ecs::EntityHandle> players_; // excludes self
    std::unordered_map<ShipSystem::Name, TerminalData> terminalData_;
    std::shared_ptr<Mesh> playerMesh_;
    std::unique_ptr<Skybox> skybox_;
    ecs::EntityHandle player_;
    ecs::EntityHandle hitMarker_;
    glm::mat4 projection_;
    float time_ = 0.0f;
    uint32_t frameCounter_ = 0;
    PlayerId playerId_ = InvalidPlayerId;
    bool started_ = false;
    bool running_ = false;
};
