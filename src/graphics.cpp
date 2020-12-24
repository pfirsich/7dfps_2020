#include "graphics.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>

#include <glm/gtx/transform.hpp>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <glwx.hpp>

#include "components.hpp"
#include "constants.hpp"
#include "imgui.hpp"
#include "physics.hpp"
#include "terminaldata.hpp"
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

const auto terminalVert = R"(
    #version 330 core

    uniform mat4 modelMatrix;
    uniform mat4 viewMatrix;
    uniform mat4 projectionMatrix;

    layout (location = 0) in vec3 attrPosition;
    layout (location = 3) in vec2 attrTexCoords;

    out vec2 texCoords;

    void main() {
        texCoords = attrTexCoords;
        gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(attrPosition, 1.0);
    }
)"sv;

const auto terminalFrag = R"(
    #version 330 core

    uniform vec4 baseColorFactor;
    uniform sampler2D baseColorTexture;
    uniform vec2 texCoordScale;
    uniform vec2 texCoordOffset;

    in vec2 texCoords;

    out vec4 fragColor;

    vec2 flipX(vec2 tc) {
        return vec2(1.0 - tc.x, tc.y);
    }

    void main() {
        // I don't know why I have to flipX, but I do
        vec2 tc = flipX(texCoords) * texCoordScale + texCoordOffset;
        fragColor = baseColorFactor * texture(baseColorTexture, tc);
    }
)"sv;

const auto whiteFrag = R"(
    #version 330 core

    out vec4 fragColor;

    void main() {
        fragColor = vec4(1.0);
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
struct TimeDelta {
    float step()
    {
        const auto now = glwx::getTime();
        const auto dt = now - last;
        last = now;
        return dt;
    }

private:
    float last;
};

struct TerminalAtlas {
    glm::ivec2 size;
    glm::ivec2 screenSize;
    glwx::RenderTarget renderTarget;
    std::unordered_map<std::string, glm::vec2> textureOffsets;

    TerminalAtlas(const glm::ivec2& atlasSize, const glm::ivec2& screenSize)
        : size(atlasSize)
        , screenSize(screenSize)
        , renderTarget(
              glwx::makeRenderTarget(atlasSize.x, atlasSize.y, { glw::ImageFormat::Rgba }, {}))
    {
    }

    glm::vec2 getOffset(const std::string& systemName)
    {
        const auto it = textureOffsets.find(systemName);
        if (it == textureOffsets.end()) {
            const auto screensPerRow = static_cast<size_t>(size.x / screenSize.x);
            const auto row = textureOffsets.size() / screensPerRow;
            const auto col = textureOffsets.size() % screensPerRow;
            const auto offset = glm::vec2(col * screenSize.x, row * screenSize.y);
            textureOffsets.emplace(systemName, offset);
            return offset;
        }
        return it->second;
    }

    glm::vec2 getTextureScale()
    {
        return glm::vec2(
            screenSize.x / static_cast<float>(size.x), screenSize.y / static_cast<float>(size.y));
    }

    glm::vec2 getTextureOffset(const std::string& systemName)
    {
        const auto off = getOffset(systemName);
        return glm::vec2(off.x / size.x, (size.y - off.y - screenSize.y) / size.y);
    }
};

TerminalAtlas& getTerminalAtlas()
{
    static TerminalAtlas atlas(glm::ivec2(4096), glm::ivec2(1024));
    return atlas;
}

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

static std::map<std::string, glwx::RenderTarget> terminalScreenTargets;

void renderTerminalScreens(ecs::World& world, const glm::vec3& cameraPosition,
    std::unordered_map<std::string, TerminalData>& termData, const std::string& terminalInUse)
{
    const auto vp = glw::State::instance().getViewport();

    auto& atlas = getTerminalAtlas();
    atlas.renderTarget.bind();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const auto size = atlas.size;
    drawImgui(size.x, size.y, [&world, &cameraPosition, &termData, &terminalInUse, &atlas]() {
        world.forEachEntity<comp::Transform, comp::TerminalScreen>(
            [&cameraPosition, &termData, &terminalInUse, &atlas](
                const comp::Transform& transform, const comp::TerminalScreen& screen) {
                if (glm::abs(transform.getPosition().y - cameraPosition.y) > floorHeight / 2.0f) {
                    return;
                }

                const auto& system = screen.system;
                auto& term = termData[system];

                const auto pos = atlas.getOffset(system);
                const auto margin = 20;
                ImGui::SetNextWindowPos(ImVec2(pos.x + margin, pos.y + margin));
                ImGui::SetNextWindowSize(
                    ImVec2(atlas.screenSize.x - margin * 2, atlas.screenSize.y - margin * 2));
                ImGui::Begin(system.c_str(), nullptr, ImGuiWindowFlags_NoDecoration);
                ImGui::BeginChild("Child",
                    ImVec2(ImGui::GetWindowContentRegionWidth(), ImGui::GetWindowHeight() - 45),
                    false, ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextUnformatted(term.output.c_str());
                ImGui::PopTextWrapPos();
                ImGui::SetScrollY(std::min(term.scroll, ImGui::GetScrollMaxY()));
                term.lastMaxScroll = ImGui::GetScrollMaxY();
                ImGui::EndChild();
                ImGui::PushItemWidth(-1);
                // https://github.com/ocornut/imgui/issues/455
                // This cost some time. the InputText keeps focus the whole time it's visible,
                // so it gets to own inputText.
                static std::string inputText;
                ImGuiInputTextFlags flags = 0;
                // If term.input got changed from the outside, it's because of choosing history
                // entries or pressing return to execute.
                if (system == terminalInUse && inputText != term.input) {
                    inputText = term.input;
                    flags = ImGuiInputTextFlags_ReadOnly;
                }
                if (!term.inputEnabled) {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);
                }
                ImGui::InputText("Execute", &inputText, flags);
                term.input = inputText;
                if (!term.inputEnabled) {
                    ImGui::PopStyleVar();
                }
                if (system == terminalInUse) {
                    ImGui::SetKeyboardFocusHere(-1);
                }
                ImGui::PopItemWidth();
                ImGui::End();
            });

        if (terminalInUse.empty()) {
            // We want to take focus away from all terminals, so I create a dummy off-screen
            // window that takes the focus
            ImGui::SetKeyboardFocusHere();
            ImGui::SetNextWindowPos(ImVec2(atlas.size.x, atlas.size.y));
            ImGui::Begin("Focus Dummy");
            ImGui::End();
        }
    });

    glw::Framebuffer::unbind();
    glw::State::instance().setViewport(vp);
}

void renderSystem(ecs::World& world, const Frustum& frustum, const glwx::Transform& cameraTransform,
    const ShipState& shipState)
{
    static TimeDelta timeDelta;
    const auto dt = timeDelta.step();

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

    const auto lightsOffColor = glm::vec3(0.22f, 0.08f, 0.08f);
    const auto lightsOff = shipState.reactorPower == 0.0f;
    static float lerpedPower = shipState.reactorPower;
    lerpedPower = approach(lerpedPower, shipState.reactorPower, 1.0f / 0.5f * dt);
    const auto tintLerp = std::cos(std::exp(-lerpedPower) * glm::pi<float>() * 11.0f) * 0.5f + 0.5f;
    const auto lightTint = glm::mix(lightsOffColor, glm::vec3(1.0f), tintLerp);

    world.forEachEntity<const comp::Transform, const comp::Mesh>(
        [&frustum, &shipState, &view, &shader, glowAmount, lightTint](
            ecs::EntityHandle entity, const comp::Transform& transform, const comp::Mesh& mesh) {
            if (entity.has<comp::TerminalScreen>())
                return;

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

            if (!entity.has<comp::Outside>()) {
                shader.setUniform("tint", lightTint);
            } else {
                shader.setUniform("tint", glm::vec3(1.0f));
            }

            mesh->draw(shader);
        });

    static const auto terminalShader = glwx::makeShaderProgram(terminalVert, terminalFrag).value();
    terminalShader.bind();
    terminalShader.setUniform("projectionMatrix", frustum.getMatrix());
    terminalShader.setUniform("viewMatrix", view);
    auto& atlas = getTerminalAtlas();
    auto& atlasTexture = atlas.renderTarget.getTexture(glw::Framebuffer::Attachment::Color0);
    atlasTexture.bind(0);
    atlasTexture.generateMipmaps();
    atlasTexture.setMinFilter(glw::Texture::MinFilter::LinearMipmapLinear);
    terminalShader.setUniform("baseColorFactor", glm::vec4(1.0f));
    terminalShader.setUniform("baseColorTexture", 0);
    terminalShader.setUniform("texCoordScale", atlas.getTextureScale());

    world.forEachEntity<comp::Transform, comp::Mesh, comp::TerminalScreen>(
        [&frustum, &view, &atlas](ecs::EntityHandle entity, comp::Transform& transform,
            const comp::Mesh& mesh, const comp::TerminalScreen& screen) {
            const auto model = getModelMatrix(entity, transform);
            terminalShader.setUniform("modelMatrix", model);

            terminalShader.setUniform("texCoordOffset", atlas.getTextureOffset(screen.system));
            for (const auto& prim : mesh->primitives) {
                prim.primitive.draw();
                renderStats.drawCalls++;
            }
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
