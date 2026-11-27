#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <flecs.h>

/* Marker component attached to a scene root entity. */
typedef struct vbScene {
    uint8_t reserved;
} vbScene;

extern ECS_COMPONENT_DECLARE(vbScene);

void vbSceneModuleImport(ecs_world_t *world);

ecs_entity_t vbScene_create  (ecs_world_t *world, const char *name); /* Creates a root entity. Scene assets are attached lazily by asset_scene_texture(). */
ecs_entity_t vbScene_spawn   (ecs_entity_t scene, ecs_world_t *world, const char *name); /* Creates an entity parented to the scene through Flecs ChildOf. */
void         vbScene_destroy (ecs_entity_t scene, ecs_world_t *world); /* Deleting the scene. also deletes its ChildOf hierarchy and scene-owned assets. */
bool         vbScene_is_valid(ecs_entity_t scene, const ecs_world_t *world);
