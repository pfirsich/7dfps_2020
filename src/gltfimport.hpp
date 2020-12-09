#pragma once

#include <filesystem>
#include <memory>

#include <gltf.hpp>

#include "ecs.hpp"
#include "graphics.hpp"

namespace fs = std::filesystem;

std::shared_ptr<Mesh> loadMesh(const fs::path& path);
bool loadMap(const fs::path& path, ecs::World& world, bool server = false);
