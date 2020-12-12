#pragma once

#include <filesystem>
#include <memory>
#include <optional>

#include <gltf.hpp>

#include "ecs.hpp"
#include "graphics.hpp"

namespace fs = std::filesystem;

class GltfFile {
public:
    ~GltfFile();
    GltfFile(const GltfFile&) = delete;
    GltfFile(GltfFile&&) = default;

    void instantiate(ecs::World& world, bool server = false) const;
    std::shared_ptr<Mesh> getMesh(const std::string& name) const;

    static std::optional<GltfFile> load(const fs::path& path);

private:
    struct ImportCache;

    GltfFile(gltf::Gltf&& gltfFile);

    gltf::Gltf gltfFile;
    std::unique_ptr<ImportCache> importCache;
};
