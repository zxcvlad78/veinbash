#include <stdio.h>
#include <vb/modules/base/mod.h>

ECS_COMPONENT_DECLARE(Position);
ECS_COMPONENT_DECLARE(Velocity);
ECS_COMPONENT_DECLARE(WishVelocity);

float lerp_f(float a, float b, float t) { return a + t * (b - a); }

void WishMove(ecs_iter_t *it) {
  WishVelocity *wv = ecs_field(it, WishVelocity, 0);
  Velocity *v = ecs_field(it, Velocity, 1);

  for (int i = 0; i < it->count; i++) {
    v[i].x = lerp_f(v[i].x, wv[i].x, 1.0f * it->delta_time);
    v[i].y = lerp_f(v[i].y, wv[i].y, 1.0f * it->delta_time);
  }
}

void Move(ecs_iter_t *it) {
  Position *p = ecs_field(it, Position, 0);
  Velocity *v = ecs_field(it, Velocity, 1);

  for (int i = 0; i < it->count; i++) {
    p[i].x += v[i].x * it->delta_time;
    p[i].y += v[i].y * it->delta_time;
  }
}

void BaseModuleImport(ecs_world_t *world) {
  ECS_MODULE(world, BaseModule);
  printf("hello BaseModule!!\n");

  ECS_COMPONENT_DEFINE(world, Position);
  ECS_COMPONENT_DEFINE(world, Velocity);
  ECS_COMPONENT_DEFINE(world, WishVelocity);

  ECS_SYSTEM(world, WishMove, EcsOnUpdate, WishVelocity, Velocity);
  ECS_SYSTEM(world, Move, EcsOnUpdate, Position, Velocity);
}
