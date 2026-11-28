#include <modules/character/mod.h>
#include <modules/base/components.h>
#include <modules/physics/mod.h>
#include <modules/physics/components.h>

#include <raylib.h>

namespace vb {
namespace character {

module::module(flecs::world &world) {
    world.module<module>();

    world.component<CharacterControl>();
    world.component<MoveSpeed>();
    world.component<Health>();
    world.component<EventDeath>();

    world.system<CharacterControl>("Input")
        .each([](CharacterControl &cc) {
            cc.up = IsKeyDown(KEY_W);
            cc.down = IsKeyDown(KEY_S);
            cc.left = IsKeyDown(KEY_A);
            cc.right = IsKeyDown(KEY_D);
        });

    world.system<CharacterControl, physics::WishVelocity, const MoveSpeed>("ApplyInputToWishVelocity")
        .each([](CharacterControl &cc, physics::WishVelocity &wv, const MoveSpeed &ms) {
            wv.linear.x = 0.0f;
            wv.linear.y = 0.0f;
            wv.linear.x += static_cast<float>(cc.right);
            wv.linear.x -= static_cast<float>(cc.left);
            wv.linear.y += static_cast<float>(cc.down);
            wv.linear.y -= static_cast<float>(cc.up);

            wv.linear = physics::normalize_vector2(wv.linear);
            
            wv.linear.x *= ms.x;
            wv.linear.y *= ms.y;
        });
}

} // namespace character
} // namespace vb
