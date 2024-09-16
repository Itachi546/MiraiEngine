#include "InputDevice.hpp"
namespace mirai
{
    void Input::update()
    {
        for (auto &key : keys)
        {
            key.wasDown = key.isDown;
        }
    }
} // namespace mirai