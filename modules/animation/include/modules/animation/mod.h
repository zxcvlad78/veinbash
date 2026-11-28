#pragma once
#include <flecs.h>
#include <raylib.h>

namepsace vb {
namespace animation {

struct AnimationFrame {
    Rectangle rect;
};

struct AnimationClip {
    const char *id;
    AnimationFrame *frames;
    i32 frame_count;
    f32 frame_time;
    bool loop;
};

struct AnimationSet {
    AnimationClip *clips;
    i32 clip_count;
};

struct Animator {
    i32 clip_idx;
    i32 frame_idx;
    f32 timer;
    bool finished;
};

bool Animator::play_animation(ecs_world_t *world, const char *id);

}}
