#define _POSIX_C_SOURCE 200809L

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOGDI
    #define NOUSER
#endif

#include <modules/asset_server/mod.h>

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

#include <raylib.h>


ECS_COMPONENT_DECLARE(AssetServerState);

typedef struct AssetSceneOwner {
    uint32_t slot;
    uint32_t generation;
    uint64_t server_instance;
} AssetSceneOwner;

ECS_COMPONENT_DECLARE(AssetSceneOwner);

typedef struct AssetSceneRecord {
    ecs_entity_t scene_entity;
    uint32_t generation;
    uint32_t next_free;
    AssetTextureId *textures;
    uint32_t texture_count;
    uint32_t texture_capacity;
    bool active;
} AssetSceneRecord;

static uint64_t g_asset_server_instance = 1u;

typedef struct AssetTextureEntry {
    char *path;                 /* Immutable until server shutdown. */
    uint64_t hash;
    uint32_t revision;          /* Cold-path cancellation/version token. */
    uint32_t scene_owners;      /* Updated only at scene load/unload. */
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

    /* Fixed-capacity Scene owner table; only touched during scene load/unload. */
    AssetSceneRecord *scene_records;
    uint32_t scene_capacity;
    uint32_t scene_free_head;
    uint64_t instance_id;
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

static bool asset_mutex_init(AssetMutex *mutex) {
#if defined(_WIN32)
    InitializeCriticalSection(mutex);
    return true;
#else
    return pthread_mutex_init(mutex, NULL) == 0;
#endif
}

static void asset_mutex_destroy(AssetMutex *mutex) {
#if defined(_WIN32)
    DeleteCriticalSection(mutex);
#else
    (void)pthread_mutex_destroy(mutex);
#endif
}

static void asset_mutex_lock(AssetMutex *mutex) {
#if defined(_WIN32)
    EnterCriticalSection(mutex);
#else
    (void)pthread_mutex_lock(mutex);
#endif
}

static void asset_mutex_unlock(AssetMutex *mutex) {
#if defined(_WIN32)
    LeaveCriticalSection(mutex);
#else
    (void)pthread_mutex_unlock(mutex);
#endif
}

static bool asset_condition_init(AssetCondition *condition) {
#if defined(_WIN32)
    InitializeConditionVariable(condition);
    return true;
#else
    return pthread_cond_init(condition, NULL) == 0;
#endif
}

static void asset_condition_destroy(AssetCondition *condition) {
#if !defined(_WIN32)
    (void)pthread_cond_destroy(condition);
#else
    (void)condition;
#endif
}

static void asset_condition_wait(AssetCondition *condition, AssetMutex *mutex) {
#if defined(_WIN32)
    (void)SleepConditionVariableCS(condition, mutex, INFINITE);
#else
    (void)pthread_cond_wait(condition, mutex);
#endif
}

static void asset_condition_signal(AssetCondition *condition) {
#if defined(_WIN32)
    WakeConditionVariable(condition);
#else
    (void)pthread_cond_signal(condition);
#endif
}

static void asset_condition_broadcast(AssetCondition *condition) {
#if defined(_WIN32)
    WakeAllConditionVariable(condition);
#else
    (void)pthread_cond_broadcast(condition);
#endif
}

/* -------------------------------------------------------------------------- */
/* Path and hash helpers                                                       */
/* -------------------------------------------------------------------------- */

static char *asset_strdup(const char *text) {
    const size_t length = strlen(text);
    char *copy = (char *)malloc(length + 1u);
    if (copy) {
        memcpy(copy, text, length + 1u);
    }
    return copy;
}

/* Reject absolute paths and traversal; collapse repeated separators and dot. */
static char *asset_normalize_path(const char *path) {
    if (!path || !path[0] || path[0] == '/' || path[0] == '\\' ||
        strchr(path, ':') != NULL) {
        return NULL;
    }

    const size_t length = strlen(path);
    char *normalized = (char *)malloc(length + 1u);
    if (!normalized) {
        return NULL;
    }

    size_t read_index = 0u;
    size_t write_index = 0u;
    while (read_index < length) {
        while (read_index < length &&
               (path[read_index] == '/' || path[read_index] == '\\')) {
            read_index++;
        }
        if (read_index == length) {
            break;
        }

        const size_t start = read_index;
        while (read_index < length && path[read_index] != '/' &&
               path[read_index] != '\\') {
            if ((unsigned char)path[read_index] < 32u) {
                free(normalized);
                return NULL;
            }
            read_index++;
        }
        const size_t segment_length = read_index - start;

        if (segment_length == 1u && path[start] == '.') {
            continue;
        }
        if (segment_length == 2u && path[start] == '.' &&
            path[start + 1u] == '.') {
            free(normalized);
            return NULL;
        }

        if (write_index) {
            normalized[write_index++] = '/';
        }
        memcpy(normalized + write_index, path + start, segment_length);
        write_index += segment_length;
    }

    if (!write_index) {
        free(normalized);
        return NULL;
    }
    normalized[write_index] = '\0';
    return normalized;
}

static uint64_t asset_hash_string(const char *text) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        hash ^= (uint64_t)*p;
        hash *= UINT64_C(1099511628211);
    }
    return hash ? hash : UINT64_C(1);
}

static uint32_t asset_next_power_of_two(uint32_t value) {
    uint32_t result = 1u;
    while (result < value && result <= UINT32_MAX / 2u) {
        result <<= 1u;
    }
    return result;
}

static int32_t asset_map_find(const AssetServer *server,
                              const char *path,
                              uint64_t hash) {
    const uint32_t mask = server->path_map_capacity - 1u;
    uint32_t index = (uint32_t)hash & mask;

    for (uint32_t probe = 0u; probe < server->path_map_capacity; ++probe) {
        const uint32_t encoded_id = server->path_map[index];
        if (encoded_id == 0u) {
            return -1;
        }
        const AssetTextureId id = encoded_id - 1u;
        const AssetTextureEntry *entry = &server->entries[id];
        if (entry->hash == hash && strcmp(entry->path, path) == 0) {
            return (int32_t)id;
        }
        index = (index + 1u) & mask;
    }
    return -1;
}

static bool asset_map_insert(AssetServer *server,
                             AssetTextureId id,
                             uint64_t hash) {
    const uint32_t mask = server->path_map_capacity - 1u;
    uint32_t index = (uint32_t)hash & mask;
    for (uint32_t probe = 0u; probe < server->path_map_capacity; ++probe) {
        if (server->path_map[index] == 0u) {
            server->path_map[index] = id + 1u;
            return true;
        }
        index = (index + 1u) & mask;
    }
    return false;
}

static char *asset_full_path(const AssetServer *server, const char *relative) {
    const size_t root_length = strlen(server->root_directory);
    const size_t path_length = strlen(relative);
    const bool separator = root_length &&
        server->root_directory[root_length - 1u] != '/' &&
        server->root_directory[root_length - 1u] != '\\';
    const size_t size = root_length + (separator ? 1u : 0u) + path_length + 1u;
    char *full = (char *)malloc(size);
    if (!full) {
        return NULL;
    }
    memcpy(full, server->root_directory, root_length);
    size_t offset = root_length;
    if (separator) {
        full[offset++] = '/';
    }
    memcpy(full + offset, relative, path_length + 1u);
    return full;
}

/* -------------------------------------------------------------------------- */
/* Priority queue                                                              */
/* -------------------------------------------------------------------------- */

static bool asset_job_before(const AssetJob *a, const AssetJob *b) {
    if (a->priority != b->priority) {
        return a->priority > b->priority;
    }
    return a->sequence < b->sequence;
}

static bool asset_job_push_locked(AssetServer *server, AssetJob job) {
    if (server->job_count == server->job_capacity) {
        return false;
    }

    uint32_t index = server->job_count++;
    server->jobs[index] = job;
    while (index) {
        const uint32_t parent = (index - 1u) / 2u;
        if (!asset_job_before(&server->jobs[index], &server->jobs[parent])) {
            break;
        }
        const AssetJob swap = server->jobs[parent];
        server->jobs[parent] = server->jobs[index];
        server->jobs[index] = swap;
        index = parent;
    }
    asset_condition_signal(&server->condition);
    return true;
}

static AssetJob asset_job_pop_locked(AssetServer *server) {
    const AssetJob result = server->jobs[0];
    --server->job_count;
    if (!server->job_count) {
        return result;
    }

    server->jobs[0] = server->jobs[server->job_count];
    uint32_t index = 0u;
    for (;;) {
        const uint32_t left = index * 2u + 1u;
        const uint32_t right = left + 1u;
        uint32_t best = index;
        if (left < server->job_count &&
            asset_job_before(&server->jobs[left], &server->jobs[best])) {
            best = left;
        }
        if (right < server->job_count &&
            asset_job_before(&server->jobs[right], &server->jobs[best])) {
            best = right;
        }
        if (best == index) {
            break;
        }
        const AssetJob swap = server->jobs[index];
        server->jobs[index] = server->jobs[best];
        server->jobs[best] = swap;
        index = best;
    }
    return result;
}

static uint32_t asset_next_revision(uint32_t revision) {
    ++revision;
    return revision ? revision : 1u;
}

static bool asset_queue_decode(AssetServer *server,
                               AssetTextureId id,
                               AssetPriority priority,
                               bool force_reload) {
    AssetTextureEntry *entry = &server->entries[id];
    if (!force_reload) {
        if (entry->status == ASSET_TEXTURE_QUEUED ||
            entry->status == ASSET_TEXTURE_RELOADING ||
            (entry->status == ASSET_TEXTURE_READY && server->gpu_textures[id].id != 0u)) {
            return true;
        }
    }

    asset_mutex_lock(&server->mutex);
    if (server->job_count == server->job_capacity) {
        asset_mutex_unlock(&server->mutex);
        (void)snprintf(entry->error, sizeof(entry->error), "asset job queue is full");
        entry->status = server->gpu_textures[id].id ? ASSET_TEXTURE_READY : ASSET_TEXTURE_FAILED;
        return false;
    }

    const uint32_t revision = asset_next_revision(entry->revision);
    AssetJob job = {
        .id = id,
        .revision = revision,
        .sequence = server->next_job_sequence++,
        .priority = (int32_t)priority
    };
    const bool pushed = asset_job_push_locked(server, job);
    if (pushed) {
        entry->revision = revision;
        entry->status = server->gpu_textures[id].id
            ? ASSET_TEXTURE_RELOADING : ASSET_TEXTURE_QUEUED;
        entry->error[0] = '\0';
    }
    asset_mutex_unlock(&server->mutex);
    return pushed;
}

/* -------------------------------------------------------------------------- */
/* CPU file read and image decode                                               */
/* -------------------------------------------------------------------------- */

static bool asset_read_file(const char *path,
                            unsigned char **out_data,
                            int *out_size,
                            char *error,
                            size_t error_size) {
    *out_data = NULL;
    *out_size = 0;

    FILE *file = fopen(path, "rb");
    if (!file) {
        (void)snprintf(error, error_size, "cannot open file");
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        (void)snprintf(error, error_size, "cannot seek file");
        return false;
    }

    const long length = ftell(file);
    if (length <= 0 || length > INT_MAX || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        (void)snprintf(error, error_size, "invalid or oversized file");
        return false;
    }

    unsigned char *data = (unsigned char *)malloc((size_t)length);
    if (!data) {
        fclose(file);
        (void)snprintf(error, error_size, "out of memory reading file");
        return false;
    }

    const size_t read_count = fread(data, 1u, (size_t)length, file);
    fclose(file);
    if (read_count != (size_t)length) {
        free(data);
        (void)snprintf(error, error_size, "short read");
        return false;
    }

    *out_data = data;
    *out_size = (int)length;
    return true;
}

static bool asset_decode_image(const char *full_path,
                                Image *out_image,
                                char *error,
                                size_t error_size) {
    out_image->data = NULL;
    out_image->width = 0;
    out_image->height = 0;
    out_image->mipmaps = 0;
    out_image->format = 0;

    const char *extension = strrchr(full_path, '.');
    if (!extension) {
        (void)snprintf(error, error_size, "image file has no extension");
        return false;
    }

    unsigned char *file_data = NULL;
    int file_size = 0;
    if (!asset_read_file(full_path, &file_data, &file_size, error, error_size)) {
        return false;
    }

    /* CPU decode only. LoadTextureFromImage is called later on render thread. */
    *out_image = LoadImageFromMemory(extension, file_data, file_size);
    free(file_data);

    if (!out_image->data || out_image->width <= 0 || out_image->height <= 0) {
        if (out_image->data) {
            UnloadImage(*out_image);
        }
        memset(out_image, 0, sizeof(*out_image));
        (void)snprintf(error, error_size, "raylib image decoder rejected the file");
        return false;
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/* Worker pool                                                                 */
/* -------------------------------------------------------------------------- */

static void asset_worker_run(AssetServer *server) {
    for (;;) {
        AssetJob job;
        const char *relative_path;

        asset_mutex_lock(&server->mutex);
        while (!server->stopping && server->job_count == 0u) {
            asset_condition_wait(&server->condition, &server->mutex);
        }
        if (server->stopping) {
            asset_mutex_unlock(&server->mutex);
            return;
        }

        job = asset_job_pop_locked(server);
        if (job.id == ASSET_TEXTURE_INVALID ||
            job.id > server->texture_capacity ||
            server->entries[job.id].revision != job.revision) {
            /* Bundle unload/reload invalidated this queued job before decode. */
            asset_mutex_unlock(&server->mutex);
            continue;
        }
        ++server->active_workers;
        relative_path = server->entries[job.id].path;
        asset_mutex_unlock(&server->mutex);

        AssetCompletion completion;
        memset(&completion, 0, sizeof(completion));
        completion.id = job.id;
        completion.revision = job.revision;

        char *full_path = asset_full_path(server, relative_path);
        if (full_path) {
            completion.decoded = asset_decode_image(
                full_path, &completion.image,
                completion.error, sizeof(completion.error));
            free(full_path);
        } else {
            (void)snprintf(completion.error, sizeof(completion.error),
                           "out of memory building asset path");
        }

        asset_mutex_lock(&server->mutex);
        --server->active_workers;
        while (!server->stopping &&
               server->completion_count == server->completion_capacity) {
            asset_condition_wait(&server->condition, &server->mutex);
        }
        if (server->stopping) {
            asset_mutex_unlock(&server->mutex);
            if (completion.decoded) {
                UnloadImage(completion.image);
            }
            return;
        }

        server->completions[server->completion_tail] = completion;
        server->completion_tail =
            (server->completion_tail + 1u) % server->completion_capacity;
        ++server->completion_count;
        asset_condition_broadcast(&server->condition);
        asset_mutex_unlock(&server->mutex);
    }
}

#if defined(_WIN32)
static DWORD WINAPI asset_worker_entry(LPVOID argument) {
    asset_worker_run((AssetServer *)argument);
    return 0;
}
#else
static void *asset_worker_entry(void *argument) {
    asset_worker_run((AssetServer *)argument);
    return NULL;
}
#endif

static uint32_t asset_default_worker_count(void) {
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    uint32_t processors = (uint32_t)info.dwNumberOfProcessors;
#else
    const long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    uint32_t processors = cpu_count > 0 ? (uint32_t)cpu_count : 2u;
#endif
    uint32_t workers = processors > 1u ? processors - 1u : 1u;
    if (workers > 8u) {
        workers = 8u;
    }
    return workers;
}

/* -------------------------------------------------------------------------- */
/* Flecs integration and public API                                            */
/* -------------------------------------------------------------------------- */

static void asset_scene_owner_on_remove(ecs_iter_t *it);

void AssetServerModuleImport(ecs_world_t *world) {
    ECS_MODULE(world, AssetServerModule);
    ECS_COMPONENT_DEFINE(world, AssetServerState);
    ECS_COMPONENT_DEFINE(world, AssetSceneOwner);
    ecs_set_hooks(world, AssetSceneOwner, {
        .on_remove = asset_scene_owner_on_remove
    });

    asset_server_init(world, NULL);
}

AssetServerConfig asset_server_default_config(void) {
    AssetServerConfig config;
    config.root_directory = "data";
    config.max_textures = 8192u;
    config.worker_count = 0u;
    config.job_queue_capacity = 2048u;
    config.completion_queue_capacity = 256u;
    config.max_scenes = 256u;
    return config;
}

static void asset_server_free_partial(AssetServer *server) {
    if (!server) {
        return;
    }
    if (server->sync_initialized) {
        asset_condition_destroy(&server->condition);
        asset_mutex_destroy(&server->mutex);
    }
    free(server->root_directory);
    if (server->scene_records) {
        for (uint32_t i = 0u; i < server->scene_capacity; ++i) {
            free(server->scene_records[i].textures);
        }
    }
    free(server->scene_records);
    free(server->gpu_textures);
    free(server->entries);
    free(server->path_map);
    free(server->jobs);
    free(server->completions);
    free(server->workers);
    free(server);
}

bool asset_server_init(ecs_world_t *world, const AssetServerConfig *input_config) {
    if (!world || ecs_id(AssetServerState) == 0u || asset_server_get(world)) {
        return false;
    }

    const AssetServerConfig defaults = asset_server_default_config();
    const AssetServerConfig config = input_config ? *input_config : defaults;
    const char *root = config.root_directory && config.root_directory[0]
        ? config.root_directory : "data";
    const uint32_t max_textures = config.max_textures
        ? config.max_textures : defaults.max_textures;
    const uint32_t job_capacity = config.job_queue_capacity
        ? config.job_queue_capacity : defaults.job_queue_capacity;
    const uint32_t completion_capacity = config.completion_queue_capacity
        ? config.completion_queue_capacity : defaults.completion_queue_capacity;
    const uint32_t max_scenes = config.max_scenes
        ? config.max_scenes : defaults.max_scenes;
    uint32_t workers_requested = config.worker_count
        ? config.worker_count : asset_default_worker_count();
    if (workers_requested > 32u) {
        workers_requested = 32u;
    }

    if (max_textures > (uint32_t)INT32_MAX ||
        max_textures > (UINT32_MAX / 2u) || job_capacity == 0u ||
        completion_capacity == 0u || max_scenes == 0u ||
        workers_requested == 0u) {
        return false;
    }

    AssetServer *server = (AssetServer *)calloc(1u, sizeof(AssetServer));
    if (!server) {
        return false;
    }

    server->root_directory = asset_strdup(root);
    server->texture_capacity = max_textures;
    server->texture_count = 1u; /* ID zero is permanently invalid. */
    server->job_capacity = job_capacity;
    server->completion_capacity = completion_capacity;
    server->gpu_textures = (Texture2D *)calloc(
        (size_t)max_textures + 1u, sizeof(Texture2D));
    server->entries = (AssetTextureEntry *)calloc(
        (size_t)max_textures + 1u, sizeof(AssetTextureEntry));
    server->scene_capacity = max_scenes;
    server->scene_free_head = 0u;
    server->scene_records = (AssetSceneRecord *)calloc(
        max_scenes, sizeof(AssetSceneRecord));
    server->instance_id = g_asset_server_instance++;
    if (server->instance_id == 0u) {
        server->instance_id = g_asset_server_instance++;
    }
    server->path_map_capacity = asset_next_power_of_two(max_textures * 2u);
    if (server->path_map_capacity < max_textures * 2u) {
        asset_server_free_partial(server);
        return false;
    }
    server->path_map = (uint32_t *)calloc(
        server->path_map_capacity, sizeof(uint32_t));
    server->jobs = (AssetJob *)calloc(job_capacity, sizeof(AssetJob));
    server->completions = (AssetCompletion *)calloc(
        completion_capacity, sizeof(AssetCompletion));
    server->workers = (AssetThread *)calloc(
        workers_requested, sizeof(AssetThread));

    if (!server->root_directory || !server->gpu_textures ||
        !server->entries || !server->scene_records || !server->path_map ||
        !server->jobs || !server->completions || !server->workers ||
        !asset_mutex_init(&server->mutex)) {
        asset_server_free_partial(server);
        return false;
    }
    if (!asset_condition_init(&server->condition)) {
        asset_mutex_destroy(&server->mutex);
        server->sync_initialized = false;
        asset_server_free_partial(server);
        return false;
    }
    server->sync_initialized = true;

    for (uint32_t i = 0u; i < max_scenes; ++i) {
        server->scene_records[i].generation = 1u;
        server->scene_records[i].next_free =
            (i + 1u < max_scenes) ? (i + 1u) : UINT32_MAX;
    }

    for (uint32_t i = 0u; i < workers_requested; ++i) {
#if defined(_WIN32)
        server->workers[i] = CreateThread(
            NULL, 0u, asset_worker_entry, server, 0u, NULL);
        if (!server->workers[i]) {
            break;
        }
#else
        if (pthread_create(&server->workers[i], NULL,
                           asset_worker_entry, server) != 0) {
            break;
        }
#endif
        ++server->worker_count;
    }

    if (!server->worker_count) {
        asset_server_free_partial(server);
        return false;
    }

    ecs_set(world, ecs_id(AssetServerState), AssetServerState,
            { .implementation = server });
    return true;
}

const AssetServer *asset_server_get(const ecs_world_t *world) {
    if (!world || ecs_id(AssetServerState) == 0u) {
        return NULL;
    }
    const AssetServerState *state = ecs_get(
        world, ecs_id(AssetServerState), AssetServerState);
    return state ? state->implementation : NULL;
}

static void asset_evict_entry(AssetServer *server, AssetTextureId id) {
    AssetTextureEntry *entry = &server->entries[id];
    Texture2D old_texture = {0};

    /* Synchronize the revision with workers that may be popping queued jobs. */
    asset_mutex_lock(&server->mutex);
    entry->revision = asset_next_revision(entry->revision);
    old_texture = server->gpu_textures[id];
    memset(&server->gpu_textures[id], 0, sizeof(Texture2D));
    entry->status = ASSET_TEXTURE_EMPTY;
    entry->error[0] = '\0';
    asset_mutex_unlock(&server->mutex);

    if (old_texture.id) {
        UnloadTexture(old_texture);
    }
}

void asset_server_fini(ecs_world_t *world) {
    const AssetServer *constant_server = asset_server_get(world);
    if (!constant_server) {
        return;
    }
    AssetServer *server = (AssetServer *)constant_server;

    ecs_set(world, ecs_id(AssetServerState), AssetServerState,
            { .implementation = NULL });

    asset_mutex_lock(&server->mutex);
    server->stopping = true;
    asset_condition_broadcast(&server->condition);
    asset_mutex_unlock(&server->mutex);

    for (uint32_t i = 0u; i < server->worker_count; ++i) {
#if defined(_WIN32)
        (void)WaitForSingleObject(server->workers[i], INFINITE);
        CloseHandle(server->workers[i]);
#else
        (void)pthread_join(server->workers[i], NULL);
#endif
    }

    /* Workers have stopped; pending Images are CPU memory and can now be freed. */
    for (uint32_t i = 0u; i < server->completion_count; ++i) {
        const uint32_t index = (server->completion_head + i) %
                               server->completion_capacity;
        AssetCompletion *completion = &server->completions[index];
        if (completion->decoded) {
            UnloadImage(completion->image);
        }
    }

    for (uint32_t id = 1u; id < server->texture_count; ++id) {
        AssetTextureEntry *entry = &server->entries[id];
        if (server->gpu_textures[id].id) {
            UnloadTexture(server->gpu_textures[id]);
        }
        free(entry->path);
    }

    asset_server_free_partial(server);
}

static AssetTextureId asset_request_texture(AssetServer *server,
                                             const char *relative_path,
                                             AssetPriority priority) {
    char *normalized = asset_normalize_path(relative_path);
    if (!normalized) {
        return ASSET_TEXTURE_INVALID;
    }

    ++server->stats.texture_requests;
    const uint64_t hash = asset_hash_string(normalized);
    const int32_t found = asset_map_find(server, normalized, hash);
    if (found >= 0) {
        const AssetTextureId id = (AssetTextureId)found;
        AssetTextureEntry *entry = &server->entries[id];
        ++server->stats.cache_hits;
        if (!server->gpu_textures[id].id &&
            (entry->status == ASSET_TEXTURE_EMPTY ||
             entry->status == ASSET_TEXTURE_FAILED)) {
            (void)asset_queue_decode(server, id, priority, false);
        }
        free(normalized);
        return id;
    }

    if (server->texture_count > server->texture_capacity) {
        free(normalized);
        return ASSET_TEXTURE_INVALID;
    }

    const AssetTextureId id = server->texture_count++;
    AssetTextureEntry *entry = &server->entries[id];
    entry->path = normalized;
    entry->hash = hash;
    entry->revision = 0u;
    entry->status = ASSET_TEXTURE_EMPTY;

    if (!asset_map_insert(server, id, hash)) {
        free(entry->path);
        memset(entry, 0, sizeof(*entry));
        --server->texture_count;
        return ASSET_TEXTURE_INVALID;
    }

    (void)asset_queue_decode(server, id, priority, false);
    return id;
}

static AssetSceneRecord *asset_scene_record_from_owner(
    AssetServer *server, const AssetSceneOwner *owner) {
    if (!server || !owner || owner->server_instance != server->instance_id ||
        owner->slot >= server->scene_capacity) {
        return NULL;
    }
    AssetSceneRecord *record = &server->scene_records[owner->slot];
    if (!record->active || record->generation != owner->generation) {
        return NULL;
    }
    return record;
}

static uint32_t asset_scene_record_allocate(AssetServer *server,
                                            ecs_entity_t scene_entity) {
    if (!server || server->scene_free_head == UINT32_MAX) {
        return UINT32_MAX;
    }
    const uint32_t slot = server->scene_free_head;
    AssetSceneRecord *record = &server->scene_records[slot];
    server->scene_free_head = record->next_free;
    record->scene_entity = scene_entity;
    record->next_free = UINT32_MAX;
    record->texture_count = 0u;
    record->texture_capacity = 0u;
    record->active = true;
    return slot;
}

static bool asset_scene_record_reserve(AssetSceneRecord *record,
                                       uint32_t required) {
    if (required <= record->texture_capacity) {
        return true;
    }
    uint32_t capacity = record->texture_capacity
        ? record->texture_capacity * 2u : 16u;
    while (capacity < required) {
        if (capacity > UINT32_MAX / 2u) {
            return false;
        }
        capacity *= 2u;
    }
    AssetTextureId *textures = (AssetTextureId *)realloc(
        record->textures, (size_t)capacity * sizeof(AssetTextureId));
    if (!textures) {
        return false;
    }
    record->textures = textures;
    record->texture_capacity = capacity;
    return true;
}

static bool asset_scene_record_contains(const AssetSceneRecord *record,
                                        AssetTextureId id) {
    for (uint32_t i = 0u; i < record->texture_count; ++i) {
        if (record->textures[i] == id) {
            return true;
        }
    }
    return false;
}

static void asset_scene_record_release(AssetServer *server,
                                       const AssetSceneOwner *owner) {
    AssetSceneRecord *record = asset_scene_record_from_owner(server, owner);
    if (!record) {
        return;
    }

    for (uint32_t i = 0u; i < record->texture_count; ++i) {
        const AssetTextureId id = record->textures[i];
        if (!id || id >= server->texture_count) {
            continue;
        }
        AssetTextureEntry *entry = &server->entries[id];
        if (entry->scene_owners != 0u) {
            --entry->scene_owners;
        }
        if (entry->scene_owners == 0u && !entry->globally_pinned) {
            asset_evict_entry(server, id);
        }
    }

    free(record->textures);
    record->textures = NULL;
    record->texture_count = 0u;
    record->texture_capacity = 0u;
    record->scene_entity = 0u;
    record->active = false;
    record->generation = asset_next_revision(record->generation);
    record->next_free = server->scene_free_head;
    server->scene_free_head = owner->slot;
}

static void asset_scene_owner_on_remove(ecs_iter_t *it) {
    const AssetSceneOwner *owners = ecs_field(it, AssetSceneOwner, 0);
    AssetServer *server = (AssetServer *)asset_server_get(it->world);
    if (!server) {
        return; /* Server shutdown already released every GPU and CPU resource. */
    }
    for (int32_t i = 0; i < it->count; ++i) {
        asset_scene_record_release(server, &owners[i]);
    }
}

AssetTextureId asset_scene_texture(ecs_world_t *world,
                                   ecs_entity_t scene,
                                   const char *relative_path,
                                   AssetPriority priority) {
    AssetServer *server = (AssetServer *)asset_server_get(world);
    if (!server || !scene || ecs_id(Scene) == 0u ||
        !ecs_has(world, scene, Scene)) {
        return ASSET_TEXTURE_INVALID;
    }

    const AssetSceneOwner *owner_ptr = ecs_get(world, scene, AssetSceneOwner);
    AssetSceneOwner owner = {0};
    AssetSceneRecord *record = NULL;

    if (owner_ptr && owner_ptr->server_instance == server->instance_id) {
        owner = *owner_ptr;
        record = asset_scene_record_from_owner(server, &owner);
    } else {
        if (owner_ptr) {
            /* Stale owner from a previous AssetServer instance. Its hook is a no-op. */
            ecs_remove(world, scene, AssetSceneOwner);
        }
        const uint32_t slot = asset_scene_record_allocate(server, scene);
        if (slot == UINT32_MAX) {
            return ASSET_TEXTURE_INVALID;
        }
        owner.slot = slot;
        owner.generation = server->scene_records[slot].generation;
        owner.server_instance = server->instance_id;
        ecs_set(world, scene, AssetSceneOwner, {
            .slot = owner.slot,
            .generation = owner.generation,
            .server_instance = owner.server_instance
        });
        record = &server->scene_records[slot];
    }

    if (!record) {
        return ASSET_TEXTURE_INVALID;
    }

    const AssetTextureId id = asset_request_texture(server, relative_path, priority);
    if (id == ASSET_TEXTURE_INVALID) {
        return ASSET_TEXTURE_INVALID;
    }

    if (asset_scene_record_contains(record, id)) {
        return id;
    }
    if (!asset_scene_record_reserve(record, record->texture_count + 1u)) {
        AssetTextureEntry *entry = &server->entries[id];
        if (entry->scene_owners == 0u && !entry->globally_pinned) {
            asset_evict_entry(server, id);
        }
        return ASSET_TEXTURE_INVALID;
    }

    record->textures[record->texture_count++] = id;
    ++server->entries[id].scene_owners;
    return id;
}

AssetTextureId asset_server_texture_global(ecs_world_t *world,
                                           const char *relative_path,
                                           AssetPriority priority) {
    AssetServer *server = (AssetServer *)asset_server_get(world);
    if (!server) {
        return ASSET_TEXTURE_INVALID;
    }
    const AssetTextureId id = asset_request_texture(server, relative_path, priority);
    if (id) {
        server->entries[id].globally_pinned = true;
    }
    return id;
}

bool asset_texture_reload(ecs_world_t *world,
                          AssetTextureId texture,
                          AssetPriority priority) {
    AssetServer *server = (AssetServer *)asset_server_get(world);
    if (!server || !texture || texture >= server->texture_count) {
        return false;
    }
    AssetTextureEntry *entry = &server->entries[texture];
    if (entry->status == ASSET_TEXTURE_QUEUED ||
        entry->status == ASSET_TEXTURE_RELOADING) {
        return false;
    }
    if (asset_queue_decode(server, texture, priority, true)) {
        ++server->stats.reload_requests;
        return true;
    }
    return false;
}

uint32_t asset_server_pump(ecs_world_t *world, uint32_t max_completions) {
    AssetServer *server = (AssetServer *)asset_server_get(world);
    if (!server) {
        return 0u;
    }

    uint32_t processed = 0u;
    while (max_completions == 0u || processed < max_completions) {
        AssetCompletion completion;
        memset(&completion, 0, sizeof(completion));

        asset_mutex_lock(&server->mutex);
        if (!server->completion_count) {
            asset_mutex_unlock(&server->mutex);
            break;
        }
        completion = server->completions[server->completion_head];
        server->completion_head =
            (server->completion_head + 1u) % server->completion_capacity;
        --server->completion_count;
        asset_condition_broadcast(&server->condition);
        asset_mutex_unlock(&server->mutex);

        ++processed;
        if (!completion.id || completion.id >= server->texture_count) {
            if (completion.decoded) {
                UnloadImage(completion.image);
            }
            continue;
        }

        AssetTextureEntry *entry = &server->entries[completion.id];
        if (entry->revision != completion.revision) {
            if (completion.decoded) {
                UnloadImage(completion.image);
            }
            continue;
        }

        if (!completion.decoded) {
            entry->status = server->gpu_textures[completion.id].id
                ? ASSET_TEXTURE_READY : ASSET_TEXTURE_FAILED;
            (void)snprintf(entry->error, sizeof(entry->error), "%s",
                           completion.error[0] ? completion.error : "image decode failed");
            ++server->stats.decode_failures;
            continue;
        }

        const Texture2D replacement = LoadTextureFromImage(completion.image);
        UnloadImage(completion.image);
        if (!replacement.id) {
            entry->status = server->gpu_textures[completion.id].id
                ? ASSET_TEXTURE_READY : ASSET_TEXTURE_FAILED;
            (void)snprintf(entry->error, sizeof(entry->error),
                           "raylib GPU texture upload failed");
            ++server->stats.gpu_upload_failures;
            continue;
        }

        if (server->gpu_textures[completion.id].id) {
            UnloadTexture(server->gpu_textures[completion.id]);
        }
        server->gpu_textures[completion.id] = replacement;
        entry->status = ASSET_TEXTURE_READY;
        entry->error[0] = '\0';
        ++server->stats.decode_successes;
        ++server->stats.gpu_uploads;
    }
    return processed;
}

const Texture2D *asset_texture_fast(const AssetServer *server,
                                    AssetTextureId texture) {
    if (!server || texture == ASSET_TEXTURE_INVALID ||
        texture >= server->texture_count) {
        return NULL;
    }
    const Texture2D *gpu_texture = &server->gpu_textures[texture];
    return gpu_texture->id ? gpu_texture : NULL;
}

AssetTextureStatus asset_texture_status(const AssetServer *server,
                                        AssetTextureId texture) {
    if (!server || !texture || texture >= server->texture_count) {
        return ASSET_TEXTURE_EMPTY;
    }
    return server->entries[texture].status;
}

bool asset_texture_error(const AssetServer *server,
                         AssetTextureId texture,
                         char *destination,
                         uint32_t destination_size) {
    if (!destination || !destination_size) {
        return false;
    }
    destination[0] = '\0';
    if (!server || !texture || texture >= server->texture_count) {
        return false;
    }
    (void)snprintf(destination, destination_size, "%s",
                   server->entries[texture].error);
    return true;
}

void asset_server_get_stats(const AssetServer *server,
                            AssetServerStats *out_stats) {
    if (!out_stats) {
        return;
    }
    memset(out_stats, 0, sizeof(*out_stats));
    if (!server) {
        return;
    }

    *out_stats = server->stats;
    out_stats->texture_count = server->texture_count - 1u;

    asset_mutex_lock((AssetMutex *)&server->mutex);
    out_stats->queued_jobs = server->job_count;
    out_stats->active_workers = server->active_workers;
    out_stats->pending_completions = server->completion_count;
    asset_mutex_unlock((AssetMutex *)&server->mutex);

    for (uint32_t id = 1u; id < server->texture_count; ++id) {
        const Texture2D *gpu_texture = &server->gpu_textures[id];
        if (gpu_texture->id) {
            out_stats->resident_texture_bytes +=
                (uint64_t)gpu_texture->width *
                (uint64_t)gpu_texture->height * 4u;
        }
    }
}

uint32_t asset_server_collect_unused(AssetServer *server) {
    if (!server) {
        return 0u;
    }
    uint32_t evicted = 0u;
    for (uint32_t id = 1u; id < server->texture_count; ++id) {
        AssetTextureEntry *entry = &server->entries[id];
        if (!entry->scene_owners && !entry->globally_pinned &&
            (server->gpu_textures[id].id ||
             entry->status != ASSET_TEXTURE_EMPTY)) {
            asset_evict_entry(server, id);
            ++evicted;
        }
    }
    return evicted;
}
