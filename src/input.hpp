#pragma once

#include <memory>
#include <vector>

#include <SDL2/SDL.h>

#include "singleton.hpp"

// We have this singleton, so we can keep track of different frames in one spot
// and fetch all input data at the same time and not across a frame
class InputManager : public Singleton<InputManager> {
    friend class Singleton<InputManager>;

public:
    static constexpr size_t numFrames = 2;

    struct InputState {
        uint32_t mouseButtonState = 0;
        int mouseX = 0;
        int mouseY = 0;
        const uint8_t* keyboardState;

        InputState();
    };

    void update();

    const InputState& getState(size_t pastFrame = 0) const;

private:
    InputManager() = default;

    // index 0 is current frame, index n is n frames in the past
    std::vector<InputState> inputStates;
};

struct BinaryInput {
    virtual ~BinaryInput()
    {
    }

    bool getState(size_t pastFrame = 0) const;
    bool getPressed(size_t pastFrame = 0) const;
    bool getReleased(size_t pastFrame = 0) const;

protected:
    virtual bool getState(const InputManager::InputState& inputState) const = 0;
};

struct KeyboardInput : public BinaryInput {
    KeyboardInput(SDL_Scancode scancode)
        : scancode_(scancode)
    {
    }

private:
    bool getState(const InputManager::InputState& inputState) const override;

    SDL_Scancode scancode_;
};

struct MouseButtonInput : public BinaryInput {
    MouseButtonInput(int button)
        : button_(button)
    {
    }

private:
    bool getState(const InputManager::InputState& inputState) const override;

    int button_;
};

struct AnalogInput {
    virtual ~AnalogInput()
    {
    }

    float getState(size_t pastFrame = 0) const;
    float getDelta(size_t pastFrame = 0) const;

protected:
    virtual float getState(const InputManager::InputState& inputState) const = 0;
};

struct MouseAxisInput : public AnalogInput {
    enum class Axis { X, Y };

    MouseAxisInput(Axis axis)
        : axis_(axis)
    {
    }

private:
    float getState(const InputManager::InputState& inputState) const override;

    Axis axis_;
};
