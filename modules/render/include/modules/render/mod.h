#pragma once

#include <flecs.h>
#include <raylib.h>

namespace vb {
namespace render {

struct Sprite {
    u64 texture = 0;
    Rectangle rect{0.0f, 0.0f, 0.0f, 0.0f};
};

struct module {
    module(flecs::world &world);
};

} // namespace render
} // namespace vb
