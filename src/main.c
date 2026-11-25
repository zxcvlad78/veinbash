#include <raylib.h>
#include <vb/modules/base/mod.h>
#include <vb/modules/input/mod.h>
#include <vb/modules/render/mod.h>

int main(int argc, char *argv[]) {
  InitWindow(1280, 720, "veinbash");
  SetTargetFPS(144);

  ecs_world_t *world = ecs_init_w_args(argc, argv);
  ECS_IMPORT(world, InputModule);
  ECS_IMPORT(world, BaseModule);
  ECS_IMPORT(world, RenderModule);

  ecs_entity_t entity = ecs_new(world);
  ecs_set(world, entity, Position, {640.0f, 320.0f});
  ecs_set(world, entity, Velocity, {10.0f, 10.0f});

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
