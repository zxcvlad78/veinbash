#pragma once
#include <flecs.h>
#include <raylib.h>

typedef struct {
  u64 texture;
  Rectangle rect;
} Sprite;

extern ECS_COMPONENT_DECLARE(Sprite);

void RenderModuleImport(ecs_world_t *world);
