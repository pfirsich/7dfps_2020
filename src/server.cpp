#include "server.hpp"

#include <cassert>

#include <fmt/format.h>

namespace {
}

bool Server::run(const std::string& host, Port port)
{
    assert(!started_);
    started_ = true;

    const auto addr = enet::getAddress(host, port);
    if (!addr) {
        fmt::print(stderr, "Could not get address.");
        return false;
    }

    host_ = enet::Host(*addr, maxPlayers, static_cast<uint8_t>(Channel::Count));
    if (!host_) {
        fmt::print(stderr, "Could not create server host.");
        return false;
    }

    running_.store(true);
    while (running_.load()) {
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
                disconnectClient(getGuid(discEvent->peerData));
            } else if (const auto recvEvent = std::get_if<enet::ReceiveEvent>(&event.value())) {
                receive(getGuid(recvEvent->peer->data), recvEvent->packet);
            } else if (const auto errEvent
                = std::get_if<enet::ServiceFailedEvent>(&event.value())) {
                fmt::print(stderr, "Host service failed: {}", errEvent->result);
            }
        }
    }

    // TODO: Disconnect remaining clients

    return true;
}

bool Server::isRunning() const
{
    return running_.load();
}

void Server::stop()
{
    running_.store(false);
}

Server::Client::Id Server::Client::getNextId()
{
    static Id idCounter = 0;
    return idCounter++;
}

Server::Client::Client(ENetPeer* peer)
    : guid(getNextId())
    , peer(peer)
{
}

size_t Server::getClientIndex(Server::Client::Id guid) const
{
    for (size_t i = 0; i < clients_.size(); ++i)
        if (clients_[i].guid == guid)
            return i;
    std::abort();
}

Server::Client::Id Server::getGuid(const void* peerData) const
{
    return static_cast<Client::Id>(reinterpret_cast<uintptr_t>(peerData));
}

void Server::connectPeer(ENetPeer* peer)
{
    auto client = clients_.emplace_back(peer);
    peer->data = reinterpret_cast<void*>(client.guid);
    fmt::print(
        "Client connected from {}: guid = {}", enet::getIp(peer->address).value(), client.guid);
    client.send(Message<MessageType::Hello> { client.guid }, Channel::Reliable);
}

void Server::disconnectClient(Client::Id guid)
{
    const auto idx = getClientIndex(guid);
    clients_.erase(clients_.begin() + idx);
}

#define MESSAGE_CASE(Type)                                                                         \
    case MessageType::Type:                                                                        \
        processMessage<MessageType::Type>(client, packet);                                         \
        break;

void Server::receive(Client::Id guid, const enet::Packet& packet)
{
    auto& client = clients_[getClientIndex(guid)];
    if (packet.getSize() == 0)
        return; // ignore
    const auto msgType = static_cast<MessageType>(packet.getData<uint8_t>()[0]);
    switch (msgType) {
        MESSAGE_CASE(Hello);
        MESSAGE_CASE(ClientMoveUpdate);
    default:
        fmt::print(stderr, "Received unrecognized message.");
    }
}

void Server::processMessage(Client& /*client*/, const Message<MessageType::Hello>& /*msg*/)
{
}

void Server::processMessage(
    Client& /*client*/, const Message<MessageType::ClientMoveUpdate>& /*msg*/)
{
}
