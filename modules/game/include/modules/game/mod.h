#pragma once
#include <flecs.h>
#include <raylib.h>
#include <modules/base/mod.h>
#include <modules/animation/mod.h>
#include <modules/asset_server/mod.h>
#include <modules/character/mod.h>
#include <modules/game/mod.h>
#include <modules/render/mod.h>
#include <modules/scene/mod.h>
#include <modules/weapon/mod.h>
void GameModuleImport(ecs_world_t *world);
