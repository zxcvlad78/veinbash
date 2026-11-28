#pragma once

#include <flecs.h>
#include <raylib.h>

namespace vb {
namespace base {

struct Velocity {
    Vector2 linear{0.0f, 0.0f};
    float angular = 0.0f;
};

struct WishVelocity {
    Vector2 linear{0.0f, 0.0f};
    float angular = 0.0f;
};

struct Transform2D {
    Vector2 translation{0.0f, 0.0f};
    Vector2 scale{1.0f, 1.0f};
    float rotation = 0.0f;
};

Vector2 normalize_vector2(Vector2 v);

struct module {
    module(flecs::world &world);
};

} // namespace base
} // namespace vb
