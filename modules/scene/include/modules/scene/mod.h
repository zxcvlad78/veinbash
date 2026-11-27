#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <flecs.h>

#ifdef __cplusplus
#error "veinbash Scene module is a C11 API; compile this header as C"
#endif

/* Marker component attached to a scene root entity. */
typedef struct Scene {
    uint8_t reserved;
} Scene;

extern ECS_COMPONENT_DECLARE(Scene);

void SceneModuleImport(ecs_world_t *world);

/* Creates a root entity. Scene assets are attached lazily by asset_scene_texture(). */
ecs_entity_t scene_create(ecs_world_t *world, const char *name);

/* Creates an entity parented to the scene through Flecs ChildOf. */
ecs_entity_t scene_spawn(ecs_world_t *world,
                         ecs_entity_t scene,
                         const char *name);

/* Deleting the root also deletes its ChildOf hierarchy and scene-owned assets. */
void scene_destroy(ecs_world_t *world, ecs_entity_t scene);

bool scene_is_valid(const ecs_world_t *world, ecs_entity_t scene);
