#include "graphics.hpp"

#include <string_view>

#include <glm/gtx/transform.hpp>

#include "components.hpp"
#include "physics.hpp"

using namespace std::literals;

namespace {
const auto vert = R"(
    #version 330 core

    uniform mat4 modelMatrix;
    uniform mat4 viewMatrix;
    uniform mat4 projectionMatrix;
    uniform mat3 normalMatrix;

    layout (location = 0) in vec3 attrPosition;
    layout (location = 1) in vec3 attrNormal;
    layout (location = 3) in vec2 attrTexCoords;

    out vec2 texCoords;
    out vec3 normal; // view space

    void main() {
        texCoords = attrTexCoords;
        normal = normalMatrix * attrNormal;
        gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(attrPosition, 1.0);
    }
)"sv;

const auto frag = R"(
    #version 330 core

    const float ambient = 1.0;
    const float lightIntensity = 0.0;

    uniform vec4 baseColorFactor;
    uniform sampler2D baseColorTexture;
    uniform vec3 lightDir; // view space

    in vec2 texCoords;
    in vec3 normal;

    out vec4 fragColor;

    void main() {
        vec4 base = baseColorFactor * texture(baseColorTexture, texCoords);
        float nDotL = max(dot(lightDir, normalize(normal)), 0.0);
        fragColor = vec4(base.rgb * ambient + base.rgb * nDotL * lightIntensity, base.a);
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

void renderSystem(
    ecs::World& world, const glm::mat4& projection, const glwx::Transform& cameraTransform)
{
    static glw::ShaderProgram shader = glwx::makeShaderProgram(vert, frag).value();
    shader.bind();
    shader.setUniform("lightDir", glm::vec3(0.0f, 0.0f, 1.0f));

    shader.setUniform("projectionMatrix", projection);
    const auto view = glm::inverse(cameraTransform.getMatrix());
    shader.setUniform("viewMatrix", view);

    world.forEachEntity<const comp::Hierarchy, const comp::Transform, const comp::Mesh>(
        [&view](const comp::Hierarchy& hierarchy, const comp::Transform& transform,
            const comp::Mesh& mesh) {
            auto parent = hierarchy.parent;
            const auto model = parent && parent.has<comp::Transform>()
                ? parent.get<comp::Transform>().getMatrix() * transform.getMatrix()
                : transform.getMatrix();

            shader.setUniform("modelMatrix", model);
            const auto modelView = view * model;
            const auto normal = glm::mat3(glm::transpose(glm::inverse(modelView)));
            shader.setUniform("normalMatrix", normal);

            for (const auto& prim : mesh->primitives) {
                const auto& material
                    = prim.material ? *prim.material : *Material::getDefaultMaterial();
                material.baseColorTexture->bind(0);
                shader.setUniform("baseColorTexture", 0);
                shader.setUniform("baseColorFactor", material.baseColor);

                prim.primitive.draw();
            }
        });
}
