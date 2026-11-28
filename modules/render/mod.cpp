#include <modules/render/mod.h>
#include <modules/animation/mod.h>
#include <modules/asset_server/mod.h>
#include <modules/base/mod.h>

#include <raylib.h>

namespace vb {
namespace render {

module::module(flecs::world &world) {
    world.module<module>();

    world.component<Sprite>();

    world.system<Sprite, animation::Animator, const animation::AnimationSet>("SpriteAnimation")
        .each([](Sprite &sprite, animation::Animator &anim, const animation::AnimationSet &set) {
            const i32 clip_index = anim.clip_idx;
            if (clip_index < 0 || clip_index >= set.clip_count) return;

            const animation::AnimationClip &clip = set.clips[clip_index];
            if (!clip.frames || clip.frame_count <= 0 || anim.frame_idx < 0 || anim.frame_idx >= clip.frame_count) {
                return;
            }

            sprite.rect = clip.frames[anim.frame_idx].rect;
        });

    world.system<const base::Transform2D, const Sprite>("DrawSprite")
        .kind(flecs::PostUpdate)
        .each([](flecs::iter &it, size_t, const base::Transform2D &t, const Sprite &s) {
            asset_server::AssetServer *assets = asset_server::get(it.world());
            const Texture2D *texture = asset_server::texture_fast(assets, s.texture);
            if (!texture) return;

            Rectangle dest{
                t.translation.x,
                t.translation.y,
                s.rect.width * t.scale.x,
                s.rect.height * t.scale.y
            };
            Vector2 origin{0.0f, 0.0f};
            DrawTexturePro(*texture, s.rect, dest, origin, t.rotation, WHITE);
        });
}

} // namespace render
} // namespace vb
