#define _POSIX_C_SOURCE 200809L

#include <vb/modules/asset_server/mod.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    typedef CRITICAL_SECTION AssetMutex;
    typedef CONDITION_VARIABLE AssetCondition;
    typedef HANDLE AssetThread;
#else
    #include <pthread.h>
    #include <unistd.h>
    typedef pthread_mutex_t AssetMutex;
    typedef pthread_cond_t AssetCondition;
    typedef pthread_t AssetThread;
#endif

ECS_COMPONENT_DECLARE(AssetServerState);

typedef struct AssetTextureEntry {
    char *path;                 /* Immutable until server shutdown. */
    uint64_t hash;
    uint32_t revision;          /* Cold-path cancellation/version token. */
    uint32_t bundle_owners;     /* Updated only at bundle load/unload. */
    AssetTextureStatus status;
    bool globally_pinned;
    char error[128];
} AssetTextureEntry;

typedef struct AssetJob {
    AssetTextureId id;
    uint32_t revision;
    uint64_t sequence;
    int32_t priority;
} AssetJob;

typedef struct AssetCompletion {
    AssetTextureId id;
    uint32_t revision;
    Image image;
    bool decoded;
    char error[128];
} AssetCompletion;

struct AssetServer {
    char *root_directory;

    /*
     * Fixed-capacity structure-of-arrays:
     * - gpu_textures is the hot render table (dense Texture2D handles only).
     * - entries contains cold path/cache/ownership/status metadata.
     * Neither allocation moves during server lifetime.
     */
    Texture2D *gpu_textures;
    AssetTextureEntry *entries;
    uint32_t texture_count;
    uint32_t texture_capacity;

    /* Open-addressed path -> (AssetTextureId + 1), no deletion/tombstones. */
    uint32_t *path_map;
    uint32_t path_map_capacity;

    /* Priority max-heap, protected by mutex. */
    AssetJob *jobs;
    uint32_t job_count;
    uint32_t job_capacity;
    uint64_t next_job_sequence;

    /* Bounded completion ring. Workers block instead of dropping decoded data. */
    AssetCompletion *completions;
    uint32_t completion_head;
    uint32_t completion_tail;
    uint32_t completion_count;
    uint32_t completion_capacity;

    AssetThread *workers;
    uint32_t worker_count;
    uint32_t active_workers;
    bool stopping;

    AssetMutex mutex;
    AssetCondition condition;
    bool sync_initialized;

    AssetServerStats stats;
};

/* -------------------------------------------------------------------------- */
/* Platform synchronization                                                   */
/* -------------------------------------------------------------------------- */
