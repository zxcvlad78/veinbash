#pragma once
#include <flecs.h>

typedef struct {
  double x, y;
} Position, Velocity, WishVelocity;

extern ECS_COMPONENT_DECLARE(Position);
extern ECS_COMPONENT_DECLARE(Velocity);
extern ECS_COMPONENT_DECLARE(WishVelocity);

void BaseModuleImport(ecs_world_t *world);
