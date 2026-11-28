#pragma once

#include <flecs.h>

namespace vb {
namespace weapon {

struct Damage {
    f32 falue;
};

struct Weapon {
    flecs::entity projectile_prefab;
};

struct module {
    module(flecs::world &world);
};

} // namespace weapon
} // namespace vb
