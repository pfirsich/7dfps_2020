#include "input.hpp"

#include <cassert>
#include <memory>

#include <SDL2/SDL.h>

InputManager::InputState::InputState()
    : keyboardState(SDL_GetKeyboardState(nullptr))
{
    mouseButtonState = SDL_GetRelativeMouseState(&mouseRelX, &mouseRelY);
}

void InputManager::update()
{
    inputStates.insert(inputStates.begin(), InputState());
    while (inputStates.size() > numFrames)
        inputStates.pop_back();
}

InputManager::InputManager()
{
    SDL_SetRelativeMouseMode(SDL_TRUE);
}

const InputManager::InputState& InputManager::getState(size_t pastFrame) const
{
    assert(pastFrame < inputStates.size());
    return inputStates[pastFrame];
}

bool BinaryInput::getState(size_t pastFrame) const
{
    return getState(InputManager::instance().getState(pastFrame));
}

bool BinaryInput::getPressed(size_t pastFrame) const
{
    return getState(pastFrame) && !getState(pastFrame + 1);
}

bool BinaryInput::getReleased(size_t pastFrame) const
{
    return !getState(pastFrame) && getState(pastFrame + 1);
}

bool KeyboardInput::getState(const InputManager::InputState& inputState) const
{
    return inputState.keyboardState[scancode_];
}

bool MouseButtonInput::getState(const InputManager::InputState& inputState) const
{
    return inputState.mouseButtonState & SDL_BUTTON(button_);
}

float AnalogInput::getState(size_t pastFrame) const
{
    return getState(InputManager::instance().getState(pastFrame));
}

float AnalogInput::getDelta(size_t pastFrame) const
{
    return getState(pastFrame) - getState(pastFrame + 1);
}

float MouseMoveInput::getState(const InputManager::InputState& inputState) const
{
    if (axis_ == Axis::X)
        return inputState.mouseRelX;
    if (axis_ == Axis::Y)
        return inputState.mouseRelY;
    assert(false);
}
