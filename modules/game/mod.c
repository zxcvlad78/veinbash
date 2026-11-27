#include <modules/game/mod.h>

static void vb_init_window() {
    InitWindow(1280, 720, "veinbash");
    SetTargetFPS(144);
}

void GameModuleImport(ecs_world_t *world) {
    ECS_MODULE(world, GameModule);

    ECS_IMPORT(world, BaseModule);
    ECS_IMPORT(world, AnimationModule);
    ECS_IMPORT(world, SceneModule);
    ECS_IMPORT(world, AssetServerModule);
    ECS_IMPORT(world, RenderModule);
    ECS_IMPORT(world, CharacterModule);

    vb_init_window();

}
