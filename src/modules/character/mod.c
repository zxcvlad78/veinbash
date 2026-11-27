#include <vb/modules/base/mod.h>
#include <vb/modules/character/mod.h>

#include <raylib.h>

ECS_COMPONENT_DECLARE(CharacterControl);
ECS_COMPONENT_DECLARE(MoveSpeed);

void Input(ecs_iter_t *it) {
  CharacterControl *cc = ecs_field(it, CharacterControl, 0);

  for (int i = 0; i < it->count; i++) {
    cc[i].up = IsKeyDown(KEY_W);
    cc[i].down = IsKeyDown(KEY_S);
    cc[i].left = IsKeyDown(KEY_A);
    cc[i].right = IsKeyDown(KEY_D);
  }
}

void ApplyInputToWishVelocity(ecs_iter_t *it) {
  CharacterControl *cc = ecs_field(it, CharacterControl, 0);
  WishVelocity *wv = ecs_field(it, WishVelocity, 1);
  MoveSpeed *ms = ecs_field(it, MoveSpeed, 2);

  for (int i = 0; i < it->count; i++) {
    wv[i].linear.x = 0.0f;
    wv[i].linear.y = 0.0f;
    wv[i].linear.x += ((float)cc[i].right);
    wv[i].linear.x -= ((float)cc[i].left);
    wv[i].linear.y += ((float)cc[i].down);
    wv[i].linear.y -= ((float)cc[i].up);

    wv[i].linear = normalize_vector2(wv[i].linear);
    wv[i].linear.x *= ms[i].x;
    wv[i].linear.y *= ms[i].y;
  }
}

void CharacterModuleImport(ecs_world_t *world) {
  ECS_MODULE(world, CharacterModule);

  ECS_COMPONENT_DEFINE(world, CharacterControl);
  ECS_COMPONENT_DEFINE(world, MoveSpeed);

  ECS_TAG(world, EventDeath);

  ECS_SYSTEM(world, Input, EcsOnUpdate, CharacterControl);
  ECS_SYSTEM(world, ApplyInputToWishVelocity, EcsOnUpdate, CharacterControl,
             WishVelocity, MoveSpeed);
}
