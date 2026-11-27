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
