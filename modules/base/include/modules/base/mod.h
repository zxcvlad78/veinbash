#pragma once
#include <flecs.h>
#include <raylib.h>

typedef struct {
  Vector2 linear;
  float angular;
} Velocity, WishVelocity;

Vector2 normalize_vector2(Vector2 v);

typedef struct {
  Vector2 translation;
  Vector2 scale;
  float rotation;
} Transform2D;

extern ECS_COMPONENT_DECLARE(Velocity);
extern ECS_COMPONENT_DECLARE(WishVelocity);
extern ECS_COMPONENT_DECLARE(Transform2D);

void BaseModuleImport(ecs_world_t *world);
