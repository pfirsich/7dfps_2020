#include "sound.hpp"

#include <memory>
#include <string>
#include <unordered_map>

#include <soloud_wav.h>

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

namespace {
static constexpr auto defaultMinDistance = 1.0f;
static constexpr auto defaultMaxDistance = 60.0f;
static constexpr auto defaultHalfDistance = 30.0f;

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

constexpr float getRolloff(float minDistance, float halfDistance)
{
    return minDistance / (halfDistance - minDistance);
}

struct SoundProperties {
    std::string filename;
    float volume = 1.0f;
    float minDistance = defaultMinDistance;
    float maxDistance = defaultMaxDistance;
    float halfDistance = defaultHalfDistance;

    static SoundProperties getFromLuaObject(sol::object obj)
    {
        if (obj.is<std::string>()) {
            return SoundProperties { obj.as<std::string>() };
        } else if (obj.is<sol::table>()) {
            const auto table = obj.as<sol::table>();
            auto props = SoundProperties { table["file"].get<std::string>() };
            if (table["volume"] != sol::nil)
                props.volume = table["volume"].get<float>();
            if (table["halfDistance"] != sol::nil)
                props.halfDistance = table["halfDistance"].get<float>();
            if (table["minDistance"] != sol::nil)
                props.minDistance = table["minDistance"].get<float>();
            if (table["maxDistance"] != sol::nil)
                props.maxDistance = table["maxDistance"].get<float>();
            return props;
        }
        std::abort();
    }
};
}

SoLoud::Soloud soloud;

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
        const auto props = SoundProperties::getFromLuaObject(value);
        const auto path = "media/sounds/" + props.filename;
        auto source = std::make_unique<SoLoud::Wav>();
        if (source->load(path.c_str()) != 0) {
            fmt::print(stderr, "Could not load sound '{}'\n", path);
            return false;
        }
        const auto rolloff = getRolloff(props.minDistance, props.halfDistance);
        source->set3dMinMaxDistance(props.minDistance, props.maxDistance);
        source->set3dAttenuation(SoLoud::AudioSource::INVERSE_DISTANCE, rolloff);

        const auto nameStr = name.as<std::string>();
        const auto [it, inserted]
            = sounds.emplace(nameStr, SoundData { nameStr, std::move(source), props.volume });
        assert(inserted);
    }
    return true;
}

void deinitSound()
{
    soloud.deinit();
}

SoLoud::handle playSound(const std::string& name, float volume, float playbackSpeed)
{
    const auto& sound = getSound(name);
    auto handle = soloud.play(*sound.source, sound.volume * volume, 0.0f, true);
    soloud.setRelativePlaySpeed(handle, playbackSpeed);
    soloud.setPause(handle, false);
    return handle;
}

SoLoud::handle play3dSound(
    const std::string& name, const glm::vec3& position, float volume, float playbackSpeed)
{
    const auto& sound = getSound(name);
    auto handle = soloud.play3d(*sound.source, position.x, position.y, position.z, 0.0f, 0.0f, 0.0f,
        sound.volume * volume, true);
    soloud.setRelativePlaySpeed(handle, playbackSpeed);
    soloud.setPause(handle, false);
    return handle;
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
