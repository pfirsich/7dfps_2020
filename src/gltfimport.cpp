#include "gltfimport.hpp"

#include "components.hpp"
#include "graphics.hpp"
#include "physics.hpp"
#include "shipsystem.hpp"

namespace {
template <typename T, typename Container>
T makeGlm(const Container& vals)
{
    T ret;
    const auto ptr = glm::value_ptr(ret);
    static constexpr auto N = sizeof(T) / sizeof(decltype(*ptr)); // not clean
    assert(vals.size() == N);
    for (size_t i = 0; i < vals.size(); ++i)
        ptr[i] = vals[i];
    return ret;
}

std::unordered_map<std::string, size_t> attributeLocations = {
    { "POSITION", AttributeLocations::Position },
    { "NORMAL", AttributeLocations::Normal },
    { "TANGENT", AttributeLocations::Tangent },
    { "TEXCOORD_0", AttributeLocations::TexCoord0 },
    { "TEXCOORD_1", AttributeLocations::TexCoord1 },
    { "COLOR_0", AttributeLocations::Color0 },
    { "JOINTS_0", AttributeLocations::Joints0 },
    { "WEIGHTS_0", AttributeLocations::Weights0 },
};

struct GltfImportCache {
    std::unordered_map<gltf::NodeIndex, ecs::EntityHandle> entityMap;
    std::unordered_map<gltf::MeshIndex, std::shared_ptr<Mesh>> meshMap;
    std::unordered_map<gltf::BufferViewIndex, std::shared_ptr<glw::Buffer>> bufferMap;
    std::unordered_map<gltf::MaterialIndex, std::shared_ptr<Material>> materialMap;
    std::unordered_map<gltf::TextureIndex, std::shared_ptr<glw::Texture>> textureMap;

    std::shared_ptr<glw::Buffer> getBuffer(
        const gltf::Gltf& gltfFile, gltf::BufferViewIndex bvIndex)
    {
        const auto it = bufferMap.find(bvIndex);
        if (it != bufferMap.end())
            return it->second;

        const auto& bv = gltfFile.bufferViews[bvIndex];
        assert(bv.target);
        const auto target = static_cast<glw::Buffer::Target>(*bv.target);
        const auto data = gltfFile.getBufferViewData(bvIndex);
        auto buffer = std::make_shared<glw::Buffer>();
        buffer->data(target, glw::Buffer::UsageHint::StaticDraw, data.first, data.second);
        bufferMap.emplace(bvIndex, buffer);
        return buffer;
    }

    std::shared_ptr<glw::Texture> getTexture(
        const gltf::Gltf& gltfFile, gltf::TextureIndex textureIndex)
    {
        const auto it = textureMap.find(textureIndex);
        if (it != textureMap.end())
            return it->second;

        const auto& gtexture = gltfFile.textures[textureIndex];

        auto minFilter = glw::Texture::MinFilter::LinearMipmapNearest;
        auto magFilter = glw::Texture::MagFilter::Linear;
        auto wrapS = glw::Texture::WrapMode::Repeat;
        auto wrapT = glw::Texture::WrapMode::Repeat;
        if (gtexture.sampler) {
            const auto& sampler = gltfFile.samplers[*gtexture.sampler];
            if (sampler.minFilter)
                minFilter = static_cast<glw::Texture::MinFilter>(*sampler.minFilter);
            if (sampler.magFilter)
                magFilter = static_cast<glw::Texture::MagFilter>(*sampler.magFilter);
            wrapS = static_cast<glw::Texture::WrapMode>(sampler.wrapS);
            wrapT = static_cast<glw::Texture::WrapMode>(sampler.wrapT);
        }
        const auto mipmaps = static_cast<GLenum>(minFilter)
            >= static_cast<GLenum>(glw::Texture::MinFilter::NearestMipmapNearest);

        assert(gtexture.source);
        const auto data = gltfFile.getImageData(*gtexture.source);

        auto tex = glwx::makeTexture2D(data.first, data.second, mipmaps);
        assert(tex);
        tex->setMinFilter(minFilter);
        tex->setMagFilter(magFilter);
        tex->setWrap(wrapS, wrapT);

        auto texture = std::make_shared<glw::Texture>(std::move(*tex));
        textureMap.emplace(textureIndex, texture);
        return texture;
    }

    std::shared_ptr<Material> getMaterial(
        const gltf::Gltf& gltfFile, gltf::MaterialIndex materialIndex)
    {
        const auto it = materialMap.find(materialIndex);
        if (it != materialMap.end())
            return it->second;

        const auto& gmaterial = gltfFile.materials[materialIndex];
        const auto material = std::make_shared<Material>();
        assert(gmaterial.pbrMetallicRoughness);
        const auto& pbr = *gmaterial.pbrMetallicRoughness;
        material->baseColor = makeGlm<glm::vec4>(pbr.baseColorFactor);
        if (pbr.baseColorTexture) {
            const auto& texInfo = *pbr.baseColorTexture;
            assert(texInfo.texCoord == 0);
            material->baseColorTexture = getTexture(gltfFile, texInfo.index);
        }
        materialMap.emplace(materialIndex, material);
        return material;
    }

    std::shared_ptr<Mesh> getMesh(const gltf::Gltf& gltfFile, gltf::MeshIndex meshIndex)
    {
        const auto it = meshMap.find(meshIndex);
        if (it != meshMap.end())
            return it->second;

        const auto& gmesh = gltfFile.meshes[meshIndex];
        auto mesh = std::make_shared<Mesh>();
        for (const auto& gprim : gmesh.primitives) {
            const auto mode = static_cast<glw::DrawMode>(gprim.mode);
            auto& prim = mesh->primitives.emplace_back(
                Mesh::Primitive { glwx::Primitive(mode), Material::getDefaultMaterial(), {} });

            // I need to determine a vertex format per buffer (which may be used in multiple
            // attributes), so I determine the set of buffers first, then build the formats.
            std::set<gltf::BufferViewIndex> bufferViews;
            for (const auto& attr : gprim.attributes)
                bufferViews.insert(gltfFile.accessors[attr.accessor].bufferView.value());

            for (const auto bvIndex : bufferViews) {
                glw::VertexFormat vfmt;
                std::optional<size_t> vertexCount;
                // Now find the attributes that use this buffer again to build the vertex format
                for (const auto& attr : gprim.attributes) {
                    const auto& accessor = gltfFile.accessors[attr.accessor];
                    if (accessor.bufferView.value() == bvIndex) {
                        const auto count = static_cast<size_t>(accessor.type);
                        assert(count >= 1 && count <= 4);
                        const auto componentType
                            = static_cast<glw::AttributeType>(accessor.componentType);
                        vfmt.add(accessor.byteOffset, attributeLocations.at(attr.id), count,
                            componentType, accessor.normalized);
                        vertexCount
                            = vertexCount ? std::min(*vertexCount, accessor.count) : accessor.count;
                    }
                }

                const auto& bufferView = gltfFile.bufferViews[bvIndex];
                if (bufferView.byteStride)
                    vfmt.setStride(*bufferView.byteStride);

                const auto buffer = getBuffer(gltfFile, bvIndex);
                prim.primitive.addVertexBuffer(*buffer, vfmt);
                prim.primitive.vertexRange = glwx::Primitive::Range { 0, vertexCount.value() };
                prim.buffers.push_back(std::move(buffer));
            }

            if (gprim.indices) {
                const auto& accessor = gltfFile.accessors[*gprim.indices];
                const auto type = static_cast<glw::IndexType>(accessor.componentType);
                assert(type == glw::IndexType::U8 || type == glw::IndexType::U16
                    || type == glw::IndexType::U32);
                const auto buffer = getBuffer(gltfFile, accessor.bufferView.value());
                prim.primitive.setIndexBuffer(*buffer, type);
                prim.primitive.indexRange
                    = glwx::Primitive::Range { accessor.byteOffset / glw::getIndexTypeSize(type),
                          accessor.count };
            }

            if (gprim.material) {
                prim.material = getMaterial(gltfFile, *gprim.material);
            }
        }
        meshMap.emplace(meshIndex, mesh);
        return mesh;
    }

    std::optional<gltf::NodeIndex> getNodeIndex(const gltf::Gltf& gltfFile, const std::string& name)
    {
        for (size_t i = 0; i < gltfFile.nodes.size(); ++i)
            if (gltfFile.nodes[i].name && gltfFile.nodes[i].name == name)
                return i;
        return std::nullopt;
    }

    ecs::EntityHandle getEntity(
        ecs::World& world, const gltf::Gltf& gltfFile, gltf::NodeIndex nodeIndex, bool server)
    {
        const auto it = entityMap.find(nodeIndex);
        if (it != entityMap.end())
            return it->second;

        const auto& node = gltfFile.nodes[nodeIndex];
        auto entity = world.createEntity();
        if (node.name) {
            entity.add<comp::Name>(comp::Name { *node.name });
        }
        if (node.name && node.name->find("collider") == 0) {
            assert(!node.parent);
            const auto& mesh = gltfFile.meshes[node.mesh.value()];
            assert(mesh.primitives.size() == 1);
            assert(mesh.primitives[0].attributes[0].id == "POSITION");
            const auto& accessor = gltfFile.accessors[mesh.primitives[0].attributes[0].accessor];
            const auto min = makeGlm<glm::vec3>(accessor.min);
            const auto max = makeGlm<glm::vec3>(accessor.max);
            const auto offset = (max + min) / 2.0f;
            const auto scale = (max - min) / 2.0f;
            const auto trs = std::get<gltf::Node::Trs>(node.transform);
            const auto trsScale = makeGlm<glm::vec3>(trs.scale);
            entity.add<comp::Transform>().setPosition(
                makeGlm<glm::vec3>(trs.translation) + offset * trsScale);
            entity.add<comp::BoxCollider>().halfExtents = trsScale * scale;

            const auto& extras = node.extras;
            if (!std::holds_alternative<gltf::JsonNull>(extras)) {
                const auto& obj = *std::get<std::unique_ptr<gltf::JsonObject>>(extras);
                if (obj.count("visual_link")) {
                    const auto name = std::get<std::string>(obj.at("visual_link"));
                    const auto nodeIndex = getNodeIndex(gltfFile, name);
                    if (nodeIndex) {
                        entity.add<comp::VisualLink>().entity
                            = getEntity(world, gltfFile, *nodeIndex, server);
                    } else {
                        fmt::print(stderr, "Invalid visual link '{}'\n", name);
                    }
                }
                if (obj.count("ladder")) {
                    const auto dir = std::get<std::string>(obj.at("ladder"));
                    assert(dir == "up" || dir == "down");
                    entity.add<comp::Ladder>().dir
                        = dir == "up" ? comp::Ladder::Dir::Up : comp::Ladder::Dir::Down;
                }
                if (obj.count("terminal")) {
                    fmt::print("Terminal '{}': {}\n", std::get<std::string>(obj.at("terminal")),
                        *node.name);
                    entity.add<comp::Terminal>().systemName
                        = std::get<std::string>(obj.at("terminal"));
                }
            }
        } else {
            entity.add<comp::Hierarchy>();
            entity.add<comp::Transform>().setMatrix(makeGlm<glm::mat4>(node.getTransformMatrix()));
            if (!server && node.mesh) {
                entity.add<comp::Mesh>(getMesh(gltfFile, *node.mesh));
            }
            if (node.parent) {
                auto parent = getEntity(world, gltfFile, *node.parent, server);
                comp::Hierarchy::setParent(entity, parent);
            }
            entityMap.emplace(nodeIndex, entity);
        }
        return entity;
    }
};
}

std::shared_ptr<Mesh> loadMesh(const fs::path& path)
{
    const auto gltfFileOpt = gltf::load(path);
    if (!gltfFileOpt) {
        return nullptr;
    }
    const auto& gltfFile = *gltfFileOpt;
    assert(gltfFile.scenes.size() == 1);

    GltfImportCache importCache;
    assert(gltfFile.nodes.size() == 1 && gltfFile.nodes[0].mesh);
    return importCache.getMesh(gltfFile, *gltfFile.nodes[0].mesh);
}

bool loadMap(const fs::path& path, ecs::World& world, bool server)
{
    const auto gltfFileOpt = gltf::load(path);
    if (!gltfFileOpt) {
        return false;
    }
    const auto& gltfFile = *gltfFileOpt;
    assert(gltfFile.scenes.size() == 1);

    GltfImportCache importCache;

    for (const auto nodeIndex : gltfFile.scenes[0].nodes) {
        importCache.getEntity(world, gltfFile, nodeIndex, server);
    }

    return true;
}
