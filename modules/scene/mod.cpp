#include <modules/scene/mod.h>

namespace vb {
namespace scene {

bool is_valid(flecs::world &world, flecs::entity scene) {
    return scene.is_alive() && scene.has<Scene>();
}

flecs::entity create(flecs::world &world, const char *name) {
    const char *n = (name && name[0]) ? name : "Scene";
    flecs::entity e = world.entity(n);
    e.add<Scene>();
    return e;
}

flecs::entity spawn(flecs::world &world, flecs::entity scene_entity, const char *name) {
    if (!is_valid(world, scene_entity)) return flecs::entity::null();

    flecs::entity e = (name && name[0]) ? world.entity(name) : world.entity();
    e.child_of(scene_entity);
    return e;
}

void destroy(flecs::world &world, flecs::entity scene_entity) {
    if (is_valid(world, scene_entity)) {
        scene_entity.destruct();
    }
}

module::module(flecs::world &world) {
    world.module<module>();
    world.component<Scene>();
}

} // namespace scene
} // namespace vb
