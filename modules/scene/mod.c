#include <modules/scene/mod.h>

ECS_COMPONENT_DECLARE(vbScene);

void vbSceneModuleImport(ecs_world_t *world) {
    ECS_MODULE(world, vbSceneModule);
    ECS_COMPONENT_DEFINE(world, vbScene);
}

bool vbScene_is_valid(const ecs_world_t *world, ecs_entity_t scene) {
    return world != NULL && scene != 0 && ecs_id(vbScene) != 0u && ecs_has(world, scene, vbScene);
}

ecs_entity_t vbScene_create(ecs_world_t *world, const char *name) {
    if (!world || ecs_id(vbScene) == 0) {
        return 0;
    }

    ecs_entity_desc_t desc = {0};
    desc.name = (name && name[0]) ? name : "vbScene";
    const ecs_entity_t scene = ecs_entity_init(world, &desc);
    if (!scene) {
        return 0;
    }

    ecs_set(world, scene, vbScene, { .reserved = 0u });
    return scene;
}

ecs_entity_t vbScene_spawn(ecs_world_t *world, ecs_entity_t scene, const char *name) {
    if (!vbScene_is_valid(world, scene)) {
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

void vbScene_destroy(ecs_world_t *world, ecs_entity_t scene) {
    if (vbScene_is_valid(world, scene)) {
        /* Flecs ChildOf cleanup recursively removes scene-owned child entities. */
        ecs_delete(world, scene);
    }
}
