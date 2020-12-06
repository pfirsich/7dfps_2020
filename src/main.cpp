

#include <fmt/format.h>

#include "components.hpp"
#include "ecs.hpp"
#include "gltfimport.hpp"
#include "graphics.hpp"
#include "input.hpp"
#include "physics.hpp"

/* TODO
 * ECS: Make EntityId fully internal and only use EntityHandle outside of ECS implementation
 * ECS: Move all (short) implementations out of the header part
 */

int main(int, char**)
{
    glwx::Window::Properties props;
    props.msaaSamples = 8;
    const auto window = glwx::makeWindow("7DFPS", 1024, 768, props).value();
    glw::State::instance().setViewport(window.getSize().x, window.getSize().y);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

#ifndef NDEBUG
    glwx::debug::init();
#endif

    ecs::World world;
    if (!loadMap("media/ship.glb", world)) {
        return 1;
    }

    auto player = world.createEntity();
    player.add<comp::Transform>();
    player.add<comp::Velocity>();
    player.add<comp::CircleCollider>(comp::CircleCollider { 0.6f });
    player.add<comp::PlayerInputController>(SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A,
        SDL_SCANCODE_D, SDL_SCANCODE_R, SDL_SCANCODE_F, SDL_SCANCODE_LSHIFT, MouseButtonInput(1));

    const auto aspect = static_cast<float>(window.getSize().x) / window.getSize().y;
    const auto projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 200.0f);

    world.flush();

    SDL_Event event;
    bool running = true;
    float time = glwx::getTime();
    while (running) {
        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                }
                break;
            }
        }

        const auto now = glwx::getTime();
        const auto dt = now - time;
        time = now;

        InputManager::instance().update();
        playerControlSystem(world, dt);
        integrationSystem(world, dt);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        auto cameraTransform = player.get<comp::Transform>();
        cameraTransform.move(glm::vec3(0.0f, 3.5f, 0.0f));
        renderSystem(world, projection, cameraTransform);

        window.swap();
    }

    return 0;
}
