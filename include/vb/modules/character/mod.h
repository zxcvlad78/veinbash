#pragma once
#include <flecs.h>

typedef struct {
  bool up, down, left, right;
} CharacterControl;

typedef struct {
  float x, y;
} MoveSpeed;

extern ECS_COMPONENT_DECLARE(CharacterControl);
extern ECS_COMPONENT_DECLARE(MoveSpeed);

void CharacterModuleImport(ecs_world_t *world);
