#include <modules/scene/mod.h>

ECS_COMPONENT_DECLARE(Scene);

void SceneModuleImport(ecs_world_t *world) {
    ECS_MODULE(world, SceneModule);
    ECS_COMPONENT_DEFINE(world, Scene);
}

bool scene_is_valid(const ecs_world_t *world, ecs_entity_t scene) {
    return world != NULL && scene != 0 && ecs_id(Scene) != 0u &&
           ecs_has(world, scene, Scene);
}

ecs_entity_t scene_create(ecs_world_t *world, const char *name) {
    if (!world || ecs_id(Scene) == 0) {
        return 0;
    }

    ecs_entity_desc_t desc = {0};
    desc.name = (name && name[0]) ? name : "Scene";
    const ecs_entity_t scene = ecs_entity_init(world, &desc);
    if (!scene) {
        return 0;
    }

    ecs_set(world, scene, Scene, { .reserved = 0u });
    return scene;
}

ecs_entity_t scene_spawn(ecs_world_t *world,
                         ecs_entity_t scene,
                         const char *name) {
    if (!scene_is_valid(world, scene)) {
        return 0;
    }

    ecs_entity_desc_t desc = {0};
    desc.name = (name && name[0]) ? name : NULL;
    const ecs_entity_t entity = ecs_entity_init(world, &desc);
    if (!entity) {
        return 0;
    }

    ecs_add_pair(world, entity, EcsChildOf, scene);
    return entity;
}

void scene_destroy(ecs_world_t *world, ecs_entity_t scene) {
    if (scene_is_valid(world, scene)) {
        /* Flecs ChildOf cleanup recursively removes scene-owned child entities. */
        ecs_delete(world, scene);
    }
}
