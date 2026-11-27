#pragma once
#include <flecs.h>
#include <raylib.h>

typedef struct {

} ProjectileWeapon;

extern ECS_COMPONENT_DECLARE(Sprite);

void WeaponModuleImport(ecs_world_t *world);
