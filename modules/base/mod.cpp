#include <modules/base/mod.h>

#include <cmath>

namespace vb {
namespace base {

static float lerp_f(float a, float b, float t) {
    return a + t * (b - a);
}

Vector2 normalize_vector2(Vector2 v) {
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

    world.component<Transform2D>();
    world.component<Velocity>();
    world.component<WishVelocity>();

    world.system<WishVelocity, Velocity>("WishMove")
        .each([](flecs::iter &it, size_t, WishVelocity &wv, Velocity &v) {
            float dt = it.delta_time();
            v.linear.x = lerp_f(v.linear.x, wv.linear.x, 15.0f * dt);
            v.linear.y = lerp_f(v.linear.y, wv.linear.y, 15.0f * dt);
        });

    world.system<Transform2D, Velocity>("Move")
        .each([](flecs::iter &it, size_t, Transform2D &t, Velocity &v) {
            float dt = it.delta_time();
            t.translation.x += v.linear.x * dt;
            t.translation.y += v.linear.y * dt;
        });
}

} // namespace base
} // namespace vb
