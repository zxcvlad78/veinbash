#include <modules/base/components.h>
#include <modules/weapon/mod.h>

namespace vb {
namespace weapon {

module::module(flecs::world &world) {
    world.module<module>();
    world.component<Weapon>();

    flecs::entity SpaceShip = world.prefab("BulletSuka")
        .set<Damage>(7.8f)
        .set<base::Transform2D>(base::Transform2D())
        ;
}

} // namespace weapon
} // namespace vb
