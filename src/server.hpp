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
    struct Client {
        using Id = uint32_t;

        Id guid;
        ENetPeer* peer;

        static Id getNextId();

        Client(ENetPeer* peer);

        template <MessageType MsgType>
        void send(const Message<MsgType>& message, Channel channel)
        {
            const auto buffer = serializeMessage(message);
            if (enet_peer_send(peer, static_cast<uint8_t>(channel),
                    enet::Packet(buffer.getData(), buffer.getSize(), getChannelFlags(channel))
                        .release())
                < 0) {
                fmt::print(stderr, "Error sending message of type {}", MsgType);
            }
        }
    };

    template <MessageType MsgType>
    void broadcast(const Message<MsgType>& message, Channel channel)
    {
        const auto buffer = serializeMessage(message);
        host_.broadcast(static_cast<uint8_t>(channel),
            enet::Packet(buffer.getData(), buffer.getSize(), getChannelFlags(channel)));
    }

    size_t getClientIndex(Client::Id guid) const;
    Client::Id getGuid(const void* peerData) const;
    void connectPeer(ENetPeer* peer);
    void disconnectClient(Client::Id guid);
    void receive(Client::Id guid, const enet::Packet& packet);

    template <MessageType MsgType>
    void processMessage(Client& client, const enet::Packet& packet)
    {
        const auto msgOpt = deserializeMessage<MsgType>(packet);
        if (!msgOpt)
            return;
        processMessage(client, *msgOpt);
    }

    void processMessage(Client& client, const Message<MessageType::Hello>& msg);
    void processMessage(Client& client, const Message<MessageType::ClientMoveUpdate>& msg);

    enet::Host host_;
    ecs::World world_;
    std::vector<Client> clients_;
    std::atomic<bool> running_ { false };
    bool started_ = false;
};
