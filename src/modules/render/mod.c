#include <vb/modules/animation/mod.h>
#include <vb/modules/base/mod.h>
#include <vb/modules/render/mod.h>

#include <raylib.h>

ECS_COMPONENT_DECLARE(Sprite);

void SpriteAnimation(ecs_iter_t *it) {
  Sprite *s = ecs_field(it, Sprite, 0);
  Animator *a = ecs_field(it, Animator, 1);
  AnimationSet *as = ecs_field(it, AnimationSet, 2);

  for (int i = 0; i < it->count; i++) {
    const AnimationClip *clip = &as[i].clips[a[i].clip_idx];
    if (!clip)
      continue;

    s[i].rect = clip->frames[a[i].frame_idx].rect;
  }
}

void DrawSprite(ecs_iter_t *it) {
  Transform2D *t = ecs_field(it, Transform2D, 0);
  Sprite *s = ecs_field(it, Sprite, 1);

  for (int i = 0; i < it->count; i++) {
    Rectangle dest = {t[i].translation.x, t[i].translation.y,
                      s[i].rect.width * t[i].scale.x,
                      s[i].rect.height * t[i].scale.y};

    Vector2 origin = {0.0f, 0.0f};
    DrawTexturePro(s[i].texture, s[i].rect, dest, origin, t[i].rotation, WHITE);
  }
}

void RenderModuleImport(ecs_world_t *world) {
  ECS_MODULE(world, RenderModule);

  ECS_COMPONENT_DEFINE(world, Sprite);

  ECS_SYSTEM(world, SpriteAnimation, EcsOnUpdate, Sprite, Animator,
             AnimationSet);
  ECS_SYSTEM(world, DrawSprite, EcsOnUpdate, Transform2D, Sprite);
}
