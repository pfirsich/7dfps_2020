#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <enet/enet.h>

using Port = uint16_t;

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

class Packet {
public:
    enum class Delivery : uint32_t {
        Reliable = ENET_PACKET_FLAG_RELIABLE,
        Unsequenced = ENET_PACKET_FLAG_UNSEQUENCED,
    };

    Packet(ENetPacket* packet)
        : packet_(packet)
    {
    }

    template <typename T>
    Packet(const T* data, size_t size, Delivery delivery)
        : packet_(enet_packet_create(data, size, static_cast<uint32_t>(delivery)))
    {
    }

    Packet(std::string_view data, Delivery delivery)
        : packet_(enet_packet_create(data.data(), data.size(), static_cast<uint32_t>(delivery)))
    {
    }

    Packet(const Packet&) = delete;

    Packet(Packet&& other)
        : packet_(other.packet_)
    {
        other.packet_ = nullptr;
    }

    Packet& operator=(Packet&& other)
    {
        packet_ = other.packet_;
        other.packet_ = nullptr;
        return *this;
    }

    ~Packet()
    {
        if (packet_)
            enet_packet_destroy(packet_);
    }

    template <typename T>
    const T* getData() const
    {
        return reinterpret_cast<const T*>(packet_->data);
    }

    size_t getSize() const
    {
        return packet_->dataLength;
    }

    std::string_view getView() const
    {
        return std::string_view(getData<char>(), getSize());
    }

    // TODO: userData accessor?

    ENetPacket* get()
    {
        return packet_;
    }

    ENetPacket* release()
    {
        auto ret = packet_;
        packet_ = nullptr;
        return ret;
    }

private:
    ENetPacket* packet_ = nullptr;
};

struct ConnectEvent {
    ENetPeer* peer;
    uint32_t data;
};

struct DisconnectEvent {
    void* peerData;
};

struct ReceiveEvent {
    ENetPeer* peer;
    uint8_t channelId;
    Packet packet;
};

using Event = std::variant<ConnectEvent, DisconnectEvent, ReceiveEvent>;

class Host {
public:
    Host(const ENetAddress& addr, size_t maxClients, size_t channelCount, uint32_t inBandwidth,
        uint32_t outBandwidth)
        : host_(enet_host_create(&addr, maxClients, channelCount, inBandwidth, outBandwidth))
    {
    }

    Host(size_t channelCount, uint32_t inBandwidth, uint32_t outBandwidth)
        : host_(enet_host_create(nullptr, 1, channelCount, inBandwidth, outBandwidth))
    {
    }

    ~Host()
    {
        if (host_)
            enet_host_destroy(host_);
    }

    ENetPeer* connect(const ENetAddress& addr, size_t channelCount, uint32_t data = 0)
    {
        return enet_host_connect(host_, &addr, channelCount, data);
    }

    explicit operator bool() const
    {
        return host_ != nullptr;
    }

    ENetHost* get() const
    {
        return host_;
    }

    std::optional<Event> service(uint32_t timeoutMs = 0)
    {
        ENetEvent event;
        const auto res = enet_host_service(host_, &event, timeoutMs);
        if (res < 0) {
            std::cerr << "host_service failed." << std::endl;
            return std::nullopt;
        }
        if (res == 0)
            return std::nullopt;
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
            return ConnectEvent { event.peer, event.data };
            break;
        case ENET_EVENT_TYPE_DISCONNECT: {
            void* data = event.peer->data;
            event.peer->data = nullptr;
            return DisconnectEvent { data };
            break;
        }
        case ENET_EVENT_TYPE_RECEIVE:
            return ReceiveEvent { event.peer, event.channelID, Packet(event.packet) };
            break;
        default:
            std::cerr << "Unknown event type." << std::endl;
            return std::nullopt;
        }
    }

    void flush()
    {
        enet_host_flush(host_);
    }

    bool compressWithRangeCoder()
    {
        return enet_host_compress_with_range_coder(host_) == 0;
    }

private:
    ENetHost* host_ = nullptr;
};

int server(const std::string& host, Port port)
{
    const auto addr = getAddress(host, port);
    if (!addr) {
        std::cerr << "Could not get address." << std::endl;
        return 1;
    }

    Host server(*addr, 32, 2, 0, 0);
    if (!server) {
        std::cerr << "Could not create server host." << std::endl;
        return 1;
    }

    while (true) {
        std::optional<Event> event;
        while ((event = server.service())) {
            if (const auto connect = std::get_if<ConnectEvent>(&event.value())) {
                std::cout << "Client connected: " << connect->peer << std::endl;
            } else if (const auto disconnect = std::get_if<DisconnectEvent>(&event.value())) {
                // pass
            } else if (const auto receive = std::get_if<ReceiveEvent>(&event.value())) {
                std::cout << "Receive: " << receive->packet.getView() << std::endl;
                enet_peer_send(
                    receive->peer, 0, Packet("server data", Packet::Delivery::Reliable).release());
            }
        }
    }
}

int client(const std::string& host, Port port)
{
    Host client(2, 0, 0);
    if (!client) {
        std::cerr << "Could not create client host." << std::endl;
        return 1;
    }

    const auto addr = getAddress(host, port);
    if (!addr) {
        std::cerr << "Could not get address." << std::endl;
        return 1;
    }

    auto peer = client.connect(*addr, 2, 69);
    if (!peer) {
        std::cerr << "Could not connect." << std::endl;
        return 1;
    }

    const auto event = client.service(5000);
    if (event && std::holds_alternative<ConnectEvent>(*event)) {
        // const auto& connectEvent = std::get<ConnectEvent>(*event);
        std::cout << "Connected." << std::endl;
    } else {
        std::cerr << "Connection failed." << std::endl;
        enet_peer_reset(peer);
        return 1;
    }

    auto nextSend = std::chrono::steady_clock::now();
    while (true) {
        std::optional<Event> event;
        while ((event = client.service())) {
            if (const auto receive = std::get_if<ReceiveEvent>(&event.value())) {
                std::cout << "Received: " << receive->packet.getView() << std::endl;
            } else if (const auto disconnect = std::get_if<DisconnectEvent>(&event.value())) {
                std::cout << "Disconnected." << std::endl;
                return 0;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (nextSend < now) {
            nextSend = now + std::chrono::seconds(1);
            enet_peer_send(peer, 0, Packet("client data", Packet::Delivery::Reliable).release());
            client.flush();
        }
    }
}

int main(int argc, char** argv)
{
    const auto args = std::vector<std::string>(argv + 1, argv + argc);
    if (args.size() < 3 || (args[0] != "server" && args[0] != "client")) {
        std::cerr << "Usage:\nenet-test server <host> <port> # host may be \"any\"\nor:"
                  << "\nUsage: enet-test client <host> <port>" << std::endl;
        return 1;
    }

    if (enet_initialize()) {
        std::cerr << "Could not initialize ENet" << std::endl;
        return 1;
    }
    atexit(enet_deinitialize);

    const Port port = std::stoi(args[2]);
    if (args[0] == "server") {
        return server(args[1], port);
    } else if (args[0] == "client") {
        return client(args[1], port);
    }
    return 1;
}
