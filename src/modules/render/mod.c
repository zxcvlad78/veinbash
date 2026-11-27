#include <vb/modules/animation/mod.h>
#include <vb/modules/asset_server/mod.h>
#include <vb/modules/base/mod.h>
#include <vb/modules/render/mod.h>

#include <raylib.h>

ECS_COMPONENT_DECLARE(Sprite);

static void SpriteAnimation(ecs_iter_t *it) {
    Sprite *sprites = ecs_field(it, Sprite, 0);
    Animator *animators = ecs_field(it, Animator, 1);
    AnimationSet *sets = ecs_field(it, AnimationSet, 2);

    for (int32_t i = 0; i < it->count; ++i) {
        const int32_t clip_index = animators[i].clip_idx;
        if (clip_index < 0 || clip_index >= sets[i].clip_count) {
            continue;
        }

        const AnimationClip *clip = &sets[i].clips[clip_index];
        if (!clip->frames || clip->frame_count <= 0 ||
            animators[i].frame_idx < 0 ||
            animators[i].frame_idx >= clip->frame_count) {
            continue;
        }

        sprites[i].rect = clip->frames[animators[i].frame_idx].rect;
    }
}

static void DrawSprite(ecs_iter_t *it) {
    const Transform2D *transforms = ecs_field(it, Transform2D, 0);
    const Sprite *sprites = ecs_field(it, Sprite, 1);
    const AssetServer *assets = asset_server_get(it->world);

    for (int32_t i = 0; i < it->count; ++i) {
        const Texture2D *texture = asset_texture_fast(assets, sprites[i].texture);
        if (!texture) {
            continue; 
        }

        const Rectangle destination = {
            transforms[i].translation.x,
            transforms[i].translation.y,
            sprites[i].rect.width * transforms[i].scale.x,
            sprites[i].rect.height * transforms[i].scale.y
        };
        const Vector2 origin = { 0.0f, 0.0f };

        DrawTexturePro(*texture, sprites[i].rect, destination, origin,
                       transforms[i].rotation, WHITE);
    }
}

void RenderModuleImport(ecs_world_t *world) {
    ECS_MODULE(world, RenderModule);

    ECS_COMPONENT_DEFINE(world, Sprite);

    ECS_SYSTEM(world, SpriteAnimation,
               EcsOnUpdate, Sprite, [in] Animator, [in] AnimationSet);
    ECS_SYSTEM(world, DrawSprite,
               EcsPostUpdate, [in] Transform2D, [in] Sprite);
}
