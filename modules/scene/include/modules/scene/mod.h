#pragma once

#include <flecs.h>

namespace vb {
namespace scene {

struct Scene {
    u8 reserved = 0;
};

bool is_valid(flecs::world &world, flecs::entity scene);
flecs::entity create(flecs::world &world, const char *name);
flecs::entity spawn(flecs::world &world, flecs::entity scene, const char *name);
void destroy(flecs::world &world, flecs::entity scene);

struct module {
    module(flecs::world &world);
};

} // namespace scene
} // namespace vb
