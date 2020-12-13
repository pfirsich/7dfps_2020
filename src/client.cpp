#include "client.hpp"

#include <regex>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include "constants.hpp"
#include "gltfimport.hpp"
#include "graphics.hpp"
#include "imgui.hpp"
#include "physics.hpp"
#include "shipsystem.hpp"
#include "sound.hpp"
#include "util.hpp"

namespace {
static bool debugCollisionGeometry = false;
static bool debugRaycast = false;
static bool debugFrustumCulling = false;
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

uint32_t Client::showConnectCodeMenu(HostPort& hostPort)
{
    static std::regex connectCodeRegex(
        "^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\:\\d{1,5}\\:[0-9A-F]{6}$");
    SDL_SetRelativeMouseMode(SDL_FALSE);
    bool menuRunning = true;
    uint32_t gameCode;
    while (menuRunning) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            switch (event.type) {
            case SDL_QUIT:
                menuRunning = false;
                break;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        drawImgui(window_.getSdlWindow(), [this, &hostPort, &menuRunning, &gameCode]() {
            static std::string connectCode;

            const auto size = window_.getSize();
            const auto paneSize = glm::vec2(500.0f, 165.0f);
            ImGui::SetNextWindowPos(
                ImVec2(size.x / 2 - paneSize.x / 2, size.y / 2 - paneSize.y / 2));
            ImGui::SetNextWindowSize(ImVec2(paneSize.x, paneSize.y));
            ImGui::Begin("Menu", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                    | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

            if (connectCode.empty()) {
                ImGui::TextUnformatted("Paste connect code:");
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.0, 0.0f, 1.0f), "Invalid connect code");
            }
            ImGui::PushItemWidth(-1);
            ImGui::InputText("", &connectCode, ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopItemWidth();

            std::smatch match;
            if (std::regex_match(connectCode, match, connectCodeRegex)) {
                const auto firstColon = connectCode.find(':');
                const auto secondColon = connectCode.find(':', firstColon + 1);
                assert(firstColon != std::string::npos && secondColon != std::string::npos);
                const auto host = connectCode.substr(0, firstColon);
                const auto port = parseInt<Port>(
                    connectCode.substr(firstColon + 1, secondColon - firstColon - 1));
                const auto gameCode_ = parseInt<uint32_t>(connectCode.substr(secondColon + 1), 16);
                if (port && gameCode) {
                    hostPort = HostPort { host, *port };
                    gameCode = *gameCode_;
                    menuRunning = false;
                }
            }

            ImGui::Dummy(ImVec2(0.0f, 25.0f));
            const auto textIndent = paneSize.x / 2 - 25.0f;
            ImGui::Indent(textIndent);
            ImGui::TextUnformatted("- or -");
            ImGui::Unindent(textIndent);
            ImGui::Dummy(ImVec2(0.0f, 25.0f));

            const auto buttonWidth = 150.0f;
            ImGui::Indent(paneSize.x / 2 - buttonWidth / 2);
            const char* host = "http://207.154.224.60/";
            if (ImGui::Button("Create Game", ImVec2(buttonWidth, 0.0f))) {
#ifdef _WIN32
                ShellExecute(nullptr, "open", host, nullptr, nullptr, SW_SHOWNORMAL);
#elif __linux__
                std::system(fmt::format("xdg-open {}", host).c_str());
#elif __APPLE__
                std::system(fmt::format("open {}", host).c_str());
#endif
            }

            ImGui::End();
        });

        window_.swap();
    }
    SDL_SetRelativeMouseMode(SDL_TRUE);
    return gameCode;
}

void Client::showError(const std::string& message)
{
    SDL_SetRelativeMouseMode(SDL_FALSE);
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        drawImgui(window_.getSdlWindow(), [this, &message]() {
            static std::string connectCode;

            const auto size = window_.getSize();
            const auto paneSize = glm::vec2(600.0f, 50.0f);
            ImGui::SetNextWindowPos(
                ImVec2(size.x / 2 - paneSize.x / 2, size.y / 2 - paneSize.y / 2));
            ImGui::SetNextWindowSize(ImVec2(paneSize.x, paneSize.y));
            ImGui::Begin("Menu", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                    | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);

            ImGui::TextUnformatted(message.c_str());

            ImGui::End();
        });

        window_.swap();
    }
}

bool Client::run(std::optional<HostPort> hostPort, uint32_t gameCode)
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
    glw::State::instance().setDepthFunc(glw::DepthFunc::Lequal); // needed for skybox
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    initImgui(window_.getSdlWindow(), window_.getSdlGlContext());

    if (!initSound()) {
        return false;
    }

    InputManager::instance().update(); // Initialize data for first frame

#ifndef NDEBUG
    // TODO: Somehow filter some output out, because it's too much to learn anything right now
    // glwx::debug::init();
#endif

    drawImgui(window_.getSdlWindow(), [this]() {
        ImGui::Begin("Loading..");
        ImGui::TextUnformatted("Loading..");
        ImGui::End();
    });
    window_.swap();

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

    // We load everything else before attempting to connect, because we don't want a connection
    // to time out or something.

    if (!hostPort) {
        hostPort = HostPort { "", 0 };
        gameCode = showConnectCodeMenu(*hostPort);
    }
    resized(window_.getSize().x, window_.getSize().y);

    host_ = enet::Host(static_cast<uint8_t>(Channel::Count), 0, 0);
    if (!host_) {
        showError("Could not create client host.");
        return false;
    }

    const auto addr = enet::getAddress(hostPort->host, hostPort->port);
    if (!addr) {
        showError("Could not resolve address.");
        return false;
    }

    serverPeer_ = host_.connect(*addr, 2, getConnectCode(gameCode));
    if (!serverPeer_) {
        showError("Could not connect.");
        return false;
    }
    fmt::print("Connecting to {}:{}..\n", enet::getIp(*addr), addr->port);

    // TODO: MAKE THIS NOT BLOCK
    const auto event = host_.service(5000);
    if (event && std::holds_alternative<enet::ConnectEvent>(*event)) {
        fmt::print("Connected.\n");
    } else {
        showError("Connection failed.");
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
        showError("Handshake failed (wrong gamecode or mismatching versions).");
        enet_peer_disconnect_now(serverPeer_, 0);
        return false;
    }
    fmt::print("Player id: {}\n", playerId_);

    soloud.setLooping(playEntitySound("engineIdle", "engine"), true);
    soloud.setLooping(playEntitySound("engineIdle", "engine", 1.0f, 2.0f), true);

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

    deinitImgui();

    deinitSound();

    return true;
}

void Client::resized(size_t width, size_t height)
{
    glw::State::instance().setViewport(width, height);
    const auto aspect = static_cast<float>(width) / height;
    frustum_.setPerspective(glm::radians(45.0f), aspect, 0.1f, 300.0f);
}

void Client::stopTerminalInteraction()
{
    const auto& terminalState = std::get<TerminalState>(state_);
    playEntitySound("terminalInteractEnd", terminalState.terminalEntity);
    state_ = MoveState {};
    send(Channel::Reliable, Message<MessageType::ClientInteractTerminal> { "" });
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
        case SDL_TEXTINPUT:
            if (std::holds_alternative<TerminalState>(state_)) {
                playEntitySound("terminalType", std::get<TerminalState>(state_).terminalEntity);
            }
        case SDL_KEYDOWN:
            switch (event.key.keysym.scancode) {
            case SDL_SCANCODE_RETURN:
                if (auto terminalState = std::get_if<TerminalState>(&state_)) {
                    send(Channel::Reliable,
                        Message<MessageType::ClientExecuteCommand> {
                            terminalState->terminalInput });
                    terminalState->terminalInput = "";
                    playEntitySound("terminalExecute", terminalState->terminalEntity);
                }
                break;
            case SDL_SCANCODE_ESCAPE:
                if (std::holds_alternative<TerminalState>(state_)) {
                    stopTerminalInteraction();
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
            case SDL_SCANCODE_F:
                if (event.key.keysym.mod & KMOD_CTRL)
                    debugFrustumCulling = !debugFrustumCulling;
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
                    // So first move closer to the ladder (along the ray, but don't change
                    // height).
                    const auto& collider = player_.get<comp::CylinderCollider>();
                    const auto dir = glm::vec3(rayDir.x, 0.0f, rayDir.z);
                    while (!findFirstCollision(world_, player_, trafo, collider)) {
                        trafo.move(dir * 0.05f);
                    }

                    const auto delta = ladder->dir == comp::Ladder::Dir::Up ? 1.0f : -1.0f;
                    trafo.setPosition(
                        trafo.getPosition() + glm::vec3(0.0f, delta * floorHeight, 0.0f));

                    playSound("ladderInteract");
                }
            }
        }
        if (auto terminal = hit->entity.getPtr<comp::Terminal>()) {
            if (interact) {
                send(Channel::Reliable,
                    Message<MessageType::ClientInteractTerminal> { terminal->systemName });
                hit->entity.get<comp::VisualLink>().entity.remove<comp::RenderHighlight>();
                playEntitySound("terminalInteract", hit->entity);
            }
        }
    } else {
        hitMarker_.get<comp::Transform>().setPosition(glm::vec3(0.0f, -1000.0f, 0.0f));
    }
}

SoLoud::handle Client::playEntitySound(
    const std::string& name, ecs::EntityHandle entity, float volume, float playbackSpeed)
{
    return play3dSound(name, entity.get<comp::Transform>().getPosition(), volume, playbackSpeed);
}

SoLoud::handle Client::playEntitySound(
    const std::string& name, const std::string entityName, float volume, float playbackSpeed)
{
    auto entity = findEntity(world_, entityName);
    if (entity) {
        return playEntitySound(name, entity, volume, playbackSpeed);
    }
    return 0;
}

void Client::update(float dt)
{
    InputManager::instance().update();
    if (const auto move = std::get_if<MoveState>(&state_)) {
        playerLookSystem(world_, dt);
        playerControlSystem(world_, dt);
        integrationSystem(world_, dt);
        handleInteractions();

        // Negative velocity, because otherwise the doppler effect will be the wrong way around
        // :)
        updateListener(player_.get<comp::Transform>(), -player_.get<comp::Velocity>().value);
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
            stopTerminalInteraction();
        }

        updateListener(player_.get<comp::Transform>(), glm::vec3(0.0f));
    }

    world_.forEachEntity<const comp::Terminal, const comp::Transform>(
        [this](const comp::Terminal&, const comp::Transform& transform) {
            if (rand<float>() < 0.01f)
                play3dSound("terminalIdleBeep", transform.getPosition());
        });

    world_.forEachEntity<const comp::Transform, const comp::Mesh, const comp::Name>(
        [this](const comp::Transform& transform, const comp::Mesh&, const comp::Name& name) {
            if (name.value.find("reactorcell") == 0 && rand<float>() < 0.01f)
                play3dSound("reactorZap", transform.getPosition());
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
    if (const auto terminalState = std::get_if<TerminalState>(&state_)) {
        if (terminalState->systemName == message.terminal)
            playEntitySound("terminalOutput", terminalState->terminalEntity);
    }
}

void Client::draw()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto cameraTransform = player_.get<comp::Transform>();
    cameraTransform.move(glm::vec3(0.0f, cameraOffsetY, 0.0f));
    glw::State::instance().resetStatistics();
    resetRenderStats();
    if (debugCollisionGeometry) {
        collisionRenderSystem(world_, frustum_, cameraTransform);
    } else if (debugFrustumCulling) {
        cullingRenderSystem(world_, frustum_, cameraTransform);
    } else {
        renderSystem(world_, frustum_, cameraTransform);
        skybox_->draw(frustum_, cameraTransform);
    }

    // Only one drawImgui call per frame!
    // Also make sure it's called, even if we don't draw anything, so input is handled correctly
    // (otherwise widgets that are not drawn keep focus).
    drawImgui(window_.getSdlWindow(), [this]() {
        if (std::holds_alternative<TerminalState>(state_)) {
            // ImGui::ShowDemoWindow();

            auto& terminalState = std::get<TerminalState>(state_);

            constexpr auto margin = 200.0f;
            const auto size = window_.getSize();
            ImGui::SetNextWindowPos(ImVec2(margin, margin));
            ImGui::SetNextWindowSize(ImVec2(size.x - margin * 2.0f, size.y - margin * 2.0f));
            ImGui::Begin("Terminal", nullptr, ImGuiWindowFlags_NoDecoration);
            ImGui::BeginChild("Child",
                ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetWindowHeight() - 40));
            ImGui::TextUnformatted(terminalData_[terminalState.systemName].output.c_str());
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::PushItemWidth(-1);
            bool focus = false;
            if (ImGui::InputText("Execute", &terminalState.terminalInput,
                    ImGuiInputTextFlags_EnterReturnsTrue)) {
                terminalState.terminalInput = "";
            }
            ImGui::SetKeyboardFocusHere(-1);
            ImGui::PopItemWidth();
            ImGui::End();
        }
    });

    window_.swap();
}
