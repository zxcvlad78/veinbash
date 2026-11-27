#pragma once
#include <flecs.h>

typedef struct {
  float points;
  float max_points;
} Health;

typedef struct {
  ecs_entity_t killer;
} EventDeath;

typedef struct {
  bool up, down, left, right;
} CharacterControl;

typedef struct {
  float x, y;
} MoveSpeed;

extern ECS_COMPONENT_DECLARE(EventDeath);
extern ECS_COMPONENT_DECLARE(CharacterControl);
extern ECS_COMPONENT_DECLARE(MoveSpeed);

void CharacterModuleImport(ecs_world_t *world);
