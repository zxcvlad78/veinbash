#include <raylib.h>
#include <vb/modules/animation/mod.h>
#include <vb/modules/asset_server/mod.h>
#include <vb/modules/base/mod.h>
#include <vb/modules/character/mod.h>
#include <vb/modules/render/mod.h>

int main(int argc, char *argv[]) {
  InitWindow(1280, 720, "veinbash");
  SetTargetFPS(144);

  ecs_world_t *world = ecs_init_w_args(argc, argv);
  ECS_IMPORT(world, BaseModule);
  ECS_IMPORT(world, AnimationModule);
  ECS_IMPORT(world, AssetServerModule);
  ECS_IMPORT(world, RenderModule);
  ECS_IMPORT(world, CharacterModule);

  AssetServerConfig asset_config = asset_server_default_config();
  asset_config.root_directory = "data";
  if (!asset_server_init(world, &asset_config)) {
    TraceLog(LOG_FATAL, "AssetServer initialization failed");
    ecs_fini(world);
    CloseWindow();
    return 1;
  }

  const AssetHandle player_texture = asset_texture_acquire(
      world, "atlas.png", ASSET_PRIORITY_CRITICAL);
  if (!ASSET_HANDLE_IS_VALID(player_texture)) {
    TraceLog(LOG_FATAL, "Could not queue data/atlas.png");
    asset_server_fini(world);
    ecs_fini(world);
    CloseWindow();
    return 1;
  }

  static AnimationFrame idle_frames[2];
  for (int i = 0; i < 2; i++) {
    idle_frames[i] = (AnimationFrame){{i * 16, 0, 16, 32}};
  }

  static AnimationClip clips[2] = {
      {"idle", idle_frames, 2, 1.0f, true},
      {"walk", idle_frames, 2, 10.0f, true}
  };

  const ecs_entity_t entity = ecs_new(world);
  ecs_set(world, entity, Transform2D, {{640.0f, 320.0f}, {2.0f, 2.0f}});
  ecs_set(world, entity, CharacterControl, {0, 0, 0, 0});
  ecs_set(world, entity, MoveSpeed, {150.0f, 150.0f});
  ecs_set(world, entity, Velocity, {0.0f, 0.0f});
  ecs_set(world, entity, WishVelocity, {0.0f, 0.0f});
  ecs_set(world, entity, Sprite, {
      .texture = player_texture,
      .rect = {0.0f, 0.0f, 16.0f, 32.0f}
  });
  ecs_set(world, entity, AnimationSet, {clips, 2});
  ecs_set(world, entity, Animator, {0});

  (void)play_animation(world, entity, "idle");

  while (!WindowShouldClose()) {
    /* GPU object creation is budgeted and always happens on the render thread. */
    (void)asset_server_pump(world, 4u);
    ecs_progress(world, GetFrameTime());

    BeginDrawing();
    ClearBackground(GRAY);
    EndDrawing();
  }

  /* Release GPU assets while the raylib graphics context is still alive. */
  asset_server_fini(world);
  ecs_fini(world);
  CloseWindow();
  return 0;
}
