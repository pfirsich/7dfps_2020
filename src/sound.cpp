#include "sound.hpp"

#include <memory>
#include <string>
#include <unordered_map>

#include <soloud.h>
#include <soloud_wav.h>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace {
SoLoud::Soloud soloud;

struct SoundData {
    std::string name;
    std::unique_ptr<SoLoud::AudioSource> source;
    float volume;
};
std::unordered_map<std::string, SoundData> sounds;

SoundData& getSound(const std::string& name)
{
    const auto it = sounds.find(name);
    if (it == sounds.end()) {
        fmt::print(stderr, "Could not play unknown sound '{}'\n", name);
        std::abort();
    }
    return it->second;
}
}

bool initSound()
{
    const auto res = soloud.init(SoLoud::Soloud::CLIP_ROUNDOFF, SoLoud::Soloud::SDL2);
    if (res != 0) {
        fmt::print(stderr, "SoLoud could not be initialized: {}\n", res);
        return false;
    }

    sol::state lua;
    const sol::table soundmap = lua.script_file("media/sounds/soundmap.lua");
    for (auto [name, value] : soundmap) {
        const auto path = "media/sounds/" + value.as<std::string>();
        auto source = std::make_unique<SoLoud::Wav>();
        if (source->load(path.c_str()) != 0) {
            fmt::print(stderr, "Could not load sound '{}'\n", path);
            return false;
        }
        const auto [it, inserted] = sounds.emplace(
            name.as<std::string>(), SoundData { name.as<std::string>(), std::move(source), 1.0f });
        assert(inserted);
    }
    return true;
}

void deinitSound()
{
    soloud.deinit();
}

void playSound(const std::string& name, float volume, float playbackSpeed)
{
    const auto& sound = getSound(name);
    auto handle = soloud.play(*sound.source, sound.volume * volume, 0.0f, true);
    soloud.setRelativePlaySpeed(handle, playbackSpeed);
    soloud.setPause(handle, false);
}

void play3dSound(
    const std::string& name, const glm::vec3& position, float volume, float playbackSpeed)
{
    const auto& sound = getSound(name);
    auto handle = soloud.play3d(*sound.source, position.x, position.y, position.z, 0.0f, 0.0f, 0.0f,
        sound.volume * volume, true);
    soloud.setRelativePlaySpeed(handle, playbackSpeed);
    soloud.setPause(handle, false);
}

void updateListener(const comp::Transform& listenerTransform, const glm::vec3& listenerVelocity)
{
    const auto pos = listenerTransform.getPosition();
    const auto lookAt = listenerTransform.getForward();
    const auto up = listenerTransform.getUp();
    soloud.set3dListenerParameters(pos.x, pos.y, pos.z, lookAt.x, lookAt.y, lookAt.z, up.x, up.y,
        up.z, listenerVelocity.x, listenerVelocity.y, listenerVelocity.z);
    soloud.update3dAudio();
}
