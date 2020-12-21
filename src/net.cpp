#include "net.hpp"

std::string asString(MessageType messageType)
{
    switch (messageType) {
    case MessageType::ServerHello:
        return "ServerHello";
    case MessageType::ClientMoveUpdate:
        return "ClientMoveUpdate";
    case MessageType::ServerPlayerStateUpdate:
        return "ServerPlayerStateUpdate";
    case MessageType::ClientInteractTerminal:
        return "ClientInteractTerminal";
    case MessageType::ServerInteractTerminal:
        return "ServerInteractTerminal";
    case MessageType::ClientUpdateTerminalInput:
        return "ClientUpdateTerminalInput";
    case MessageType::ClientExecuteCommand:
        return "ClientExecuteCommand";
    case MessageType::ServerUpdateTerminalOutput:
        return "ServerUpdateTerminalOutput";
    case MessageType::ServerAddTerminalHistory:
        return "ServerAddTerminalHistory";
    case MessageType::ClientPlaySound:
        return "ClientPlaySound";
    case MessageType::ServerUpdateInputEnabled:
        return "ServerUpdateInputEnabled";
    default:
        return fmt::format("Unknown({})", static_cast<uint8_t>(messageType));
    }
}

uint32_t getChannelFlags(Channel channel)
{
    switch (channel) {
    case Channel::Unreliable:
        return ENET_PACKET_FLAG_UNSEQUENCED;
    case Channel::Reliable:
        return ENET_PACKET_FLAG_RELIABLE;
    default:
        std::abort();
    }
}
