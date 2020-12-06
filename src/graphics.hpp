#pragma once

#include <memory>

#include <glm/glm.hpp>

#include <glw.hpp>
#include <glwx.hpp>

#include "components.hpp"
#include "ecs.hpp"

struct Material {
    std::shared_ptr<glw::Texture> baseColorTexture = nullptr;
    glm::vec4 baseColor = glm::vec4(1.0f);

    Material();

    static std::shared_ptr<glw::Texture> getDefaultTexture();
    static std::shared_ptr<Material> getDefaultMaterial();
};

struct Mesh {
    struct Primitive {
        glwx::Primitive primitive;
        std::shared_ptr<Material> material;
        // This is just so the mesh can keep ownership its buffers
        std::vector<std::shared_ptr<glw::Buffer>> buffers;
    };

    std::vector<Primitive> primitives;
};

namespace comp {
// This thing is not data driven at all
using Mesh = std::shared_ptr<Mesh>;
}

namespace AttributeLocations {
constexpr size_t Position = 0;
constexpr size_t Normal = 1;
constexpr size_t Tangent = 2;
constexpr size_t TexCoord0 = 3;
constexpr size_t TexCoord1 = 4;
constexpr size_t Color0 = 5;
constexpr size_t Joints0 = 6;
constexpr size_t Weights0 = 7;
}

void renderSystem(
    ecs::World& world, const glm::mat4& projection, const glwx::Transform& cameraTransform);