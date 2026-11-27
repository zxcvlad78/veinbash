#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <flecs.h>
#include <raylib.h>

typedef struct AssetServer AssetServer;

typedef struct {
    uint32_t index;
    uint32_t generation;
} AssetHandle;

typedef enum {
    ASSET_STATUS_INVALID = 0,
    ASSET_STATUS_QUEUED,
    ASSET_STATUS_LOADING,
    ASSET_STATUS_CPU_READY,
    ASSET_STATUS_READY,
    ASSET_STATUS_RELOADING,
    ASSET_STATUS_FAILED
} AssetStatus;

typedef enum {
    ASSET_PRIORITY_LOW = 0,
    ASSET_PRIORITY_NORMAL = 1,
    ASSET_PRIORITY_HIGH = 2,
    ASSET_PRIORITY_CRITICAL = 3
} AssetPriority;

typedef struct {
    const char *root_directory;
    uint32_t worker_count;
    uint32_t queue_capacity;
} AssetServerConfig;

typedef struct {
    uint64_t requests;
    uint64_t cache_hits;
    uint64_t successful_loads;
    uint64_t failed_loads;
    uint64_t reloads;
    uint32_t cached_textures;
    uint32_t queued_jobs;
    uint32_t active_workers;
    uint64_t texture_bytes;
} AssetServerStats;

typedef struct {
    AssetServer *implementation;
} AssetServerState;

extern ECS_COMPONENT_DECLARE(AssetServerState);

void AssetServerModuleImport(ecs_world_t *world);
AssetServerConfig asset_server_default_config(void);
bool asset_server_init(ecs_world_t *world, const AssetServerConfig *config);
void asset_server_fini(ecs_world_t *world);

AssetHandle asset_texture_acquire(ecs_world_t *world, const char *relative_path, AssetPriority priority);
bool asset_retain(ecs_world_t *world, AssetHandle handle);
void asset_release(ecs_world_t *world, AssetHandle handle);
bool asset_texture_reload(ecs_world_t *world, AssetHandle handle, AssetPriority priority);
uint32_t asset_server_pump(ecs_world_t *world, uint32_t max_uploads);
const Texture2D *asset_texture_resolve(const AssetServer *server, AssetHandle handle);
const Texture2D *asset_texture_get(ecs_world_t *world, AssetHandle handle);
AssetStatus asset_status(ecs_world_t *world, AssetHandle handle);
bool asset_error_copy(ecs_world_t *world, AssetHandle handle, char *destination, size_t destination_size);
void asset_server_get_stats(ecs_world_t *world, AssetServerStats *out_stats);
uint32_t asset_server_collect_unused(ecs_world_t *world);

#define ASSET_HANDLE_INVALID ((AssetHandle){ UINT32_MAX, 0u })
#define ASSET_HANDLE_IS_VALID(h) ((h).index != UINT32_MAX && (h).generation != 0u)
