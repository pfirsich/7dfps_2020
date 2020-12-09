#pragma once

#include <atomic>
#include <string>
#include <vector>

#include "ecs.hpp"
#include "net.hpp"

class Server {
public:
    Server() = default;

    // this blocks until you call stop
    bool run(const std::string& host, Port port);

    bool isRunning() const;

    void stop();

private:
    struct Player {
        ecs::EntityHandle entity;
        ENetPeer* peer;
        PlayerId id;

        static PlayerId getNextId();

        Player(ENetPeer* peer);
    };

    template <MessageType MsgType>
    void broadcast(Channel channel, const Message<MsgType>& message)
    {
        const auto buffer = serializeMessage(frameCounter_, message);
        host_.broadcast(static_cast<uint8_t>(channel),
            enet::Packet(buffer.getData(), buffer.getSize(), getChannelFlags(channel)));
    }

    template <MessageType MsgType>
    bool send(Player& player, Channel channel, const Message<MsgType>& message)
    {
        return sendMessage(player.peer, channel, frameCounter_, message);
    }

    void processEnetEvents();
    void tick(float dt);
    void broadcastUpdate();

    size_t getPlayerIndex(PlayerId id) const;
    PlayerId getPlayerId(const void* peerData) const;
    void connectPeer(ENetPeer* peer);
    void disconnectPlayer(PlayerId id);
    void receive(PlayerId id, const enet::Packet& packet);
    void findSpawnPosition(Player& player);

    template <MessageType MsgType>
    void processMessage(Player& player, uint32_t frameNumber, ReadBuffer& buffer)
    {
        Message<MsgType> message;
        if (!deserialize(buffer, message)) {
            fmt::print(stderr, "Could not decode message of type {}\n", MsgType);
            return;
        }
        processMessage(player, frameNumber, message);
    }

    void processMessage(Player& player, uint32_t frameNumber,
        const Message<MessageType::ClientMoveUpdate>& message);

    enet::Host host_;
    ecs::World world_;
    std::vector<Player> players_;
    float time_ = 0.0f;
    uint32_t frameCounter_ = 0;
    std::atomic<bool> running_ { false };
    bool started_ = false;
};
