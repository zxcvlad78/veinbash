#include <modules/asset_server/mod.h>

#include <cstdio>

namespace vb {
namespace asset_server {

struct TextureEntry {
    AssetTextureId id;
    Texture2D tex;
    bool loaded;
};

static constexpr u32 MAX_TEXTURES = 256;
static TextureEntry g_textures[MAX_TEXTURES];
static u32 g_texture_count = 0;
static AssetTextureId g_next_id = 1;
static AssetServer g_server{0};

AssetServer *get(flecs::world /*world*/) {
    return &g_server;
}

const Texture2D *texture_fast(const AssetServer * /*server*/, AssetTextureId id) {
    for (u32 i = 0; i < g_texture_count; ++i) {
        if (g_textures[i].id == id && g_textures[i].loaded) {
            return &g_textures[i].tex;
        }
    }
    return nullptr;
}

AssetTextureId scene_texture(flecs::world /*world*/, flecs::entity /*scene*/, const char *path, AssetPriority /*priority*/) {
    if (!path || !path[0]) return ASSET_TEXTURE_INVALID;
    if (g_texture_count >= MAX_TEXTURES) return ASSET_TEXTURE_INVALID;

    char full[512];
    std::snprintf(full, sizeof(full), "data/%s", path);

    Texture2D tex = LoadTexture(full);
    if (tex.id == 0) return ASSET_TEXTURE_INVALID;

    AssetTextureId id = g_next_id++;
    g_textures[g_texture_count].id = id;
    g_textures[g_texture_count].tex = tex;
    g_textures[g_texture_count].loaded = true;
    g_texture_count++;
    return id;
}

void fini(flecs::world /*world*/) {
    for (u32 i = 0; i < g_texture_count; ++i) {
        if (g_textures[i].loaded) {
            UnloadTexture(g_textures[i].tex);
            g_textures[i].loaded = false;
        }
    }
    g_texture_count = 0;
    g_next_id = 1;
}

module::module(flecs::world &world) {
    world.module<module>();
}

} // namespace asset_server
} // namespace vb
