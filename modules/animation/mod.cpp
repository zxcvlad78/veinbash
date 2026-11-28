#include <modules/animation/mod.h>

#include <cstring>

namespace vb {
namespace animation {

bool play(flecs::world &world, flecs::entity e, const char *id) {
    if (!e.has<Animator, AnimationSet>()) return false;

    Animator &a = e.get_mut<Animator>();
    const AnimationSet &as = e.get<AnimationSet>();

    for (i32 i = 0; i < as.clip_count; i++) {
        if (as.clips[i].id && std::strcmp(as.clips[i].id, id) == 0) {
            a.clip_idx = i;
            a.frame_idx = 0;
            a.timer = 0.0f;
            a.finished = false;
            e.modified<Animator>();
            return true;
        }
    }

    return false;
}

module::module(flecs::world &world) {
    world.module<module>();

    world.component<AnimationSet>();
    world.component<Animator>();

    world.system<Animator, const AnimationSet>("Animate")
        .each([](flecs::iter &it, size_t, Animator &a, const AnimationSet &as) {
            if (a.finished) return;
            if (a.clip_idx < 0 || a.clip_idx >= as.clip_count) return;

            const AnimationClip &c = as.clips[a.clip_idx];
            if (!c.frames || c.frame_count <= 0) return;

            a.timer += it.delta_time();
            while (a.timer >= c.frame_time) {
                a.timer -= c.frame_time;
                a.frame_idx++;

                if (a.frame_idx >= c.frame_count) {
                    if (c.loop) {
                        a.frame_idx = 0;
                    } else {
                        a.frame_idx = c.frame_count - 1;
                        a.finished = true;
                        break;
                    }
                }
            }
        });
}

} // namespace animation
} // namespace vb
