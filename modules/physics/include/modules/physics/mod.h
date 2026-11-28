#pragma once
#include <flecs.h>
#include <cmath>

namespace vb {
namespace physics {

Vector2 normalize_vector2(Vector2 v);

struct module {
    module(flecs::world &world);
};

} // namespace physics
} // namespace vb
