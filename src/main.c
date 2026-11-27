#include <modules/game/mod.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    ecs_world_t *world = ecs_init_w_args(argc, argv);
    ECS_IMPORT(world, GameModule);


    const ecs_entity_t player_scene = scene_create(world, "PlayerScene");
    if (!player_scene) {
        TraceLog(LOG_FATAL, "Scene creation failed");
        asset_server_fini(world);
        ecs_fini(world);
        CloseWindow();
        return 1;
    }

    const AssetTextureId player_atlas = asset_scene_texture(
        world, player_scene, "atlas.png", ASSET_PRIORITY_CRITICAL);
    if (player_atlas == ASSET_TEXTURE_INVALID) {
        TraceLog(LOG_FATAL, "Could not request data/atlas.png");
        scene_destroy(world, player_scene);
        asset_server_fini(world);
        ecs_fini(world);
        CloseWindow();
        return 1;
    }

    static AnimationFrame idle_frames[2];
    for (int i = 0; i < 2; ++i) {
        idle_frames[i] = (AnimationFrame){{ i * 16.0f, 0.0f, 16.0f, 32.0f }};
    }

    static AnimationClip clips[2] = {
        { "idle", idle_frames, 2, 1.0f, true },
        { "walk", idle_frames, 2, 10.0f, true }
    };
    (void)clips;

    const ecs_entity_t player = scene_spawn(world, player_scene, "Player");
    if (!player) {
        TraceLog(LOG_FATAL, "Could not create player entity in PlayerScene");
        scene_destroy(world, player_scene);
        asset_server_fini(world);
        ecs_fini(world);
        CloseWindow();
        return 1;
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

        (void)asset_server_pump(world, 4u);
        ecs_progress(world, GetFrameTime());

        EndDrawing();
    }

    scene_destroy(world, player_scene);

    asset_server_fini(world);
    ecs_fini(world);
    CloseWindow();
    return 0;
}
