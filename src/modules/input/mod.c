#include <vb/modules/base/mod.h>
#include <vb/modules/input/mod.h>

#include <raylib.h>

ECS_COMPONENT_DECLARE(CharacterInput);

void Input(ecs_iter_t *it) {
  CharacterInput *input = ecs_field(it, CharacterInput, 0);

  for (int i = 0; i < it->count; i++) {
    input[i].up = IsKeyDown(KEY_W);
    input[i].down = IsKeyDown(KEY_S);
    input[i].left = IsKeyDown(KEY_A);
    input[i].right = IsKeyDown(KEY_D);
  }
}

void InputModuleImport(ecs_world_t *world) {
  ECS_MODULE(world, RenderModule);

  ECS_COMPONENT_DEFINE(world, CharacterInput);

  ECS_SYSTEM(world, Input, EcsOnUpdate, CharacterInput);
}
