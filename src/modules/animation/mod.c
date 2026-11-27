#include <vb/modules/animation/mod.h>

ECS_COMPONENT_DECLARE(AnimationSet);
ECS_COMPONENT_DECLARE(Animator);

void Animation(ecs_iter_t *it) {
  Animator *a = ecs_field(it, Animator, 0);
  AnimationSet *as = ecs_field(it, AnimationSet, 1);
  float dt = it->delta_time;

  for (int i = 0; i < it->count; i++) {
    if (a[i].finished)
      continue;
    const AnimationClip *c = &as[i].clips[a[i].clip_idx];
    if (!c)
      continue;

    a[i].timer += dt;
    while (a[i].timer >= c->frame_time) {
      a[i].timer -= c->frame_time;
      a[i].frame_idx++;

      if (a[i].frame_idx >= c->frame_count) {
        if (c->loop)
          a[i].frame_idx = 0;
        else {
          a[i].frame_idx = c->frame_count - 1;
          a[i].finished = true;
          break;
        }
      }
    }
  }
}

void AnimationModuleImport(ecs_world_t *world) {
  ECS_MODULE(world, AnimationModule);

  ECS_COMPONENT_DEFINE(world, AnimationSet);
  ECS_COMPONENT_DEFINE(world, Animator);

  ECS_SYSTEM(world, Animation, EcsOnUpdate, Animator, AnimationSet);
}
