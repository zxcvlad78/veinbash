#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <flecs.h>
#include <raylib.h>

#ifdef __cplusplus
#error "veinbash AssetServer is a C11 API; compile this header as C"
#endif

typedef struct AssetServer AssetServer;

typedef uint32_t AssetTextureId;

#define ASSET_TEXTURE_INVALID ((AssetTextureId)0u)

typedef enum AssetTextureStatus {
    ASSET_TEXTURE_EMPTY = 0,
    ASSET_TEXTURE_QUEUED,
    ASSET_TEXTURE_READY,
    ASSET_TEXTURE_RELOADING,
    ASSET_TEXTURE_FAILED
} AssetTextureStatus;

typedef enum AssetPriority {
    ASSET_PRIORITY_LOW = 0,
    ASSET_PRIORITY_NORMAL = 1,
    ASSET_PRIORITY_HIGH = 2,
    ASSET_PRIORITY_CRITICAL = 3
} AssetPriority;

typedef struct AssetServerConfig {
    /* Paths passed to the server are relative to this directory. */
    const char *root_directory;

    /* Fixed capacity: texture IDs are stable and never recycled. 0 = 8192. */
    uint32_t max_textures;

    /* 0 = auto (logical CPUs - 1, clamped to 1..8). */
    uint32_t worker_count;

    /* Bounded work queue. 0 = 2048. */
    uint32_t job_queue_capacity;

    /* Bounded decoded-image queue. Workers wait when it is full. 0 = 256. */
    uint32_t completion_queue_capacity;
} AssetServerConfig;

typedef struct AssetServerStats {
    uint64_t texture_requests;
    uint64_t cache_hits;
    uint64_t decode_successes;
    uint64_t decode_failures;
    uint64_t gpu_uploads;
    uint64_t gpu_upload_failures;
    uint64_t reload_requests;
    uint32_t texture_count;
    uint32_t queued_jobs;
    uint32_t active_workers;
    uint32_t pending_completions;
    uint64_t resident_texture_bytes;
} AssetServerStats;

/* A bundle is a cold-path owner. Sprite components never retain/release assets. */
typedef struct AssetBundle {
    AssetServer *server;
    AssetTextureId *textures;
    uint32_t count;
    uint32_t capacity;
} AssetBundle;

typedef struct AssetServerState {
    AssetServer *implementation;
} AssetServerState;

extern ECS_COMPONENT_DECLARE(AssetServerState);

/* Register this module before asset_server_init(). */
void AssetServerModuleImport(ecs_world_t *world);

AssetServerConfig asset_server_default_config(void);
bool asset_server_init(ecs_world_t *world, const AssetServerConfig *config);
void asset_server_fini(ecs_world_t *world);

/* Obtain the server once per render batch, then use asset_texture_fast(). */
const AssetServer *asset_server_get(const ecs_world_t *world);

/* Bundle lifetime API: all functions are called on the world/render thread. */
bool asset_bundle_init(ecs_world_t *world, AssetBundle *bundle);
AssetTextureId asset_bundle_texture(AssetBundle *bundle,
                                    const char *relative_path,
                                    AssetPriority priority);
void asset_bundle_unload(AssetBundle *bundle);

/* Globally pinned resource, released only by asset_server_fini(). */
AssetTextureId asset_server_texture_global(ecs_world_t *world,
                                           const char *relative_path,
                                           AssetPriority priority);

/* Explicit reload. A previously ready texture remains usable until replacement. */
bool asset_texture_reload(ecs_world_t *world,
                          AssetTextureId texture,
                          AssetPriority priority);

/*
 * Process CPU-decoded images and create GPU textures. Call on the raylib thread
 * once per frame, before the ECS render system. max_completions == 0 is unlimited.
 */
uint32_t asset_server_pump(ecs_world_t *world, uint32_t max_completions);

/*
 * Render hot path: direct table indexing, no hash lookup, lock, allocation,
 * atomic operation, reference count, or generation check. Returns NULL until
 * the first GPU upload succeeds. Returns the old texture during a reload.
 */
const Texture2D *asset_texture_fast(const AssetServer *server,
                                    AssetTextureId texture);

AssetTextureStatus asset_texture_status(const AssetServer *server,
                                        AssetTextureId texture);
bool asset_texture_error(const AssetServer *server,
                         AssetTextureId texture,
                         char *destination,
                         uint32_t destination_size);
void asset_server_get_stats(const AssetServer *server,
                             AssetServerStats *out_stats);

/* Optional maintenance: no-op for owned/pinned entries. Call on render thread. */
uint32_t asset_server_collect_unused(AssetServer *server);
