#include "client.hpp"

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
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

#ifndef NDEBUG
    // TODO: Somehow filter some output out, because it's too much to learn anything right now
    // glwx::debug::init();
#endif

    if (!loadMap("media/ship.glb", world_)) {
        return false;
    }

    player_ = world_.createEntity();
    player_.add<comp::Transform>();
    player_.add<comp::Velocity>();
    player_.add<comp::CircleCollider>(comp::CircleCollider { 0.6f });
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

    serverPeer_ = host_.connect(*addr, 2, 69);
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

    running_ = true;
    time_ = glwx::getTime();
    while (running_) {
        const auto now = glwx::getTime();
        const auto dt = now - time_;
        time_ = now;

        processSdlEvents();
        processEnetEvents();

        update(dt);
        draw();
    }
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
        if (const auto receive = std::get_if<enet::ReceiveEvent>(&event.value())) {
            std::cout << "Received: " << receive->packet.getView() << std::endl;
        } else if (const auto disconnect = std::get_if<enet::DisconnectEvent>(&event.value())) {
            std::cout << "Disconnected." << std::endl;
        }
    }
}

void Client::update(float dt)
{
    InputManager::instance().update();
    playerControlSystem(world_, dt);
    integrationSystem(world_, dt);
}

void Client::draw()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto cameraTransform = player_.get<comp::Transform>();
    cameraTransform.move(glm::vec3(0.0f, 3.5f, 0.0f));
    renderSystem(world_, projection_, cameraTransform);

    window_.swap();
}
