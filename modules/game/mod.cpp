#include <modules/game/mod.h>

#include <modules/animation/mod.h>
#include <modules/asset_server/mod.h>
#include <modules/base/mod.h>
#include <modules/character/mod.h>
#include <modules/render/mod.h>
#include <modules/scene/mod.h>
#include <modules/weapon/mod.h>

#include <raylib.h>
#include <cstdio>

namespace vb {
namespace game {

static void init_window() {
    InitWindow(1280, 720, "veinbash");
    SetTargetFPS(144);
}

module::module(flecs::world &world) {
    world.module<module>();

    init_window();

    world.import<base::module>();
    world.import<animation::module>();
    world.import<scene::module>();
    world.import<asset_server::module>();
    world.import<render::module>();
    world.import<character::module>();
    world.import<weapon::module>();

    flecs::entity scene_entity = scene::create(world, "Scene");
    if (!scene_entity.is_alive()) {
        asset_server::fini(world);
        CloseWindow();
        return;
    }

    const asset_server::AssetTextureId player_atlas = asset_server::scene_texture(
        world, scene_entity, "atlas.png", asset_server::ASSET_PRIORITY_CRITICAL);
    if (player_atlas == asset_server::ASSET_TEXTURE_INVALID) {
        scene::destroy(world, scene_entity);
        asset_server::fini(world);
        CloseWindow();
        return;
    }

    static animation::AnimationFrame idle_frames[2];
    for (int i = 0; i < 2; ++i) {
        idle_frames[i] = animation::AnimationFrame{{i * 16.0f, 0.0f, 16.0f, 32.0f}};
    }

    static animation::AnimationClip clips[2] = {
        {"idle", idle_frames, 2, 1.0f, true},
        {"walk", idle_frames, 2, 10.0f, true},
    };

    flecs::entity player = scene::spawn(world, scene_entity, "Player");
    if (!player.is_alive()) {
        scene::destroy(world, scene_entity);
        asset_server::fini(world);
        CloseWindow();
        return;
    }

    player.set<base::Transform2D>({{640.0f, 320.0f}, {2.0f, 2.0f}, 0.0f});
    player.set<character::CharacterControl>({});
    player.set<character::MoveSpeed>({150.0f, 150.0f});
    player.set<base::Velocity>({{0.0f, 0.0f}, 0.0f});
    player.set<base::WishVelocity>({{0.0f, 0.0f}, 0.0f});
    player.set<render::Sprite>({player_atlas, {0.0f, 0.0f, 16.0f, 32.0f}});
    player.set<animation::AnimationSet>({clips, 2});
    player.set<animation::Animator>({0, 0, 0.0f, false});

    animation::play(world, player, "idle");

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(Color{110, 0, 0, 255});

        world.progress(GetFrameTime());

        EndDrawing();
    }

    scene::destroy(world, scene_entity);
    asset_server::fini(world);
    CloseWindow();
}

} // namespace game
} // namespace vb
