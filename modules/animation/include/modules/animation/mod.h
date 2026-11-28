#pragma once

#include <flecs.h>
#include <raylib.h>

namespace vb {
namespace animation {

struct AnimationFrame {
    Rectangle rect{};
};

struct AnimationClip {
    const char *id = nullptr;
    AnimationFrame *frames = nullptr;
    i32 frame_count = 0;
    f32 frame_time = 0.0f;
    bool loop = false;
};

struct AnimationSet {
    AnimationClip *clips = nullptr;
    i32 clip_count = 0;
};

struct Animator {
    i32 clip_idx = 0;
    i32 frame_idx = 0;
    f32 timer = 0.0f;
    bool finished = false;
};

bool play(flecs::world &world, flecs::entity e, const char *id);

struct module {
    module(flecs::world &world);
};

} // namespace animation
} // namespace vb
