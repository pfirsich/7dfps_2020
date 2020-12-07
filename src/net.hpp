#pragma once

#include <unordered_map>

#include <fmt/format.h>

#include "enet.hpp"
#include "serialization.hpp"

static constexpr size_t maxPlayers = 4;
static constexpr uint32_t protocolVersion = 1;

enum class Channel : uint8_t {
    Unreliable = 0,
    Reliable = 1,
    Count,
};

uint32_t getChannelFlags(Channel channel);

enum class MessageType : uint8_t {
    Hello = 0,
    ClientMoveUpdate,
    ServerPlayerStateUpdate,
};

template <MessageType MsgType>
struct Message;

template <>
struct Message<MessageType::Hello> {
    uint32_t guid;

    SERIALIZE()
    {
        FIELD(guid);
        SERIALIZE_END;
    }
};

template <>
struct Message<MessageType::ClientMoveUpdate> {
    uint8_t inputs; // bitmask
    glm::quat orientation;

    SERIALIZE()
    {
        FIELD(inputs);
        FIELD(orientation);
        SERIALIZE_END;
    }
};

template <>
struct Message<MessageType::ServerPlayerStateUpdate> {
    struct PlayerState {
        uint32_t guid;
        glm::vec3 position;
        glm::quat orientation;

        SERIALIZE()
        {
            FIELD(guid);
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
std::optional<Message<MsgType>> deserializeMessage(const enet::Packet& packet)
{
    ReadBuffer buf(packet.getData<uint8_t>(), packet.getSize());
    Message<MsgType> message;
    if (!deserialize(buf, message)) {
        fmt::print(stderr, "Could not deserialize message of type {}", static_cast<int>(MsgType));
        return std::nullopt;
    }
    return message;
}

template <MessageType MsgType>
WriteBuffer serializeMessage(Message<MsgType> message)
{
    WriteBuffer buffer(1024);
    assert(serialize(buffer, message));
    return buffer;
}
