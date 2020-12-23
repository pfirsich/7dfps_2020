#pragma once

#include <memory>

#include <glm/glm.hpp>

#include <glw.hpp>
#include <glwx.hpp>

#include "components.hpp"
#include "ecs.hpp"
#include "shipsystem.hpp"
#include "terminaldata.hpp"

struct Plane {
    // as in nx * x + ny * y + nz * z + d = 0
    glm::vec3 normal = glm::vec3(0.0f);
    float d = 0.0f;

    float distance(const glm::vec3& point) const;
};

class Frustum {
public:
    void setPerspective(float fovy, float aspect, float znear, float zfar);

    const glm::mat4& getMatrix() const;

    bool contains(const glm::vec3& center, float radius) const;

private:
    glm::mat4 matrix_ = glm::mat4(1.0f);
    std::array<Plane, 6> planes_;
};

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
        std::vector<std::shared_ptr<glw::Buffer>> buffers = {};
    };

    std::vector<Primitive> primitives;
    glwx::Aabb aabb = glwx::Aabb {};
    float radius = 0.0f;

    void draw(const glw::ShaderProgram& shader) const;
};

struct Skybox {
    glwx::Mesh mesh;
    glw::Texture texture;

    bool load(const std::filesystem::path& posX, const std::filesystem::path& negX,
        const std::filesystem::path& posY, const std::filesystem::path& negY,
        const std::filesystem::path& posZ, const std::filesystem::path& negZ);

    void draw(const Frustum& frustum, const glwx::Transform& cameraTransform);
};

namespace comp {
// This thing is not data driven at all
using Mesh = std::shared_ptr<Mesh>;

struct RenderHighlight {
};

struct Outside {
};
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

struct RenderStats {
    size_t drawCalls = 0;
};

void resetRenderStats();
RenderStats getRenderStats();

void collisionRenderSystem(
    ecs::World& world, const Frustum& frustum, const glwx::Transform& cameraTransform);

void renderTerminalScreens(ecs::World& world, const glm::vec3& cameraPosition,
    std::unordered_map<std::string, TerminalData>& terminalData, const std::string& terminalInUse);

void renderSystem(ecs::World& world, const Frustum& frustum, const glwx::Transform& cameraTransform,
    const ShipState& shipState);

void cullingRenderSystem(
    ecs::World& world, const Frustum& frustum, const glwx::Transform& cameraTransform);
