#include "physics.hpp"

#include <glm/gtc/quaternion.hpp>

#include "components.hpp"
#include "constants.hpp"

namespace {
bool intervalsOverlap(float minA, float maxA, float minB, float maxB)
{
    return minA <= maxB && minB < maxA;
}

std::optional<CollisionResult> intersect(const glm::vec3& cylPos, const comp::CylinderCollider& cyl,
    const glm::vec3& boxPos, const comp::BoxCollider& box)
{
    const auto boxMin = boxPos - box.halfExtents;
    const auto boxMax = boxPos + box.halfExtents;
    if (!intervalsOverlap(cylPos.y, cylPos.y + cyl.height, boxMin.y, boxMax.y))
        return std::nullopt;

    // circle position clamped into rect
    const auto clamped = glm::clamp(cylPos, boxMin, boxMax);
    auto rel = cylPos - clamped;
    rel.y = 0.0f;
    const auto len = glm::length(rel) + 1e-5f;
    if (len >= cyl.radius)
        return std::nullopt;

    const auto normal = rel / len;
    return CollisionResult { glm::vec3(normal.x, 0.0f, normal.z), cyl.radius - len };
}

std::optional<CollisionResult> intersect(const glm::vec3& circlePos1,
    const comp::CylinderCollider& circle1, const glm::vec3& circlePos2,
    const comp::CylinderCollider& circle2)
{
    if (!intervalsOverlap(circlePos1.y, circlePos1.y + circle1.height, circlePos2.y,
            circlePos2.y + circle2.height))
        return std::nullopt;

    const auto radiusSum = circle1.radius + circle2.radius;
    auto rel = circlePos1 - circlePos2;
    rel.y = 0.0f;
    const auto dist = glm::length(rel);
    if (dist > radiusSum)
        return std::nullopt;

    // they are probably perfectly inside each other.
    // We return a collision result, so the server knows not to spawn another player there, but we
    // return a zero collision normal, so we don't resolve it.
    if (dist <= 0.0f)
        return CollisionResult { glm::vec3(0.0f), 0.0f };

    const auto normal = rel / dist;
    return CollisionResult { glm::vec3(normal.x, 0.0f, normal.z), radiusSum - dist };
}
}

std::optional<CollisionResult> findFirstCollision(ecs::World& world, ecs::EntityHandle entity,
    comp::Transform& transform, const comp::CylinderCollider& collider)
{
    std::optional<CollisionResult> maxDepthResult;
    // We use the maximum penetration depth as a heuristic to find the first collision (in time),
    // because (intuitively) the longer ago a collision was in the past, the further an object can
    // have penetrated another.
    auto l = [&](ecs::EntityHandle other, const comp::Transform& otherTransform,
                 const auto& otherCollider) {
        if (entity == other)
            return;
        const auto col = intersect(
            transform.getPosition(), collider, otherTransform.getPosition(), otherCollider);
        if (!col)
            return;
        if (!maxDepthResult || col->penetrationDepth > maxDepthResult->penetrationDepth) {
            maxDepthResult = col;
        }
    };
    world.forEachEntity<const comp::Transform, const comp::CylinderCollider>(l);
    world.forEachEntity<const comp::Transform, const comp::BoxCollider>(l);
    return maxDepthResult;
}

namespace {
// https://medium.com/@bromanz/another-view-on-the-classic-ray-aabb-intersection-algorithm-for-bvh-traversal-41125138b525
// https://gamedev.stackexchange.com/questions/18436/most-efficient-aabb-vs-ray-collision-algorithms
std::optional<float> intersectRayAabb(
    const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glwx::Aabb& box)
{
    const auto t0 = (box.min - rayOrigin) / rayDir;
    const auto t1 = (box.max - rayOrigin) / rayDir;
    const auto tmin = glm::min(t0, t1);
    const auto tmax = glm::max(t0, t1);
    const auto min = std::max({ tmin.x, tmin.y, tmin.z });
    const auto max = std::min({ tmax.x, tmax.y, tmax.z });
    if (max < 0.0f) // aabb intersects "behind" the ray
        return std::nullopt;
    if (min > max) // no intersection
        return std::nullopt;
    return min;
}
}

std::optional<RayCastHit> castRay(
    ecs::World& world, const glm::vec3& rayOrigin, const glm::vec3& rayDir)
{
    std::optional<RayCastHit> hit;
    world.forEachEntity<const comp::Transform, const comp::BoxCollider>(
        [&](ecs::EntityHandle entity, const comp::Transform& trafo,
            const comp::BoxCollider& collider) {
            const auto aabb = glwx::Aabb {
                trafo.getPosition() - collider.halfExtents,
                trafo.getPosition() + collider.halfExtents,
            };
            const auto t = intersectRayAabb(rayOrigin, rayDir, aabb);
            if (t && (!hit || *t < hit->t)) {
                hit = RayCastHit { entity, *t };
            }
        });
    return hit;
}

namespace {
void integrateCylinderColliders(ecs::World& world, ecs::EntityHandle entity,
    comp::Velocity& velocity, comp::Transform& transform, const comp::CylinderCollider& collider,
    float dt)
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

void integrationSystem(ecs::World& world, float dt)
{
    // velocity will prevent this from being executed for non-local players
    world.forEachEntity<comp::Velocity, comp::Transform, const comp::CylinderCollider>(
        [&world, dt](ecs::EntityHandle entity, comp::Velocity& velocity, comp::Transform& transform,
            const comp::CylinderCollider& collider) {
            integrateCylinderColliders(world, entity, velocity, transform, collider, dt);
        });
}

void playerLookSystem(ecs::World& world, float dt)
{
    world.forEachEntity<comp::Transform, comp::PlayerInputController>(
        [dt](comp::Transform& transform, comp::PlayerInputController& ctrl) {
            const auto look
                = glm::vec2(ctrl.lookX->getState(), ctrl.lookY->getState()) * lookSensitivity;
            ctrl.yaw += -look.x;
            ctrl.pitch -= look.y;
            ctrl.pitch = std::clamp(ctrl.pitch, -glm::half_pi<float>(), glm::half_pi<float>());
            const auto pitchQuat = glm::angleAxis(ctrl.pitch, glm::vec3(1.0f, 0.0f, 0.0f));
            const auto yawQuat = glm::angleAxis(ctrl.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
            transform.setOrientation(yawQuat * pitchQuat);
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
            const auto forward = ctrl.forwards->getState() - ctrl.backwards->getState();
            const auto sideways = ctrl.right->getState() - ctrl.left->getState();
            const auto move = glm::vec3(sideways, 0.0f, -forward); // forward is -z

            auto currentMaxSpeed = maxSpeed;
            if (ctrl.sprint->getState()) {
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
