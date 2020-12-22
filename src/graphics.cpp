#include "graphics.hpp"

#include <algorithm>
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

    uniform vec3 tint;
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
        float brightness = mix(nDotL, 1.0, ambientBlend);
        vec4 lit = vec4(base.rgb * tint * brightness, base.a);
        fragColor = mix(lit, glowColor, glowAmount);
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
    texture.setMagFilter(glw::Texture::MagFilter::Nearest);

    glw::VertexFormat vfmt;
    vfmt.add(0, 3, glw::AttributeType::F32);
    mesh = glwx::makeBoxMesh(vfmt, { 0 }, 1.0f, 1.0f, 1.0f);
    return true;
}

void Skybox::draw(const Frustum& frustum, const glwx::Transform& cameraTransform)
{
    static glw::ShaderProgram shader = glwx::makeShaderProgram(skyboxVert, skyboxFrag).value();
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    // get rid of translation
    auto view = glm::inverse(glm::mat4(glm::mat3(cameraTransform.getMatrix())));
    shader.setUniform("modelViewProjection", frustum.getMatrix() * view);
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

float Plane::distance(const glm::vec3& point) const
{
    return glm::dot(normal, point) + d;
}

void Frustum::setPerspective(float fovy, float aspect, float znear, float zfar)
{
    matrix_ = glm::perspective(fovy, aspect, znear, zfar);

    // https://www.iquilezles.org/www/articles/frustum/frustum.htm
    // Though it seems wrong to me that the z-components increase with fovy, as they should point
    // more *forwards*, which as iq said in his article, is negative z.
    // So I changed all the z signs and it works that way (and only that way).
    // Also we need to normalize the normal vectors for left/right, since we want to make sphere
    // checks and not just point checks (as iq did).
    // That `* aspect`-trick is INSANE and it took me 20 minutes to verify that it works. iq is a
    // beast!
    const float s = std::sin(fovy);
    const float c = std::cos(fovy);
    planes_[0] = Plane { glm::vec3(0.0f, -c, -s), 0.0f }; // top
    planes_[1] = Plane { glm::vec3(0.0f, c, -s), 0.0f }; // bottom
    planes_[2] = Plane { glm::normalize(glm::vec3(c, 0.0f, -s * aspect)), 0.0f }; // left
    planes_[3] = Plane { glm::normalize(glm::vec3(-c, 0.0f, -s * aspect)), 0.0f }; // right
    planes_[4] = Plane { glm::vec3(0.0f, 0.0f, 1.0f), zfar }; // far
    planes_[5] = Plane { glm::vec3(0.0f, 0.0f, -1.0f), -znear }; // near
}

const glm::mat4& Frustum::getMatrix() const
{
    return matrix_;
}

bool Frustum::contains(const glm::vec3& center, float radius) const
{
    for (size_t i = 0; i < planes_.size(); ++i) {
        // planes point inward, so negative distance is outside
        const auto dist = planes_[i].distance(center);
        if (-dist > radius) {
            return false;
        }
    }
    return true;
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

void renderSystem(ecs::World& world, const Frustum& frustum, const glwx::Transform& cameraTransform,
    const ShipState& shipState)
{
    const auto& shader = getShader();
    shader.bind();
    shader.setUniform("lightDir", glm::vec3(0.0f, 0.0f, 1.0f));

    shader.setUniform("projectionMatrix", frustum.getMatrix());
    const auto view = glm::inverse(cameraTransform.getMatrix());
    shader.setUniform("viewMatrix", view);
    shader.setUniform("ambientBlend", 0.8f);
    shader.setUniform("glowColor", glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
    const float glowAmount = rescale(
        std::cos(glwx::getTime() * glowFrequency * 2.0f * M_PI), -1.0f, 1.0f, glowMin, glowMax);

    world.forEachEntity<const comp::Transform, const comp::Mesh>(
        [&frustum, &shipState, &view, &shader, glowAmount](
            ecs::EntityHandle entity, const comp::Transform& transform, const comp::Mesh& mesh) {
            const auto model = getModelMatrix(entity, transform);
            shader.setUniform("modelMatrix", model);
            const auto modelView = view * model;

            const auto bsCenter = glm::vec3(modelView * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            const auto modelScale
                = std::max({ glm::length(model[0]), glm::length(model[1]), glm::length(model[2]) });
            const auto bsRadius = mesh->radius * modelScale;
            if (!frustum.contains(bsCenter, bsRadius))
                return;

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

            const auto lightsOff = shipState.reactorPower == 0.0f;
            const auto lightsOffColor = glm::vec3(0.22f, 0.08f, 0.08f);
            if (lightsOff && !entity.has<comp::Outside>()) {
                shader.setUniform("tint", lightsOffColor);
            } else {
                shader.setUniform("tint", glm::vec3(1.0f));
            }

            mesh->draw(shader);
        });
}

namespace {
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
}

void collisionRenderSystem(
    ecs::World& world, const Frustum& frustum, const glwx::Transform& cameraTransform)
{
    const auto& shader = getShader();
    static const CollisionBoxMesh box;
    static const auto texture = glwx::makeTexture2D(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    shader.setUniform("projectionMatrix", frustum.getMatrix());
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

namespace {
struct BoundingSphereMesh {
    BoundingSphereMesh()
    {
        glw::VertexFormat vfmt;
        vfmt.add(AttributeLocations::Position, 3, glw::AttributeType::F32);
        vfmt.add(AttributeLocations::Normal, 3, glw::AttributeType::F32);
        vfmt.add(AttributeLocations::TexCoord0, 2, glw::AttributeType::U16, true);
        mesh = glwx::makeSphereMesh(vfmt,
            { AttributeLocations::Position, AttributeLocations::TexCoord0,
                AttributeLocations::Normal },
            1.0f, 32, 24);
    }

    glwx::Mesh mesh;
};
}

void cullingRenderSystem(
    ecs::World& world, const Frustum& frustum, const glwx::Transform& cameraTransform)
{
    const auto& shader = getShader();
    static const BoundingSphereMesh sphere;
    static const auto texture = glwx::makeTexture2D(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));

    shader.setUniform("projectionMatrix", frustum.getMatrix());
    const auto view = glm::inverse(cameraTransform.getMatrix());
    shader.setUniform("viewMatrix", view);

    texture.bind(0);
    shader.setUniform("baseColorTexture", 0);
    shader.setUniform("baseColorFactor", glm::vec4(1.0f));
    shader.setUniform("ambientBlend", 0.2f);
    shader.setUniform("glowAmount", 0.0f);

    world.forEachEntity<const comp::Transform, const comp::Mesh>(
        [&frustum, &view, &shader](
            ecs::EntityHandle entity, const comp::Transform& transform, const comp::Mesh& mesh) {
            const auto objModel = getModelMatrix(entity, transform);
            const auto objPos = objModel * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            const auto modelScale = std::max(
                { glm::length(objModel[0]), glm::length(objModel[1]), glm::length(objModel[2]) });
            const auto bsRadius = mesh->radius * modelScale;
            const auto model = glm::translate(glm::vec3(objPos)) * glm::scale(glm::vec3(bsRadius));

            shader.setUniform("modelMatrix", model);
            const auto modelView = view * model;
            const auto normal = glm::mat3(glm::transpose(glm::inverse(modelView)));
            shader.setUniform("normalMatrix", normal);

            const auto bsCenter = glm::vec3(view * objPos);
            if (frustum.contains(bsCenter, bsRadius)) {
                shader.setUniform("baseColorFactor", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
            } else {
                shader.setUniform("baseColorFactor", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
            }

            sphere.mesh.draw();
            renderStats.drawCalls++;
        });
}
