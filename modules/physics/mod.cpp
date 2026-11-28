#include <modules/base/components.h>

#include <modules/physics/mod.h>
#include <modules/physics/components.h>

namespace vb {
namespace physics {

Vector2 normalize_vector2(Vector2 v){
    float len_sq = v.x * v.x + v.y * v.y;
    if (len_sq > 1e-12f) {
        float inv_len = 1.0f / std::sqrt(len_sq);
        v.x *= inv_len;
        v.y *= inv_len;
    } else {
        v.x = 0.0f;
        v.y = 0.0f;
    }

    return v;
}

module::module(flecs::world &world) {
    world.module<module>();

    world.component<FixedUpdate>();
    world.component<RectCollider>();
    world.component<CollisionEvent>();

    world.system<WishVelocity, Velocity>("ApplyWishVelocity")
        .kind<FixedUpdate>()
        .each([](flecs::iter &it, size_t, WishVelocity &wv, Velocity &v) {
            float dt = it.delta_time();
            const float alpha = 1.0f - std::exp(-15.0f * dt);

            v.linear.x += (wv.linear.x - v.linear.x) * alpha;
            v.linear.y += (wv.linear.y - v.linear.y) * alpha;
        });

    world.system<base::Transform2D, Velocity>("ApplyVelocityToTransform")
        .kind<FixedUpdate>()
        .each([](flecs::iter &it, size_t, base::Transform2D &t, Velocity &v) {
            float dt = it.delta_time();
            t.translation.x += v.linear.x * dt;
            t.translation.y += v.linear.y * dt;
        });

    world.system<base::Transform2D, RectCollider>("DetectCollision")
        .kind<FixedUpdate>()
        .each([](flecs::iter &it, size_t i, base::Transform2D &t, RectCollider &rc) {
            flecs::world w = it.world();
            flecs::entity self = it.entity(i);

            const float shw = rc.size.x * 0.5f;
            const float shh = rc.size.y * 0.5f;
            const float sminx = t.translation.x - shw;
            const float smaxx = t.translation.x + shw;
            const float sminy = t.translation.y - shh;
            const float smaxy = t.translation.y + shh;

            auto q = w.query<base::Transform2D, RectCollider>();
            q.each([&](flecs::iter &it2, size_t j,
                    base::Transform2D &ot, RectCollider &orc) {
                flecs::entity other = it2.entity(j);
                if (self.id() >= other.id()) return;

                const float ohw = orc.size.x * 0.5f;
                const float ohh = orc.size.y * 0.5f;
                const float ominx = ot.translation.x - ohw;
                const float omaxx = ot.translation.x + ohw;
                const float ominy = ot.translation.y - ohh;
                const float omaxy = ot.translation.y + ohh;

                const bool overlap =
                    sminx < omaxx && smaxx > ominx &&
                    sminy < omaxy && smaxy > ominy;

                if (overlap) {
                    CollisionEvent evt{self, other};
                    w.event<CollisionEvent>().ctx(evt).emit();
                }
            });
        });
}

} // namespace physics
} // namespace vb
