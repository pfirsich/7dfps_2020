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

bool Server::run(const std::string& host, Port port, uint32_t gameCode, float exitTimeout)
{
    assert(!started_);
    started_ = true;

    connectCode_ = getConnectCode(gameCode);
    exitTimeout_ = exitTimeout;

    println("Loading map..");

    auto shipGltf = GltfFile::load("media/ship.glb");
    if (!shipGltf) {
        printErr("Could not load 'media/ship.glb'");
        return false;
    }
    shipGltf->instantiate(world_, true);
    world_.flush();

    println("Done");

    shipSystems_.emplace("reactor",
        ShipSystemData { std::make_unique<LuaShipSystem>("reactor", "media/systems/reactor.lua") });
    shipSystems_.emplace("engine",
        ShipSystemData { std::make_unique<LuaShipSystem>("engine", "media/systems/engine.lua") });
    shipSystems_.emplace(
        "nav", ShipSystemData { std::make_unique<LuaShipSystem>("nav", "media/systems/nav.lua") });
    shipSystems_.emplace("shields",
        ShipSystemData { std::make_unique<LuaShipSystem>("shields", "media/systems/shields.lua") });
    shipSystems_.emplace(
        "o2", ShipSystemData { std::make_unique<LuaShipSystem>("o2", "media/systems/o2.lua") });

    const auto addr = enet::getAddress(host, port);
    if (!addr) {
        printErr("Could not get address");
        return false;
    }

    host_ = enet::Host(*addr, maxPlayers, static_cast<uint8_t>(Channel::Count));
    if (!host_) {
        printErr("Could not create server host");
        return false;
    }

    println("Listening on {}:{}..", host, port);

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
            host_.flush();
            accumulator -= dt;
            time_ += dt;
            frameCounter_++;
        }
        SDL_Delay(1);
    }

    MessageBus::instance().clearEndpoints();

    for (auto& player : players_)
        enet_peer_disconnect_now(player.peer, 0);

    return true;
}

void Server::tick(float /*dt*/)
{
    for (auto& [name, system] : shipSystems_) {
        system.system->update();

        const auto terminalEnabled = !system.system->commandRunning();

        const auto totalOutputSize = system.system->getTotalTerminalOutputSize();
        const auto& output = system.system->getTerminalOutput();
        for (auto& player : players_) {
            const auto lastKnownTermSize = player.lastKnownTerminalSize[name];
            assert(totalOutputSize >= lastKnownTermSize);
            const auto deltaLength = totalOutputSize - lastKnownTermSize;
            if (deltaLength > 0) {
                const auto maxDeltaLength = std::min(deltaLength, output.size());
                const auto delta = output.substr(output.size() - maxDeltaLength);
                send(player, Channel::Reliable,
                    Message<MessageType::ServerUpdateTerminalOutput> { name, delta });
                player.lastKnownTerminalSize[name] = totalOutputSize;
            }

            if (terminalEnabled != player.lastKnownTerminalEnabled[name]) {
                send(player, Channel::Reliable,
                    Message<MessageType::ServerUpdateInputEnabled> { name, terminalEnabled });
                player.lastKnownTerminalEnabled[name] = terminalEnabled;
            }

            const auto deltaHist = std::min(
                system.history.size(), system.historyCount - player.lastKnownHistoryCount[name]);
            if (deltaHist > 0) {
                Message<MessageType::ServerAddTerminalHistory> message { name, {} };
                message.commands.reserve(deltaHist);
                for (size_t i = system.history.size() - deltaHist; i < system.history.size(); ++i) {
                    message.commands.push_back(system.history[i]);
                }
                send(player, Channel::Reliable, message);
                player.lastKnownHistoryCount[name] = system.historyCount;
            }
        }
    }

    auto update = Message<MessageType::ServerPlayerStateUpdate>();
    for (auto& player : players_) {
        const auto& trafo = player.entity.get<comp::Transform>();
        update.players.push_back(Message<MessageType::ServerPlayerStateUpdate>::PlayerState {
            player.id, trafo.getPosition(), trafo.getOrientation() });
    }
    broadcast(Channel::Unreliable, update);

    if (players_.empty()) {
        if (time_ - lastNonEmpty_ > exitTimeout_) {
            println("Exit timeout reached");
            running_.store(false);
        }
    } else {
        lastNonEmpty_ = time_;
    }
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
            if (connEvent->data != connectCode_) {
                // disconnect now, so peer is reset and we have a free slot for another
                // client!
                enet_peer_disconnect_now(connEvent->peer, version);
            } else {
                connectPeer(connEvent->peer);
            }
        } else if (const auto discEvent = std::get_if<enet::DisconnectEvent>(&event.value())) {
            disconnectPlayer(getPlayerId(discEvent->peerData));
        } else if (const auto recvEvent = std::get_if<enet::ReceiveEvent>(&event.value())) {
            receive(getPlayerId(recvEvent->peer->data), recvEvent->channelId, recvEvent->packet);
        } else if (const auto errEvent = std::get_if<enet::ServiceFailedEvent>(&event.value())) {
            printErr("Host service failed: {}", errEvent->result);
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
    static std::vector<glwx::Transform> spawnPoints;
    if (spawnPoints.empty()) {
        world_.forEachEntity<comp::SpawnPoint, comp::Transform>(
            [](ecs::EntityHandle entity, const comp::SpawnPoint&, const comp::Transform& trafo) {
                spawnPoints.push_back(trafo);
                const auto pos = trafo.getPosition();
                const auto y = std::floor(pos.y / floorHeight) * floorHeight;
                spawnPoints.back().setPosition(glm::vec3(pos.x, y, pos.z));
            });
        if (spawnPoints.empty()) {
            printErr("No spawn points in level");
            std::abort();
        }
    }
    auto& trafo = player.entity.get<comp::Transform>();
    const auto& collider = player.entity.get<comp::CylinderCollider>();
    for (const auto& pos : spawnPoints) {
        trafo = pos;
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
    println("Client connected from {}: id = {}", ip, player.id);
    player.entity = world_.createEntity();
    player.entity.add<comp::Name>(comp::Name { "player_" + std::to_string(player.id) });
    player.entity.add<comp::NetworkPlayer>();
    player.entity.add<comp::CylinderCollider>(
        comp::CylinderCollider { playerRadius, cameraOffsetY });
    const auto& trafo = player.entity.add<comp::Transform>();
    world_.flush();
    findSpawnPosition(player);
    send(player, Channel::Reliable,
        Message<MessageType::ServerHello> {
            player.id, trafo.getPosition(), trafo.getOrientation() });
}

void Server::disconnectPlayer(PlayerId id)
{
    const auto idx = getPlayerIndex(id);
    players_[idx].entity.destroy();
    players_.erase(players_.begin() + idx);
    world_.flush();
    println("Client disconnected (id = {})", id);
    const auto terminal = getUsedTerminal(id);
    if (terminal) {
        shipSystems_.at(*terminal).terminalUser = InvalidPlayerId;
    }
}

#define MESSAGE_CASE(Type)                                                                         \
    case MessageType::Type:                                                                        \
        processMessage<MessageType::Type>(player, header.frameNumber, buffer);                     \
        break;

void Server::receive(PlayerId id, uint8_t channelId, const enet::Packet& packet)
{
    auto& player = players_[getPlayerIndex(id)];
    ReadBuffer buffer(packet.getData<uint8_t>(), packet.getSize());
    CommonMessageHeader header;
    if (!deserialize(buffer, header)) {
        printErr("Could not decode common message header");
        return; // Ignore message
    }
    const auto messageType = static_cast<MessageType>(header.messageType);
    if (static_cast<Channel>(channelId) == Channel::Reliable) {
        // println("[server] Received message: {}", asString(messageType));
    }
    switch (messageType) {
        MESSAGE_CASE(ClientMoveUpdate);
        MESSAGE_CASE(ClientInteractTerminal);
        MESSAGE_CASE(ClientUpdateTerminalInput);
        MESSAGE_CASE(ClientExecuteCommand);
        MESSAGE_CASE(ClientPlaySound);
    default:
        printErr("Received unrecognized message: {}", asString(messageType));
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

std::optional<std::string> Server::getUsedTerminal(PlayerId id) const
{
    for (const auto& [name, system] : shipSystems_) {
        if (system.terminalUser == id)
            return name;
    }
    return std::nullopt;
}

void Server::processMessage(Player& player, uint32_t /*frameNumber*/,
    const Message<MessageType::ClientInteractTerminal>& message)
{
    if (message.terminal.empty()) {
        const auto terminal = getUsedTerminal(player.id);
        if (!terminal) {
            printErr("Player {} stopped using a terminal when no terminal was used.", player.id);
            return;
        }
        shipSystems_.at(*terminal).terminalUser = InvalidPlayerId;
    } else {
        const auto it = shipSystems_.find(message.terminal);
        if (it == shipSystems_.end())
            return; // garbage, do nothing

        if (it->second.terminalUser == InvalidPlayerId) { // terminal not used
            it->second.terminalUser = player.id;
            send(player, Channel::Reliable,
                Message<MessageType::ServerInteractTerminal> { message.terminal });
            if (!it->second.initialized) {
                it->second.system->executeInternalCommand("internal_init");
                it->second.initialized = true;
            }
        }
    }
}

void Server::processMessage(Player& player, uint32_t /*frameNumber*/,
    const Message<MessageType::ClientUpdateTerminalInput>& message)
{
    const auto terminal = getUsedTerminal(player.id);
    if (!terminal) {
        printErr("Player {} sent a terminal update without using a terminal", player.id);
        return;
    }
    shipSystems_.at(*terminal).terminalInput = message.input;
}

void Server::processMessage(Player& player, uint32_t /*frameNumber*/,
    const Message<MessageType::ClientExecuteCommand>& message)
{
    const auto systemName = getUsedTerminal(player.id);
    if (!systemName) {
        printErr("Player {} executed a command on a terminal update without using a terminal",
            player.id);
        return;
    }
    auto& system = shipSystems_.at(*systemName);

    if (!message.command.empty()) {
        system.history.push_back(message.command);
        system.historyCount++;
        while (system.history.size() > 32) {
            system.history.pop_front();
        }
    }

    system.system->executeCommand(message.command);
}

void Server::processMessage(
    Player& player, uint32_t /*frameNumber*/, const Message<MessageType::ClientPlaySound>& message)
{
    distribute(player, Channel::Reliable, message);
}
