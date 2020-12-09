#pragma once

#include <unordered_map>

#include <fmt/format.h>

#include "enet.hpp"
#include "serialization.hpp"

static constexpr size_t maxPlayers = 4;
static constexpr uint32_t protocolVersion = 1;
static constexpr size_t tickRate = 60;

using PlayerId = uint32_t;
static constexpr auto InvalidPlayerId = std::numeric_limits<PlayerId>::max();

enum class Channel : uint8_t {
    Unreliable = 0,
    Reliable = 1,
    Count,
};

uint32_t getChannelFlags(Channel channel);

struct CommonMessageHeader {
    uint8_t messageType;
    uint32_t frameNumber; // will not wrap in 130 years (60 fps)

    SERIALIZE()
    {
        FIELD(messageType);
        FIELD(frameNumber);
        SERIALIZE_END;
    }
};

enum class MessageType : uint8_t {
    ServerHello = 0,
    ClientMoveUpdate,
    ServerPlayerStateUpdate,
};

template <MessageType MsgType>
struct Message;

template <>
struct Message<MessageType::ServerHello> {
    uint32_t playerId;
    glm::vec3 spawnPos;

    SERIALIZE()
    {
        FIELD(playerId);
        FIELD(spawnPos);
        SERIALIZE_END;
    }
};

template <>
struct Message<MessageType::ClientMoveUpdate> {
    glm::vec3 position;
    glm::quat orientation;

    SERIALIZE()
    {
        FIELD(position);
        FIELD(orientation);
        SERIALIZE_END;
    }
};

template <>
struct Message<MessageType::ServerPlayerStateUpdate> {
    struct PlayerState {
        uint32_t id;
        glm::vec3 position;
        glm::quat orientation;

        SERIALIZE()
        {
            FIELD(id);
            FIELD(position);
            FIELD(orientation);
            SERIALIZE_END;
        }
    };

    std::vector<PlayerState> players;

    SERIALIZE()
    {
        FIELD_VEC(players);
        SERIALIZE_END;
    }
};

template <MessageType MsgType>
WriteBuffer serializeMessage(uint32_t frameNumber, Message<MsgType> message)
{
    WriteBuffer buffer(1024);
    CommonMessageHeader header { static_cast<uint8_t>(MsgType), frameNumber };
    if (!serialize(buffer, header)) {
        assert(false);
    }
    if (!serialize(buffer, message)) {
        assert(false);
    }
    return buffer;
}

template <MessageType MsgType>
bool sendMessage(
    ENetPeer* peer, Channel channel, uint32_t frameNumber, const Message<MsgType>& message)
{
    const auto buffer = serializeMessage(frameNumber, message);
    const auto packet
        = enet_packet_create(buffer.getData(), buffer.getSize(), getChannelFlags(channel));
    if (!packet) {
        fmt::print(stderr, "Could not create packet\n");
        return false;
    }
    const auto res = enet_peer_send(peer, static_cast<uint8_t>(channel), packet);
    if (res < 0) {
        fmt::print(stderr, "Error sending message of type {}\n", MsgType);
        return false;
    }
    return true;
}
