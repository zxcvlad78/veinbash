#pragma once

#include <flecs.h>

namespace vb {
namespace weapon {

struct ProjectileWeapon {
    int dmg = 0;
};

struct module {
    module(flecs::world &world);
};

} // namespace weapon
} // namespace vb
