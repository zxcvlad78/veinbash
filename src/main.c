#include <raylib.h>
#include <vb/modules/animation/mod.h>
#include <vb/modules/base/mod.h>
#include <vb/modules/character/mod.h>
#include <vb/modules/render/mod.h>

typedef struct  {
  AnimationFrame idle_frames[2];
  AnimationFrame up_frames[2];
  AnimationFrame down_frames[2];
  AnimationFrame left_anim[4];
  AnimationFrame right_anim[4];
  
} CharacterAnim;

int main(int argc, char *argv[]) {
  InitWindow(1280, 720, "veinbash");
  SetTargetFPS(144);

  Texture2D player_texture = LoadTexture("data/atlas.png");

  ecs_world_t *world = ecs_init_w_args(argc, argv);
  ECS_IMPORT(world, BaseModule);
  ECS_IMPORT(world, AnimationModule);
  ECS_IMPORT(world, RenderModule);
  ECS_IMPORT(world, CharacterModule);

  ecs_entity_t entity = ecs_new(world);
  ecs_set(world, entity, Transform2D, {{640.0f, 320.0f}, {2.0f, 2.0f}});
  ecs_set(world, entity, CharacterControl, {0, 0, 0, 0});
  ecs_set(world, entity, MoveSpeed, {150.0f, 150.0f});
  ecs_set(world, entity, Velocity, {0.0f, 0.0f});
  ecs_set(world, entity, WishVelocity, {0.0f, 0.0f});
  ecs_set(world, entity, Sprite,
          {player_texture, {0.0f, 0.0f, 16.0f, 32.0f}});

  while (!WindowShouldClose()) {
    ecs_progress(world, GetFrameTime());
    BeginDrawing();
    ClearBackground(BLACK);
    EndDrawing();
  }

  CloseWindow();
  ecs_fini(world);
  return 0;
}
