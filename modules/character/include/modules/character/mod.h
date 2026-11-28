#pragma once

#include <flecs.h>

namespace vb {
namespace character {

struct Health {
    float points = 0.0f;
    float max_points = 0.0f;
};

struct EventDeath {};

struct CharacterControl {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
};

struct MoveSpeed {
    float x = 0.0f;
    float y = 0.0f;
};

struct module {
    module(flecs::world &world);
};

} // namespace character
} // namespace vb
