#pragma once

#include <string>
#include <vector>

#include <glwx.hpp>

#include "ecs.hpp"
#include "graphics.hpp"
#include "net.hpp"

class Client {
public:
    Client() = default;

    // this blocks until the window is closed or the player ends the game
    bool run(const std::string& host, Port port);

private:
    void resized(size_t width, size_t height);

    void processSdlEvents();
    void processEnetEvents();
    void update(float dt);
    void draw();

    ENetPeer* serverPeer_ = nullptr;
    enet::Host host_;
    glwx::Window window_;
    ecs::World world_;
    std::unique_ptr<Skybox> skybox_;
    ecs::EntityHandle player_;
    glm::mat4 projection_;
    float time_;
    bool started_ = false;
    bool running_ = false;
};
