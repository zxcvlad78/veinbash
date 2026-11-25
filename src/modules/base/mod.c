#include <stdio.h>
#include <vb/modules/base/mod.h>

ECS_COMPONENT_DECLARE(Position);
ECS_COMPONENT_DECLARE(Velocity);

void Move(ecs_iter_t *it) {
  Position *p = ecs_field(it, Position, 0);
  Position *v = ecs_field(it, Velocity, 1);

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

  ECS_SYSTEM(world, Move, EcsOnUpdate, Position, Velocity);
}
