#include "physics.hpp"

#include "components.hpp"

namespace {

std::optional<CollisionResult> intersect(const comp::Transform& circleTrafo,
    const comp::CircleCollider& circle, const comp::Transform& rectTrafo,
    const comp::RectangleCollider& rect)
{
    // circle position clamped into rect
    const auto circlePos = glm::vec2(circleTrafo.getPosition().x, circleTrafo.getPosition().z);
    const auto rectPos = glm::vec2(rectTrafo.getPosition().x, rectTrafo.getPosition().z);
    const auto clamped = glm::vec2(
        std::clamp(circlePos.x, rectPos.x - rect.halfExtents.x, rectPos.x + rect.halfExtents.x),
        std::clamp(circlePos.y, rectPos.y - rect.halfExtents.y, rectPos.y + rect.halfExtents.y));
    const auto rel = circlePos - clamped;
    const auto len = glm::length(rel);
    if (len >= circle.radius)
        return std::nullopt;

    const auto normal = rel / len;
    return CollisionResult { glm::vec3(normal.x, 0.0f, normal.y), circle.radius - len };
}

std::optional<CollisionResult> intersect(const comp::Transform& circleTrafo1,
    const comp::CircleCollider& circle1, const comp::Transform& circleTrafo2,
    const comp::CircleCollider& circle2)
{
    const auto circlePos1 = glm::vec2(circleTrafo1.getPosition().x, circleTrafo1.getPosition().z);
    const auto circlePos2 = glm::vec2(circleTrafo2.getPosition().x, circleTrafo2.getPosition().z);
    const auto radiusSum = circle1.radius + circle2.radius;
    const auto rel = circlePos1 - circlePos2;
    const auto dist = glm::length(rel);
    if (dist > radiusSum)
        return std::nullopt;

    // they are probably perfectly inside each other.
    // We return a collision result, so the server knows not to spawn another player there, but we
    // return a zero collision normal, so we don't resolve it.
    if (dist <= 0.0f)
        return CollisionResult { glm::vec3(0.0f), 0.0f };

    const auto normal = rel / dist;
    return CollisionResult { glm::vec3(normal.x, 0.0f, normal.y), radiusSum - dist };
}

void integrateCircleColliders(ecs::World& world, ecs::EntityHandle entity, comp::Velocity& velocity,
    comp::Transform& transform, const comp::CircleCollider& collider, float dt)
{
    transform.move(velocity.value * dt);
    static constexpr size_t maxCollisionCount = 10;
    size_t collisionCount = 0;
    while (collisionCount < maxCollisionCount) {
        const auto collision = findFirstCollision(world, entity, transform, collider);
        if (!collision)
            return;
        transform.move(collision->normal * collision->penetrationDepth);
        // project velocity on tangent (remove normal component)
        const auto tangent = glm::vec3(-collision->normal.z, 0.0f, collision->normal.x);
        velocity.value = glm::dot(velocity.value, tangent) * tangent;
        collisionCount++;
    }
}
}

std::optional<CollisionResult> findFirstCollision(ecs::World& world, ecs::EntityHandle entity,
    comp::Transform& transform, const comp::CircleCollider& collider)
{
    std::optional<CollisionResult> maxDepthResult;
    // We use the maximum penetration depth as a heuristic to find the first collision (in time),
    // because (intuitively) the longer ago a collision was in the past, the further an object can
    // have penetrated another.
    auto l = [&](ecs::EntityHandle other, const comp::Transform& otherTransform,
                 const auto& otherCollider) {
        if (entity == other)
            return;
        const auto col = intersect(transform, collider, otherTransform, otherCollider);
        if (!col)
            return;
        if (!maxDepthResult || col->penetrationDepth > maxDepthResult->penetrationDepth) {
            maxDepthResult = col;
        }
    };
    world.forEachEntity<const comp::Transform, const comp::CircleCollider>(l);
    world.forEachEntity<const comp::Transform, const comp::RectangleCollider>(l);
    return maxDepthResult;
}

void integrationSystem(ecs::World& world, float dt)
{
    // velocity will prevent this from being executed for non-local players
    world.forEachEntity<comp::Velocity, comp::Transform, const comp::CircleCollider>(
        [&world, dt](ecs::EntityHandle entity, comp::Velocity& velocity, comp::Transform& transform,
            const comp::CircleCollider& collider) {
            integrateCircleColliders(world, entity, velocity, transform, collider, dt);
        });
}

void playerControlSystem(ecs::World& world, float dt)
{
    static constexpr auto maxSpeed = 5.0f;
    static constexpr auto fastBoostFactor = 2;
    static constexpr auto accell = maxSpeed * 5.0f;
    static constexpr auto friction = maxSpeed * 6.0f;
    // static constexpr auto turnAroundFactor = 2.0f;

    world.forEachEntity<comp::Transform, comp::Velocity, comp::PlayerInputController>(
        [dt](comp::Transform& transform, comp::Velocity& velocity,
            const comp::PlayerInputController& ctrl) {
            if (ctrl.lookToggle->getState()) {
                const auto sensitivity = 0.0025f;
                const auto look
                    = glm::vec2(ctrl.lookX->getDelta(), ctrl.lookY->getDelta()) * sensitivity;
                transform.rotate(glm::angleAxis(-look.x, glm::vec3(0.0f, 1.0f, 0.0f)));
                transform.rotateLocal(glm::angleAxis(-look.y, glm::vec3(1.0f, 0.0f, 0.0f)));
            }

            const auto forward = ctrl.forwards->getState() - ctrl.backwards->getState();
            const auto sideways = ctrl.right->getState() - ctrl.left->getState();
            const auto move = glm::vec3(sideways, 0.0f, -forward); // forward is -z

            auto currentMaxSpeed = maxSpeed;
            if (ctrl.fast->getState()) {
                currentMaxSpeed *= fastBoostFactor;
            }

            if (glm::length(move) > 0.0f) {
                auto moveWorld = transform.getOrientation() * move;
                moveWorld.y = 0.0f;
                velocity.value.y = 0.0f;
                const auto factor = 1.0f;
                //= rescale(-glm::dot(glm::normalize(velocity.value), glm::normalize(moveWorld)),
                //  -1.0f, 1.0f, 1.0f, turnAroundFactor);
                velocity.value += moveWorld * factor * accell * dt;

                const auto speed = glm::length(velocity.value);
                if (speed > currentMaxSpeed) {
                    velocity.value *= currentMaxSpeed / speed;
                }
            } else {
                const auto speed = glm::length(velocity.value) + 1e-5f;
                const auto dir = velocity.value / speed;
                velocity.value -= dir * std::min(speed, friction * dt);
            }
        });
}
