#pragma once
#include <flecs.h>
#include <raylib.h>
#include <vb/modules/asset_server/mod.h>

typedef struct {
  AssetTextureId texture;
  Rectangle rect;
} Sprite;

extern ECS_COMPONENT_DECLARE(Sprite);

void RenderModuleImport(ecs_world_t *world);
