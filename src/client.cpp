#include "client.hpp"

#include "constants.hpp"
#include "gltfimport.hpp"
#include "graphics.hpp"
#include "physics.hpp"

bool Client::run(const std::string& host, Port port)
{
    assert(!started_);
    started_ = true;

    glwx::Window::Properties props;
    props.msaaSamples = 8;
    window_ = glwx::makeWindow("7DFPS", 1024, 768, props).value();
    resized(window_.getSize().x, window_.getSize().y);
    glw::State::instance().setDepthFunc(glw::DepthFunc::Lequal); // needed for skybox
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

#ifndef NDEBUG
    // TODO: Somehow filter some output out, because it's too much to learn anything right now
    // glwx::debug::init();
#endif

    skybox_ = std::make_unique<Skybox>();
    if (!skybox_->load("media/skybox/1.png", "media/skybox/2.png", "media/skybox/3.png",
            "media/skybox/4.png", "media/skybox/5.png", "media/skybox/6.png")) {
        fmt::print("Could not load 'media/skybox'\n");
        return false;
    };

    if (!loadMap("media/ship.glb", world_)) {
        fmt::print("Could not load 'media/ship.glb'\n");
        return false;
    }

    playerMesh_ = loadMesh("media/player.glb");
    if (!playerMesh_) {
        fmt::print("Could not load 'media/player.glb'\n");
        return false;
    }

    player_ = world_.createEntity();
    player_.add<comp::Transform>();
    player_.add<comp::Velocity>();
    player_.add<comp::CircleCollider>(comp::CircleCollider { playerRadius });
    player_.add<comp::PlayerInputController>(SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A,
        SDL_SCANCODE_D, SDL_SCANCODE_R, SDL_SCANCODE_F, SDL_SCANCODE_LSHIFT, MouseButtonInput(1));

    world_.flush();

    // We load everything else before attempting to connect, because we don't want a connection to
    // time out or something.

    host_ = enet::Host(static_cast<uint8_t>(Channel::Count), 0, 0);
    if (!host_) {
        fmt::print(stderr, "Could not create client host.\n");
        return false;
    }

    const auto addr = enet::getAddress(host, port);
    if (!addr) {
        fmt::print(stderr, "Could not get address.\n");
        return false;
    }

    serverPeer_ = host_.connect(*addr, 2, protocolVersion);
    if (!serverPeer_) {
        fmt::print(stderr, "Could not connect.\n");
        return false;
    }
    fmt::print("Connecting to {}:{}...\n", enet::getIp(*addr), addr->port);

    // TODO: MAKE THIS NOT BLOCK
    const auto event = host_.service(5000);
    if (event && std::holds_alternative<enet::ConnectEvent>(*event)) {
        fmt::print("Connected.\n");
    } else {
        fmt::print(stderr, "Connection failed.\n");
        enet_peer_reset(serverPeer_);
        return false;
    }

    const auto waitHelloStart = glwx::getTime();
    while (glwx::getTime() - waitHelloStart < 2.0f) {
        processEnetEvents();
        if (playerId_ != InvalidPlayerId)
            break;
    }
    if (playerId_ == InvalidPlayerId) {
        fmt::print(stderr, "Did not receive server hello\n");
        enet_peer_disconnect_now(serverPeer_, 0);
        return false;
    }
    fmt::print("Player id: {}\n", playerId_);

    running_ = true;
    time_ = 0.0f;
    float clockTime = glwx::getTime();
    float accumulator = 0.0f;
    constexpr auto dt = 1.0f / tickRate;
    while (running_) {
        const auto now = glwx::getTime();
        const auto clockDelta = now - clockTime;
        clockTime = now;

        accumulator += clockDelta;
        while (accumulator >= dt) {
            processSdlEvents();
            processEnetEvents();
            update(dt);
            sendUpdate();
            accumulator -= dt;
            time_ += dt;
            frameCounter_++;
        }

        draw();
    }

    enet_peer_disconnect_now(serverPeer_, 0);

    return true;
}

void Client::resized(size_t width, size_t height)
{
    glw::State::instance().setViewport(width, height);
    const auto aspect = static_cast<float>(width) / height;
    projection_ = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 200.0f);
}

void Client::processSdlEvents()
{
    static SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        switch (event.type) {
        case SDL_QUIT:
            running_ = false;
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
#ifndef NDEBUG
            case SDLK_ESCAPE:
                running_ = false;
                break;
#endif
            }
            break;
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                resized(event.window.data1, event.window.data2);
        }
    }
}

void Client::processEnetEvents()
{
    std::optional<enet::Event> event;
    while ((event = host_.service())) {
        if (const auto recvEvent = std::get_if<enet::ReceiveEvent>(&event.value())) {
            receive(recvEvent->packet);
        } else if (const auto disconnect = std::get_if<enet::DisconnectEvent>(&event.value())) {
            fmt::print(stderr, "Disconnected by server\n");
            running_ = false;
        }
    }
}

void Client::update(float dt)
{
    InputManager::instance().update();
    playerControlSystem(world_, dt);
    integrationSystem(world_, dt);
}

void Client::sendUpdate()
{
    const auto& trafo = player_.get<comp::Transform>();
    send(Channel::Unreliable,
        Message<MessageType::ClientMoveUpdate> { trafo.getPosition(), trafo.getOrientation() });
}

#define MESSAGE_CASE(Type)                                                                         \
    case MessageType::Type:                                                                        \
        processMessage<MessageType::Type>(header.frameNumber, buffer);                             \
        break;

void Client::receive(const enet::Packet& packet)
{
    ReadBuffer buffer(packet.getData<uint8_t>(), packet.getSize());
    CommonMessageHeader header;
    if (!deserialize(buffer, header)) {
        fmt::print(stderr, "Could not decode common message header\n");
        return; // Ignore message
    }
    switch (static_cast<MessageType>(header.messageType)) {
        MESSAGE_CASE(ServerHello);
        MESSAGE_CASE(ServerPlayerStateUpdate);
    default:
        fmt::print(stderr, "Received unrecognized message\n");
    }
}

void Client::processMessage(
    uint32_t /*frameNumber*/, const Message<MessageType::ServerHello>& message)
{
    assert(playerId_ == InvalidPlayerId);
    playerId_ = message.playerId;
    player_.get<comp::Transform>().setPosition(message.spawnPos);
}

void Client::addPlayer(PlayerId id)
{
    auto player = world_.createEntity();
    player.add<comp::Hierarchy>();
    player.add<comp::Transform>();
    player.add<comp::CircleCollider>(comp::CircleCollider { playerRadius });
    player.add<comp::Mesh>(playerMesh_);
    players_.emplace(id, player);
}

void Client::processMessage(
    uint32_t frameNumber, const Message<MessageType::ServerPlayerStateUpdate>& message)
{
    static uint32_t lastUpdateFrame = 0;
    if (frameNumber < lastUpdateFrame)
        return;

    for (const auto& player : message.players) {
        if (player.id == playerId_)
            continue;
        auto it = players_.find(player.id);
        if (it == players_.end()) {
            addPlayer(player.id);
            it = players_.find(player.id);
            fmt::print("Player (id = {}) connected\n", player.id);
        }

        auto& trafo = it->second.get<comp::Transform>();
        const auto lookDir = player.orientation * glm::vec3(0.0f, 0.0f, 1.0f);
        trafo.lookAtPos(player.position, player.position + glm::vec3(lookDir.x, 0.0f, lookDir.z));
    }

    std::vector<PlayerId> playersToRemove;
    for (const auto& [id, entity] : players_) {
        bool found = false;
        for (const auto& msgPlayer : message.players) {
            if (msgPlayer.id == id) {
                found = true;
                break;
            }
        }
        if (!found) {
            playersToRemove.push_back(id);
        }
    }

    for (const auto id : playersToRemove) {
        auto& player = players_.at(id);
        player.destroy();
        players_.erase(id);
        fmt::print("Player (id = {}) disconnected\n", id);
    }

    world_.flush();

    lastUpdateFrame = frameNumber;
}

void Client::draw()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto cameraTransform = player_.get<comp::Transform>();
    cameraTransform.move(glm::vec3(0.0f, 3.5f, 0.0f));
    renderSystem(world_, projection_, cameraTransform);
    skybox_->draw(projection_, cameraTransform);

    window_.swap();
}
