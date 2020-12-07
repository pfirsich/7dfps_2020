#include "enet.hpp"

#include <cstdlib>

namespace enet {

std::optional<ENetAddress> getAddress(const std::string& host, Port port)
{
    ENetAddress address;
    if (host == "any")
        address.host = ENET_HOST_ANY;
    else if (enet_address_set_host(&address, host.c_str()) < 0)
        return std::nullopt;
    address.port = port;
    return address;
}

std::optional<std::string> getIp(const ENetAddress& addr)
{
    char buffer[16];
    if (enet_address_get_host_ip(&addr, buffer, sizeof(buffer)) < 0)
        return std::nullopt;
    return std::string(buffer);
}

std::optional<std::string> getHostname(const ENetAddress& addr)
{
    char buffer[256];
    if (enet_address_get_host(&addr, buffer, sizeof(buffer)) < 0)
        return std::nullopt;
    return std::string(buffer);
}

Packet::Packet(ENetPacket* packet)
    : packet_(packet)
{
}

Packet::Packet(std::string_view data, uint32_t flags)
    : packet_(enet_packet_create(data.data(), data.size(), flags))
{
}

Packet::Packet(Packet&& other)
    : packet_(other.packet_)
{
    other.packet_ = nullptr;
}

Packet& Packet::operator=(Packet&& other)
{
    packet_ = other.packet_;
    other.packet_ = nullptr;
    return *this;
}

Packet::~Packet()
{
    if (packet_)
        enet_packet_destroy(packet_);
}

size_t Packet::getSize() const
{
    return packet_->dataLength;
}

std::string_view Packet::getView() const
{
    return std::string_view(getData<char>(), getSize());
}

ENetPacket* Packet::get()
{
    return packet_;
}

ENetPacket* Packet::release()
{
    auto ret = packet_;
    packet_ = nullptr;
    return ret;
}

Host::Host(const ENetAddress& addr, size_t maxClients, size_t channelCount, uint32_t inBandwidth,
    uint32_t outBandwidth)
    : host_(enet_host_create(&addr, maxClients, channelCount, inBandwidth, outBandwidth))
{
}

Host::Host(size_t channelCount, uint32_t inBandwidth, uint32_t outBandwidth)
    : host_(enet_host_create(nullptr, 1, channelCount, inBandwidth, outBandwidth))
{
}

Host::Host(Host&& other)
    : host_(other.host_)
{
    other.host_ = nullptr;
}

Host& Host::operator=(Host&& other)
{
    host_ = other.host_;
    other.host_ = nullptr;
    return *this;
}

Host::~Host()
{
    if (host_)
        enet_host_destroy(host_);
}

void Host::broadcast(uint8_t channel, Packet&& packet)
{
    enet_host_broadcast(host_, channel, packet.release());
}

ENetPeer* Host::connect(const ENetAddress& addr, size_t channelCount, uint32_t data)
{
    return enet_host_connect(host_, &addr, channelCount, data);
}

Host::operator bool() const
{
    return host_ != nullptr;
}

ENetHost* Host::get() const
{
    return host_;
}

ENetHost* Host::release()
{
    auto ret = host_;
    host_ = nullptr;
    return ret;
}

std::optional<Event> Host::service(uint32_t timeoutMs)
{
    ENetEvent event;
    const auto res = enet_host_service(host_, &event, timeoutMs);
    if (res < 0)
        return ServiceFailedEvent { res };
    if (res == 0)
        return std::nullopt;
    switch (event.type) {
    case ENET_EVENT_TYPE_CONNECT:
        return ConnectEvent { event.peer, event.data };
    case ENET_EVENT_TYPE_DISCONNECT: {
        void* data = event.peer->data;
        event.peer->data = nullptr;
        return DisconnectEvent { data, event.data };
    }
    case ENET_EVENT_TYPE_RECEIVE:
        return ReceiveEvent { event.peer, event.channelID, Packet(event.packet) };
    default:
        std::abort();
    }
}

void Host::flush()
{
    enet_host_flush(host_);
}

bool Host::compressWithRangeCoder()
{
    return enet_host_compress_with_range_coder(host_) == 0;
}
}
