#include "client.hpp"

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl.h>
#include <misc/cpp/imgui_stdlib.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "constants.hpp"
#include "gltfimport.hpp"
#include "graphics.hpp"
#include "physics.hpp"
#include "shipsystem.hpp"
#include "sound.hpp"
#include "util.hpp"

namespace {
static bool debugCollisionGeometry = false;
static bool debugRaycast = false;
}

struct Config {
    glwx::Window::Properties props;
    size_t width = 1024;
    size_t height = 768;
    bool maximize = true;
    bool vsync = false;

    void loadFromLua(const char* path)
    {
        sol::state lua;
        const sol::table data = lua.script_file(path);
        if (data["width"] != nullptr)
            width = data["width"];
        if (data["height"] != nullptr)
            height = data["height"];
        if (data["maximize"] != nullptr)
            maximize = data["maximize"];
        if (data["vsync"] != nullptr)
            vsync = data["vsync"];
        if (data["msaa"] != nullptr)
            props.msaaSamples = data["msaa"];
        if (data["fullscreen"] != nullptr)
            props.fullscreenDesktop = data["fullscreen"];
    }
};

bool Client::run(const std::string& host, Port port)
{
    assert(!started_);
    started_ = true;

    Config config;
    config.props.msaaSamples = 4;
    config.props.stencil = true;
    config.props.allowHighDpi = false;
    if (fs::exists("config.lua"))
        config.loadFromLua("config.lua");
    else if (fs::exists("default.config.lua"))
        config.loadFromLua("default.config.lua");
    window_ = glwx::makeWindow("7DFPS", config.width, config.height, config.props).value();
    if (config.maximize)
        window_.maximize();
    window_.setSwapInterval(config.vsync ? 1 : 0);
    resized(window_.getSize().x, window_.getSize().y);
    glw::State::instance().setDepthFunc(glw::DepthFunc::Lequal); // needed for skybox
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(window_.getSdlWindow(), window_.getSdlGlContext());
    ImGui_ImplOpenGL3_Init("#version 150");

    if (!initSound()) {
        return false;
    }

    InputManager::instance().update(); // Initialize data for first frame

#ifndef NDEBUG
    // TODO: Somehow filter some output out, because it's too much to learn anything right now
    // glwx::debug::init();
#endif

    skybox_ = std::make_unique<Skybox>();
    if (!skybox_->load("media/skybox/1.png", "media/skybox/3.png", "media/skybox/5.png",
            "media/skybox/6.png", "media/skybox/2.png", "media/skybox/4.png")) {
        fmt::print("Could not load 'media/skybox'\n");
        return false;
    };

    auto shipGltf = GltfFile::load("media/ship.glb");
    if (!shipGltf) {
        fmt::print("Could not load 'media/ship.glb'\n");
        return false;
    }
    shipGltf->instantiate(world_);

    auto playerGltf = GltfFile::load("media/player.glb");
    if (!playerGltf) {
        fmt::print("Could not load 'media/player.glb'\n");
        return false;
    }

    static constexpr std::array playerMeshNames = { "SK_Character_Alien_Male_01.001",
        "SK_Character_Hacker_Female_01.001", "SK_Character_Muscle_Male_01.001", "robot_mesh.001" };
    for (const auto& name : playerMeshNames) {
        auto mesh = playerGltf->getMesh(name);
        if (!mesh) {
            fmt::print("Mesh '{}' not found in player.glb\n", name);
            return false;
        }
        playerMeshes_.push_back(mesh);
    }

    auto hitMarkerGltf = GltfFile::load("media/marker.glb");
    if (!hitMarkerGltf) {
        fmt::print("Could not load 'media/marker.glb\n");
        return false;
    }
    hitMarker_ = world_.createEntity();
    hitMarker_.add<comp::Transform>();
    hitMarker_.add<comp::Mesh>(hitMarkerGltf->getMesh("Sphere"));

    player_ = world_.createEntity();
    player_.add<comp::Transform>();
    player_.add<comp::Velocity>();
    player_.add<comp::CylinderCollider>(comp::CylinderCollider { playerRadius, cameraOffsetY });
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
    fmt::print("Connecting to {}:{}..\n", enet::getIp(*addr), addr->port);

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
    size_t fps = 0;
    float nextFps = glwx::getTime();
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

        fps++;
        if (nextFps < now) {
            const auto stats = glw::State::instance().getStatistics();
            const auto title = fmt::format(
                "7DFPS - FPS: {}, draw calls: {}, shader binds: {}, texture binds: {}", fps,
                getRenderStats().drawCalls, stats.shaderBinds, stats.textureBinds);
            window_.setTitle(title);
            nextFps = now + 1.0f;
            fps = 0;
        }
    }

    enet_peer_disconnect_now(serverPeer_, 0);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    deinitSound();

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
        ImGui_ImplSDL2_ProcessEvent(&event);
        switch (event.type) {
        case SDL_QUIT:
            running_ = false;
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_RETURN:
                if (auto terminalState = std::get_if<TerminalState>(&state_)) {
                    send(Channel::Reliable,
                        Message<MessageType::ClientExecuteCommand> {
                            terminalState->terminalInput });
                    terminalState->terminalInput = "";
                }
                break;
            case SDL_SCANCODE_ESCAPE:
                if (auto terminalState = std::get_if<TerminalState>(&state_)) {
                    state_ = MoveState {};
                    send(Channel::Reliable, Message<MessageType::ClientInteractTerminal> { "" });
                }
                break;
            case SDL_SCANCODE_S:
                if (event.key.keysym.mod & KMOD_CTRL) {
                    const auto mode = SDL_GetRelativeMouseMode() == SDL_TRUE ? SDL_FALSE : SDL_TRUE;
                    SDL_SetRelativeMouseMode(mode);
                }
                break;
#ifndef NDEBUG
            case SDL_SCANCODE_C:
                if (event.key.keysym.mod & KMOD_CTRL)
                    debugCollisionGeometry = !debugCollisionGeometry;
                break;
            case SDL_SCANCODE_R:
                if (event.key.keysym.mod & KMOD_CTRL)
                    debugRaycast = !debugRaycast;
                break;
#endif
            default:
                break;
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

void Client::handleInteractions()
{
    world_.forEachEntity<const comp::RenderHighlight>(
        [](ecs::EntityHandle entity, const comp::RenderHighlight&) {
            entity.remove<comp::RenderHighlight>();
        });

    auto& trafo = player_.get<comp::Transform>();
    const auto rayOrigin = trafo.getPosition() + glm::vec3(0.0f, cameraOffsetY, 0.0f);
    const auto rayDir = trafo.getForward();
    auto hit = castRay(world_, rayOrigin, rayDir);
    const auto interact = player_.get<comp::PlayerInputController>().interact->getPressed();
    if (hit && hit->t <= interactDistance) {
        static ecs::EntityHandle lastHit;
        if (hit->entity != lastHit) {
            // fmt::print("Hit entity '{}' at t = {}\n", comp::Name::get(hit->entity), hit->t);
            lastHit = hit->entity;
        }
        if (debugRaycast) {
            hitMarker_.get<comp::Transform>().setPosition(rayOrigin + rayDir * hit->t);
        };
        if (auto linked = hit->entity.getPtr<comp::VisualLink>()) {
            linked->entity.add<comp::RenderHighlight>();
            if (interact) {
                if (const auto ladder = hit->entity.getPtr<comp::Ladder>()) {
                    // Sometimes we use a ladder, when we are too far away from it and end up
                    // teleporting into a wall.
                    // So first move closer to the ladder (along the ray, but don't change height).
                    const auto& collider = player_.get<comp::CylinderCollider>();
                    const auto dir = glm::vec3(rayDir.x, 0.0f, rayDir.z);
                    while (!findFirstCollision(world_, player_, trafo, collider)) {
                        trafo.move(dir * 0.05f);
                    }

                    const auto delta = ladder->dir == comp::Ladder::Dir::Up ? 1.0f : -1.0f;
                    trafo.setPosition(
                        trafo.getPosition() + glm::vec3(0.0f, delta * floorHeight, 0.0f));
                }
            }
        }
        if (auto terminal = hit->entity.getPtr<comp::Terminal>()) {
            if (interact) {
                send(Channel::Reliable,
                    Message<MessageType::ClientInteractTerminal> { terminal->systemName });
                hit->entity.get<comp::VisualLink>().entity.remove<comp::RenderHighlight>();
            }
        }
    } else {
        hitMarker_.get<comp::Transform>().setPosition(glm::vec3(0.0f, -1000.0f, 0.0f));
    }
}

void Client::playSound(const std::string& name, const std::string entityName)
{
    auto entity = findEntity(world_, entityName);
    if (entity) {
        play3dSound(name, entity.get<comp::Transform>().getPosition());
    }
}

void Client::update(float dt)
{
    InputManager::instance().update();
    if (const auto move = std::get_if<MoveState>(&state_)) {
        playerLookSystem(world_, dt);
        playerControlSystem(world_, dt);
        integrationSystem(world_, dt);
        handleInteractions();

        updateListener(player_.get<comp::Transform>(), player_.get<comp::Velocity>().value);
    } else if (const auto terminal = std::get_if<TerminalState>(&state_)) {
        const auto& termTrafo = terminal->terminalEntity.get<comp::Transform>();
        const auto targetDist = 3.0f;
        const auto targetPos = termTrafo.getPosition() + termTrafo.getUp() * targetDist;
        const auto delta = targetPos - player_.get<comp::Transform>().getPosition();
        const auto dist = glm::length(delta) + 1e-5f;
        if (dist > 0.01f) {
            const auto dir = delta / dist;
            auto& trafo = player_.get<comp::Transform>();
            trafo.move(dir * std::min(dist, 5.0f * dt));
            const auto startDist = glm::length(targetPos - terminal->startPos);
            const auto t = rescale(dist, startDist, 0.0f, 0.0f, 1.0f);
            const auto targetOrientation
                = glm::angleAxis(glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f))
                * termTrafo.getOrientation();
            trafo.setOrientation(glm::slerp(trafo.getOrientation(), targetOrientation, t));
        } else {
            // playerLookSystem(world_, dt); // maybe put this in later
        }

        if (player_.get<comp::PlayerInputController>().interact->getPressed()) {
            state_ = MoveState {};
            send(Channel::Reliable, Message<MessageType::ClientInteractTerminal> { "" });
        }

        updateListener(player_.get<comp::Transform>(), glm::vec3(0.0f));
    }

    world_.forEachEntity<const comp::Terminal, const comp::Transform>(
        [this](const comp::Terminal&, const comp::Transform& transform) {
            if (rand<float>() < 0.01f)
                play3dSound("terminalIdleBeep", transform.getPosition());
        });

    world_.forEachEntity<comp::Transform, const comp::Rotate>(
        [dt](comp::Transform& transform, const comp::Rotate& rotate) {
            transform.rotate(
                glm::angleAxis(2.0f * glm::pi<float>() * rotate.frequency * dt, rotate.axis));
        });
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
    const auto messageType = static_cast<MessageType>(header.messageType);
    switch (messageType) {
        MESSAGE_CASE(ServerHello);
        MESSAGE_CASE(ServerPlayerStateUpdate);
        MESSAGE_CASE(ServerInteractTerminal);
        MESSAGE_CASE(ServerUpdateTerminalOutput);
    default:
        fmt::print(stderr, "Received unrecognized message: {}\n", asString(messageType));
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
    player.add<comp::Transform>().setScale(glm::vec3(2.1f));
    player.add<comp::CylinderCollider>(comp::CylinderCollider { playerRadius, cameraOffsetY });
    player.add<comp::Mesh>(playerMeshes_[id % playerMeshes_.size()]);
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

ecs::EntityHandle Client::findTerminal(const std::string& system)
{
    ecs::EntityHandle found;
    world_.forEachEntity<comp::Terminal>(
        [&system, &found](ecs::EntityHandle entity, const comp::Terminal& terminal) {
            if (!found && terminal.systemName == system) {
                found = entity.get<comp::VisualLink>().entity;
            }
        });
    assert(found);
    return found;
}

void Client::processMessage(
    uint32_t /*frameNumber*/, const Message<MessageType::ServerInteractTerminal>& message)
{
    state_ = TerminalState { findTerminal(message.terminal), message.terminal, "",
        player_.get<comp::Transform>().getPosition() };
    send(Channel::Reliable, Message<MessageType::ClientUpdateTerminalInput> { "" });
}

void Client::processMessage(
    uint32_t /*frameNumber*/, const Message<MessageType::ServerUpdateTerminalOutput>& message)
{
    terminalData_[message.terminal].output.append(message.text);
}

void Client::draw()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto cameraTransform = player_.get<comp::Transform>();
    cameraTransform.move(glm::vec3(0.0f, cameraOffsetY, 0.0f));
    glw::State::instance().resetStatistics();
    resetRenderStats();
    if (debugCollisionGeometry) {
        collisionRenderSystem(world_, projection_, cameraTransform);
    } else {
        renderSystem(world_, projection_, cameraTransform);
        skybox_->draw(projection_, cameraTransform);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window_.getSdlWindow());
    ImGui::NewFrame();

    // ImGui::ShowDemoWindow();

    if (std::holds_alternative<TerminalState>(state_)) {
        auto& terminalState = std::get<TerminalState>(state_);

        constexpr auto margin = 200.0f;
        const auto size = window_.getSize();
        ImGui::SetNextWindowPos(ImVec2(margin, margin));
        ImGui::SetNextWindowSize(ImVec2(size.x - margin * 2.0f, size.y - margin * 2.0f));
        ImGui::Begin("Terminal", nullptr, ImGuiWindowFlags_NoDecoration);
        ImGui::BeginChild(
            "Child", ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetWindowHeight() - 35));
        ImGui::TextUnformatted(terminalData_[terminalState.systemName].output.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        ImGui::PushItemWidth(-1);
        bool focus = false;
        if (ImGui::InputText(
                "Execute", &terminalState.terminalInput, ImGuiInputTextFlags_EnterReturnsTrue)) {
            terminalState.terminalInput = "";
        }
        ImGui::SetKeyboardFocusHere(-1);
        ImGui::PopItemWidth();
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    window_.swap();
}
