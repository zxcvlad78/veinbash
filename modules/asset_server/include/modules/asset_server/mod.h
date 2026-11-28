#pragma once

#include <flecs.h>
#include <raylib.h>

namespace vb {
namespace asset_server {

using AssetTextureId = u64;
constexpr AssetTextureId ASSET_TEXTURE_INVALID = 0;

enum AssetPriority : u32 {
    ASSET_PRIORITY_LOW = 0,
    ASSET_PRIORITY_NORMAL = 1,
    ASSET_PRIORITY_HIGH = 2,
    ASSET_PRIORITY_CRITICAL = 3,
};

struct AssetServer {
    int dummy;
};

AssetServer *get(flecs::world world);
const Texture2D *texture_fast(const AssetServer *server, AssetTextureId id);
AssetTextureId scene_texture(flecs::world world, flecs::entity scene, const char *path, AssetPriority priority);
void fini(flecs::world world);

struct module {
    module(flecs::world &world);
};

} // namespace asset_server
} // namespace vb
