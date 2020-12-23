#pragma once

#include <string>

#include <glm/glm.hpp>

#include "ecs.hpp"

namespace comp {
struct Hierarchy {
    ecs::EntityHandle parent;
    ecs::EntityHandle firstChild;
    ecs::EntityHandle prevSibling;
    ecs::EntityHandle nextSibling;

    static void removeParent(ecs::EntityHandle& entity);
    static void setParent(ecs::EntityHandle& entity, ecs::EntityHandle& parent);
};

struct Name {
    std::string value;

    static std::string get(ecs::EntityHandle entity);
    static ecs::EntityHandle find(ecs::World& world, const std::string& name);
};

struct Rotate {
    glm::vec3 axis;
    float frequency;
};

struct SpawnPoint {
};

struct TerminalScreen {
    std::string system;
};
}
