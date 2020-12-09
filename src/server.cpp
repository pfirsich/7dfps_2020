#include "server.hpp"

#include <cassert>

#include <fmt/format.h>

#include <glwx.hpp>

#include "constants.hpp"
#include "gltfimport.hpp"
#include "physics.hpp"

namespace {
}

namespace comp {
struct NetworkPlayer {
    uint32_t lastUpdatedFrame = 0;
};
}

bool Server::run(const std::string& host, Port port)
{
    assert(!started_);
    started_ = true;

    fmt::print("Loading map..\n");

    if (!loadMap("media/ship.glb", world_, true))
        return false;
    world_.flush();

    fmt::print("Done\n");

    const auto addr = enet::getAddress(host, port);
    if (!addr) {
        fmt::print(stderr, "Could not get address\n");
        return false;
    }

    host_ = enet::Host(*addr, maxPlayers, static_cast<uint8_t>(Channel::Count));
    if (!host_) {
        fmt::print(stderr, "Could not create server host\n");
        return false;
    }

    fmt::print("Listening on {}:{}..\n", host, port);

    running_.store(true);
    time_ = 0.0f;
    float clockTime = glwx::getTime();
    float accumulator = 0.0f;
    constexpr auto dt = 1.0f / tickRate;
    while (running_.load()) {
        const auto now = glwx::getTime();
        const auto clockDelta = now - clockTime;
        clockTime = now;

        accumulator += clockDelta;
        while (accumulator >= dt) {
            processEnetEvents();
            tick(dt);
            broadcastUpdate();
            host_.flush();
            accumulator -= dt;
            time_ += dt;
            frameCounter_++;
        }
    }

    for (auto& player : players_)
        enet_peer_disconnect_now(player.peer, 0);

    return true;
}

void Server::tick(float /*dt*/)
{
    // do nothing for now
}

void Server::broadcastUpdate()
{
    auto update = Message<MessageType::ServerPlayerStateUpdate>();
    for (auto& player : players_) {
        const auto& trafo = player.entity.get<comp::Transform>();
        update.players.push_back(Message<MessageType::ServerPlayerStateUpdate>::PlayerState {
            player.id, trafo.getPosition(), trafo.getOrientation() });
    }
    broadcast(Channel::Unreliable, update);
}

bool Server::isRunning() const
{
    return running_.load();
}

void Server::stop()
{
    running_.store(false);
}

void Server::processEnetEvents()
{
    std::optional<enet::Event> event;
    while ((event = host_.service())) {
        if (const auto connEvent = std::get_if<enet::ConnectEvent>(&event.value())) {
            // note peer->connectID is just a random value generated on the peer
            if (connEvent->data != protocolVersion) {
                // disconnect now, so peer is reset and we have a free slot for another
                // client!
                enet_peer_disconnect_now(connEvent->peer, protocolVersion);
            } else {
                connectPeer(connEvent->peer);
            }
        } else if (const auto discEvent = std::get_if<enet::DisconnectEvent>(&event.value())) {
            disconnectPlayer(getPlayerId(discEvent->peerData));
        } else if (const auto recvEvent = std::get_if<enet::ReceiveEvent>(&event.value())) {
            receive(getPlayerId(recvEvent->peer->data), recvEvent->packet);
        } else if (const auto errEvent = std::get_if<enet::ServiceFailedEvent>(&event.value())) {
            fmt::print(stderr, "Host service failed: {}\n", errEvent->result);
        }
    }
}

PlayerId Server::Player::getNextId()
{
    static PlayerId idCounter = 0;
    return idCounter++;
}

Server::Player::Player(ENetPeer* peer)
    : peer(peer)
    , id(getNextId())
{
}

size_t Server::getPlayerIndex(PlayerId id) const
{
    for (size_t i = 0; i < players_.size(); ++i)
        if (players_[i].id == id)
            return i;
    std::abort();
}

PlayerId Server::getPlayerId(const void* peerData) const
{
    return static_cast<PlayerId>(reinterpret_cast<uintptr_t>(peerData));
}

void Server::findSpawnPosition(Player& player)
{
    static constexpr std::array spawnPoints {
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(5.0f, 0.0f, 5.0f),
        glm::vec3(-5.0f, 0.0f, 5.0f),
        glm::vec3(-5.0f, 0.0f, -5.0f),
        glm::vec3(5.0f, 0.0f, -5.0f),
    };
    auto& trafo = player.entity.get<comp::Transform>();
    const auto& collider = player.entity.get<comp::CylinderCollider>();
    for (const auto& pos : spawnPoints) {
        trafo.setPosition(pos);
        if (!findFirstCollision(world_, player.entity, trafo, collider)) {
            return;
        }
    }
}

void Server::connectPeer(ENetPeer* peer)
{
    auto& player = players_.emplace_back(peer);
    peer->data = reinterpret_cast<void*>(player.id);
    const auto ip = enet::getIp(peer->address).value();
    fmt::print("Client connected from {}: id = {}\n", ip, player.id);
    player.entity = world_.createEntity();
    player.entity.add<comp::Name>(comp::Name { "player_" + std::to_string(player.id) });
    player.entity.add<comp::NetworkPlayer>();
    player.entity.add<comp::CylinderCollider>(
        comp::CylinderCollider { playerRadius, cameraOffsetY });
    const auto& trafo = player.entity.add<comp::Transform>();
    world_.flush();
    findSpawnPosition(player);
    send(player, Channel::Reliable,
        Message<MessageType::ServerHello> { player.id, trafo.getPosition() });
}

void Server::disconnectPlayer(PlayerId id)
{
    const auto idx = getPlayerIndex(id);
    players_[idx].entity.destroy();
    players_.erase(players_.begin() + idx);
    world_.flush();
    fmt::print("Client disconnected (id = {})\n", id);
}

#define MESSAGE_CASE(Type)                                                                         \
    case MessageType::Type:                                                                        \
        processMessage<MessageType::Type>(player, header.frameNumber, buffer);                     \
        break;

void Server::receive(PlayerId id, const enet::Packet& packet)
{
    auto& player = players_[getPlayerIndex(id)];
    ReadBuffer buffer(packet.getData<uint8_t>(), packet.getSize());
    CommonMessageHeader header;
    if (!deserialize(buffer, header)) {
        fmt::print(stderr, "Could not decode common message header\n");
        return; // Ignore message
    }
    switch (static_cast<MessageType>(header.messageType)) {
        MESSAGE_CASE(ClientMoveUpdate);
    default:
        fmt::print(stderr, "Received unrecognized message\n");
    }
}

void Server::processMessage(
    Player& player, uint32_t frameNumber, const Message<MessageType::ClientMoveUpdate>& message)
{
    auto& net = player.entity.get<comp::NetworkPlayer>();
    if (net.lastUpdatedFrame < frameNumber) {
        auto& trafo = player.entity.get<comp::Transform>();
        trafo.setPosition(message.position);
        trafo.setOrientation(message.orientation);
        net.lastUpdatedFrame = frameNumber;
    }
}
