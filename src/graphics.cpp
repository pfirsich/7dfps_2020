#include "graphics.hpp"

#include <string_view>

#include <glm/gtx/transform.hpp>

#include <glwx.hpp>

#include "components.hpp"
#include "constants.hpp"
#include "physics.hpp"
#include "util.hpp"

using namespace std::literals;

namespace {
const auto vert = R"(
    #version 330 core

    uniform mat4 modelMatrix;
    uniform mat4 viewMatrix;
    uniform mat4 projectionMatrix;
    uniform mat3 normalMatrix;
    uniform float blowup;

    layout (location = 0) in vec3 attrPosition;
    layout (location = 1) in vec3 attrNormal;
    layout (location = 3) in vec2 attrTexCoords;

    out vec2 texCoords;
    out vec3 normal; // view space

    void main() {
        texCoords = attrTexCoords;
        normal = normalMatrix * attrNormal;
        gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(attrPosition + attrNormal * blowup, 1.0);
    }
)"sv;

const auto frag = R"(
    #version 330 core

    uniform float ambientBlend = 0.8;

    uniform vec4 baseColorFactor;
    uniform sampler2D baseColorTexture;
    uniform vec3 lightDir; // view space
    uniform vec4 glowColor;
    uniform float glowAmount;

    in vec2 texCoords;
    in vec3 normal;

    out vec4 fragColor;

    void main() {
        vec4 base = baseColorFactor * texture(baseColorTexture, texCoords);
        float nDotL = max(dot(lightDir, normalize(normal)), 0.0);
        fragColor = mix(vec4(base.rgb * mix(nDotL, 1.0, ambientBlend), base.a), glowColor, glowAmount);
    }
)"sv;

const auto skyboxVert = R"(
    #version 330 core
    out vec3 texCoords;

    uniform mat4 modelViewProjection;

    layout(location = 0) in vec3 attrPosition;

    void main()
    {
        texCoords = attrPosition;
        gl_Position = modelViewProjection * vec4(attrPosition, 1.0);
        gl_Position.z = gl_Position.w; // make sure z will be 1.0 = far away
    }
)"sv;

const auto skyboxFrag = R"(
    #version 330 core
    out vec4 fragColor;

    in vec3 texCoords;

    uniform samplerCube skyboxTexture;

    void main()
    {
        fragColor = texture(skyboxTexture, texCoords);
    }
)"sv;
}

RenderStats renderStats;

bool Skybox::load(const std::filesystem::path& posX, const std::filesystem::path& negX,
    const std::filesystem::path& posY, const std::filesystem::path& negY,
    const std::filesystem::path& posZ, const std::filesystem::path& negZ)
{
    auto skyboxTextureOpt = glwx::makeCubeTexture(posX, negX, posY, negY, posZ, negZ);
    if (!skyboxTextureOpt) {
        return false;
    }
    texture = std::move(*skyboxTextureOpt);

    glw::VertexFormat vfmt;
    vfmt.add(0, 3, glw::AttributeType::F32);
    mesh = glwx::makeBoxMesh(vfmt, { 0 }, 1.0f, 1.0f, 1.0f);
    return true;
}

void Skybox::draw(const glm::mat4& projection, const glwx::Transform& cameraTransform)
{
    static glw::ShaderProgram shader = glwx::makeShaderProgram(skyboxVert, skyboxFrag).value();
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    // get rid of translation
    auto view = glm::inverse(glm::mat4(glm::mat3(cameraTransform.getMatrix())));
    shader.setUniform("modelViewProjection", projection * view);
    texture.bind(0);
    shader.setUniform("skyboxTexture", 0);
    mesh.draw();
    renderStats.drawCalls++;
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}

Material::Material()
    : baseColorTexture(getDefaultTexture())
{
}

std::shared_ptr<glw::Texture> Material::getDefaultTexture()
{
    static auto defaultTexture
        = std::make_shared<glw::Texture>(glwx::makeTexture2D(glm::vec4(1.0f)));
    return defaultTexture;
}

std::shared_ptr<Material> Material::getDefaultMaterial()
{
    static auto defaultMaterial = std::make_shared<Material>();
    return defaultMaterial;
}

void Mesh::draw(const glw::ShaderProgram& shader) const
{
    for (const auto& prim : primitives) {
        const auto& material = prim.material ? *prim.material : *Material::getDefaultMaterial();
        material.baseColorTexture->bind(0);
        shader.setUniform("baseColorTexture", 0);
        shader.setUniform("baseColorFactor", material.baseColor);
        prim.primitive.draw();
        renderStats.drawCalls++;
    }
}

namespace {
glw::ShaderProgram& getShader()
{
    static glw::ShaderProgram shader = glwx::makeShaderProgram(vert, frag).value();
    return shader;
}

glm::mat4 getModelMatrix(ecs::EntityHandle entity, const comp::Transform& transform)
{
    if (entity.has<comp::Hierarchy>()) {
        auto parent = entity.get<comp::Hierarchy>().parent;
        if (parent && parent.has<comp::Transform>()) {
            return parent.get<comp::Transform>().getMatrix() * transform.getMatrix();
        }
    }
    return transform.getMatrix();
}
}

void resetRenderStats()
{
    renderStats = RenderStats {};
}

RenderStats getRenderStats()
{
    return renderStats;
}

void renderSystem(
    ecs::World& world, const glm::mat4& projection, const glwx::Transform& cameraTransform)
{
    const auto& shader = getShader();
    shader.bind();
    shader.setUniform("lightDir", glm::vec3(0.0f, 0.0f, 1.0f));

    shader.setUniform("projectionMatrix", projection);
    const auto view = glm::inverse(cameraTransform.getMatrix());
    shader.setUniform("viewMatrix", view);
    shader.setUniform("ambientBlend", 0.8f);
    shader.setUniform("glowColor", glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
    const float glowAmount = rescale(
        std::cos(glwx::getTime() * glowFrequency * 2.0f * M_PI), -1.0f, 1.0f, glowMin, glowMax);

    world.forEachEntity<const comp::Transform, const comp::Mesh>(
        [&view, &shader, glowAmount](
            ecs::EntityHandle entity, const comp::Transform& transform, const comp::Mesh& mesh) {
            const auto model = getModelMatrix(entity, transform);
            shader.setUniform("modelMatrix", model);
            const auto modelView = view * model;
            const auto normal = glm::mat3(glm::transpose(glm::inverse(modelView)));
            shader.setUniform("normalMatrix", normal);

            const auto highlighted = entity.has<comp::RenderHighlight>();
            if (highlighted) {
                glFrontFace(GL_CW);
                shader.setUniform("glowAmount", 1.0f);
                shader.setUniform("blowup", outlineBlowup);
                mesh->draw(shader);
                glFrontFace(GL_CCW);
            }

            shader.setUniform("glowAmount", highlighted ? glowAmount : 0.0f);
            shader.setUniform("blowup", 0.0f);

            mesh->draw(shader);
        });
}

struct CollisionBoxMesh {
    CollisionBoxMesh()
    {
        glw::VertexFormat vfmt;
        vfmt.add(AttributeLocations::Position, 3, glw::AttributeType::F32);
        vfmt.add(AttributeLocations::Normal, 3, glw::AttributeType::F32);
        vfmt.add(AttributeLocations::TexCoord0, 2, glw::AttributeType::U16, true);
        mesh = glwx::makeBoxMesh(vfmt,
            { AttributeLocations::Position, AttributeLocations::TexCoord0,
                AttributeLocations::Normal },
            2.0f, 2.0f, 2.0f);
    }

    glwx::Mesh mesh;
};

void collisionRenderSystem(
    ecs::World& world, const glm::mat4& projection, const glwx::Transform& cameraTransform)
{
    const auto& shader = getShader();
    static const CollisionBoxMesh box;
    static const auto texture = glwx::makeTexture2D(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    shader.setUniform("projectionMatrix", projection);
    const auto view = glm::inverse(cameraTransform.getMatrix());
    shader.setUniform("viewMatrix", view);

    texture.bind(0);
    shader.setUniform("baseColorTexture", 0);
    shader.setUniform("baseColorFactor", glm::vec4(1.0f));
    shader.setUniform("ambientBlend", 0.2f);
    shader.setUniform("glowAmount", 0.0f);

    world.forEachEntity<const comp::Transform, const comp::BoxCollider>(
        [&view, &shader](const comp::Transform& transform, const comp::BoxCollider& collider) {
            const auto model
                = transform.getMatrix() * glm::scale(glm::mat4(1.0f), collider.halfExtents);
            shader.setUniform("modelMatrix", model);
            const auto modelView = view * model;
            const auto normal = glm::mat3(glm::transpose(glm::inverse(modelView)));
            shader.setUniform("normalMatrix", normal);
            box.mesh.draw();
            renderStats.drawCalls++;
        });
}
