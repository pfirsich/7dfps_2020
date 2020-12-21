#pragma once

#include <memory>

#include <glm/glm.hpp>

#include <glwx.hpp>

#include "ecs.hpp"
#include "input.hpp"

namespace comp {
using Transform = glwx::Transform;

struct Velocity {
    glm::vec3 value { 0.0f, 0.0f, 0.0f };
};

struct CylinderCollider {
    float radius;
    float height;
};

struct BoxCollider {
    glm::vec3 halfExtents;
};

struct VisualLink {
    ecs::EntityHandle entity;
};

struct Ladder {
    enum class Dir { Up, Down };
    Dir dir;
};

struct PlayerInputController {
    template <typename T>
    PlayerInputController(SDL_Scancode forwards, SDL_Scancode backwards, SDL_Scancode left,
        SDL_Scancode right, SDL_Scancode up, SDL_Scancode down, SDL_Scancode sprint, T&& interact)
        : forwards(std::make_unique<KeyboardInput>(forwards))
        , backwards(std::make_unique<KeyboardInput>(backwards))
        , left(std::make_unique<KeyboardInput>(left))
        , right(std::make_unique<KeyboardInput>(right))
        , up(std::make_unique<KeyboardInput>(up))
        , down(std::make_unique<KeyboardInput>(down))
        , sprint(std::make_unique<KeyboardInput>(sprint))
        , lookX(std::make_unique<MouseMoveInput>(MouseMoveInput::Axis::X))
        , lookY(std::make_unique<MouseMoveInput>(MouseMoveInput::Axis::Y))
        , interact(std::make_unique<T>(interact))
    {
    }

    // What are these doing here? This turd needs to be done soon, so now I shit code into whatever
    // place I feel like.
    float pitch = 0.0f;
    float yaw = 0.0f;

    std::unique_ptr<BinaryInput> forwards;
    std::unique_ptr<BinaryInput> backwards;
    std::unique_ptr<BinaryInput> left;
    std::unique_ptr<BinaryInput> right;
    std::unique_ptr<BinaryInput> up;
    std::unique_ptr<BinaryInput> down;
    std::unique_ptr<BinaryInput> sprint;

    std::unique_ptr<AnalogInput> lookX;
    std::unique_ptr<AnalogInput> lookY;

    std::unique_ptr<BinaryInput> interact;

    void updateFromOrientation(const comp::Transform& trafo);
};
}

struct CollisionResult {
    glm::vec3 normal;
    float penetrationDepth;
};

std::optional<CollisionResult> findFirstCollision(ecs::World& world, ecs::EntityHandle entity,
    comp::Transform& transform, const comp::CylinderCollider& collider);

struct RayCastHit {
    ecs::EntityHandle entity;
    float t;
};

std::optional<RayCastHit> castRay(
    ecs::World& world, const glm::vec3& rayOrigin, const glm::vec3& rayDir);

void integrationSystem(ecs::World& world, float dt);

void playerLookSystem(ecs::World& world, float dt);
void playerControlSystem(ecs::World& world, float dt);
