#pragma once
#include <flecs.h>
#include <raylib.h>

typedef struct {
  Rectangle rect;
} AnimationFrame;

typedef struct {
  const char *id;
  AnimationFrame *frames;
  int frame_count;
  float frame_time;
  bool loop;
} AnimationClip;

typedef struct {
  AnimationClip *clips;
  int clip_count;
} AnimationSet;

typedef struct {
  int clip_idx;
  int frame_idx;
  float timer;
  bool finished;
} Animator;

bool play_animation(ecs_world_t *world, ecs_entity_t e, const char *id);

extern ECS_COMPONENT_DECLARE(AnimationSet);
extern ECS_COMPONENT_DECLARE(Animator);

void AnimationModuleImport(ecs_world_t *world);
