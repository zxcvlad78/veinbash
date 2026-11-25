#include <vb/modules/base/mod.h>
#include <vb/modules/render/mod.h>

#include <raylib.h>

// ECS_COMPONENT_DECLARE(Position);

void Render(ecs_iter_t *it) {
  Position *p = ecs_field(it, Position, 0);

  for (int i = 0; i < it->count; i++) {
    DrawCircle((int)p[i].x, (int)p[i].y, 10.0f, RED);
  }
}

void RenderModuleImport(ecs_world_t *world) {
  ECS_MODULE(world, RenderModule);

  // ECS_COMPONENT_DEFINE(world, Velocity);

  ECS_SYSTEM(world, Render, EcsOnUpdate, Position);
}
