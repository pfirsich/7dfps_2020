#pragma once

#include <string>

#include "components.hpp"
#include "physics.hpp"

bool initSound();

void deinitSound();

void playSound(const std::string& name, float volume = 1.0f, float playbackSpeed = 1.0f);

void play3dSound(const std::string& name, const glm::vec3& position, float volume = 1.0f,
    float playbackSpeed = 1.0f);

void updateListener(const comp::Transform& listenerTransform, const glm::vec3& listenerVelocity);
