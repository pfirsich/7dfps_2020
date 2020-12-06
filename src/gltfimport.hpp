#pragma once

#include <filesystem>

#include <gltf.hpp>

#include "ecs.hpp"

namespace fs = std::filesystem;

bool loadMap(const fs::path& path, ecs::World& world);
