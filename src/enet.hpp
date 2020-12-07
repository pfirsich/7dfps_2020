#pragma once

#include <optional>
#include <variant>

#include <enet/enet.h>

using Port = uint16_t;

namespace enet {

std::optional<ENetAddress> getAddress(const std::string& host, Port port);
std::optional<std::string> getIp(const ENetAddress& addr);
std::optional<std::string> getHostname(const ENetAddress& addr);

class Packet {
public:
    Packet(ENetPacket* packet);

    Packet(std::string_view data, uint32_t flags);

    template <typename T>
    Packet(const T* data, size_t size, uint32_t flags)
        : packet_(enet_packet_create(data, size, flags))
    {
    }

    Packet(Packet&& other);

    Packet& operator=(Packet&& other);

    Packet(const Packet&) = delete;
    Packet& operator=(const Packet&) = delete;

    ~Packet();

    template <typename T>
    const T* getData() const
    {
        return reinterpret_cast<const T*>(packet_->data);
    }

    size_t getSize() const;

    std::string_view getView() const;

    // TODO: userData accessor?

    ENetPacket* get();

    ENetPacket* release();

private:
    ENetPacket* packet_ = nullptr;
};

struct ConnectEvent {
    ENetPeer* peer;
    uint32_t data;
};

struct DisconnectEvent {
    void* peerData;
    uint32_t data;
};

struct ReceiveEvent {
    ENetPeer* peer;
    uint8_t channelId;
    Packet packet;
};

struct ServiceFailedEvent {
    int result;
};

using Event = std::variant<ConnectEvent, DisconnectEvent, ReceiveEvent, ServiceFailedEvent>;

class Host {
public:
    Host() = default;

    Host(const ENetAddress& addr, size_t maxClients, size_t channelCount, uint32_t inBandwidth = 0,
        uint32_t outBandwidth = 0);

    Host(size_t channelCount, uint32_t inBandwidth = 0, uint32_t outBandwidth = 0);

    Host(Host&& other);

    Host& operator=(Host&& other);

    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    ~Host();

    void broadcast(uint8_t channel, Packet&& packet);

    ENetPeer* connect(const ENetAddress& addr, size_t channelCount, uint32_t data = 0);

    explicit operator bool() const;

    ENetHost* get() const;

    ENetHost* release();

    std::optional<Event> service(uint32_t timeoutMs = 0);

    void flush();

    bool compressWithRangeCoder();

private:
    ENetHost* host_ = nullptr;
};

}
