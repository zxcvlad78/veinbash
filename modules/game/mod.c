#include <modules/game/mod.h>

static void vb_init_window() {
    InitWindow(1280, 720, "veinbash");
    SetTargetFPS(144);
}

void GameModuleImport(ecs_world_t *world) {
    ECS_MODULE(world, GameModule);

    vb_init_window();

    ECS_IMPORT(world, BaseModule);
    ECS_IMPORT(world, AnimationModule);
    ECS_IMPORT(world, vbSceneModule);
    ECS_IMPORT(world, AssetServerModule);
    ECS_IMPORT(world, RenderModule);
    ECS_IMPORT(world, CharacterModule);


    const ecs_entity_t scene = vbScene_create(world, "Scene");
    if (!scene) {
        asset_server_fini(world);
        ecs_fini(world);
        CloseWindow();
        return;
    }

    const AssetTextureId player_atlas = asset_scene_texture(
        world, scene, "atlas.png", ASSET_PRIORITY_CRITICAL);
    if (player_atlas == ASSET_TEXTURE_INVALID) {
        vbScene_destroy(world, scene);
        asset_server_fini(world);
        ecs_fini(world);
        CloseWindow();
        return;
    }

    printf("player_atlas: %i\n", player_atlas);

    static AnimationFrame idle_frames[2];
    for (int i = 0; i < 2; ++i) {
        idle_frames[i] = (AnimationFrame){{ i * 16.0f, 0.0f, 16.0f, 32.0f }};
    }

    static AnimationClip clips[2] = {
        { "idle", idle_frames, 2, 1.0f, true },
        { "walk", idle_frames, 2, 10.0f, true }
    };
    (void)clips;

    const ecs_entity_t player = vbScene_spawn(world, scene, "Player");
    if (!player) {
        vbScene_destroy(world, scene);
        asset_server_fini(world);
        ecs_fini(world);
        CloseWindow();
        return;
    }
    ecs_set(world, player, Transform2D, { .translation = { 640.0f, 320.0f }, .scale = { 2.0f, 2.0f }, .rotation = 0.0f });
    ecs_set(world, player, CharacterControl, { 0, 0, 0, 0 });
    ecs_set(world, player, MoveSpeed, { 150.0f, 150.0f });
    ecs_set(world, player, Velocity, { .linear = { 0.0f, 0.0f }, .angular = 0.0f });
    ecs_set(world, player, WishVelocity, { .linear = { 0.0f, 0.0f }, .angular = 0.0f });
    ecs_set(world, player, Sprite, { .texture = player_atlas, .rect = { 0.0f, 0.0f, 16.0f, 32.0f } });
    ecs_set(world, player, AnimationSet, { clips, 2 });
    ecs_set(world, player, Animator, { 0 });

    (void)play_animation(world, player, "idle");

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){110, 0, 0});

        ecs_progress(world, GetFrameTime());

        EndDrawing();
    }

    vbScene_destroy(world, scene);

    asset_server_fini(world);
    ecs_fini(world);
    CloseWindow();
}
