#pragma once
#include <flecs.h>

/*
typedef struct {
  double x, y;
} Position, Velocity;

extern ECS_COMPONENT_DECLARE(Position);
*/

void RenderModuleImport(ecs_world_t *world);
