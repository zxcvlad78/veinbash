#include <math.h>
#include <modules/base/mod.h>

ECS_COMPONENT_DECLARE(Transform2D);
ECS_COMPONENT_DECLARE(Velocity);
ECS_COMPONENT_DECLARE(WishVelocity);

float lerp_f(float a, float b, float t) { return a + t * (b - a); }

Vector2 normalize_vector2(Vector2 v) {
  float len_sq = v.x * v.x + v.y * v.y;

  if (len_sq > 1e-12f) {
    float inv_len = 1.0f / sqrtf(len_sq);
    v.x *= inv_len;
    v.y *= inv_len;
  } else {
    v.x = 0.0f;
    v.y = 0.0f;
  }

  return v;
}

void WishMove(ecs_iter_t *it) {
  WishVelocity *wv = ecs_field(it, WishVelocity, 0);
  Velocity *v = ecs_field(it, Velocity, 1);

  for (int i = 0; i < it->count; i++) {
    v[i].linear.x =
        lerp_f(v[i].linear.x, wv[i].linear.x, 15.0f * it->delta_time);
    v[i].linear.y =
        lerp_f(v[i].linear.y, wv[i].linear.y, 15.0f * it->delta_time);
  }
}

void Move(ecs_iter_t *it) {
  Transform2D *t = ecs_field(it, Transform2D, 0);
  Velocity *v = ecs_field(it, Velocity, 1);

  for (int i = 0; i < it->count; i++) {
    t[i].translation.x += v[i].linear.x * it->delta_time;
    t[i].translation.y += v[i].linear.y * it->delta_time;
  }
}

void BaseModuleImport(ecs_world_t *world) {
  ECS_MODULE(world, BaseModule);

  ECS_COMPONENT_DEFINE(world, Transform2D);
  ECS_COMPONENT_DEFINE(world, Velocity);
  ECS_COMPONENT_DEFINE(world, WishVelocity);

  ECS_SYSTEM(world, WishMove, EcsOnUpdate, WishVelocity, Velocity);
  ECS_SYSTEM(world, Move, EcsOnUpdate, Transform2D, Velocity);
}
