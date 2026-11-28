#include <modules/weapon/mod.h>

namespace vb {
namespace weapon {

module::module(flecs::world &world) {
    world.module<module>();
    world.component<ProjectileWeapon>();
}

} // namespace weapon
} // namespace vb
