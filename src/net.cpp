#include "net.hpp"

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
