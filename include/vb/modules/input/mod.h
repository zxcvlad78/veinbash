#pragma once
#include <flecs.h>

typedef struct {
  bool up, down, left, right;
} CharacterInput;

extern ECS_COMPONENT_DECLARE(CharacterInput);

void InputModuleImport(ecs_world_t *world);
