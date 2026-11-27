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

typedef struct {
    uint32_t slot;
    uint32_t generation;
    int32_t priority;
    uint64_t sequence;
} AssetJob;

typedef struct {
    uint32_t slot;
    uint32_t generation;
} AssetCompletion;

typedef struct {
    char *path;
    uint32_t generation;
    uint32_t reference_count;
    uint32_t next_free;
    AssetStatus status;
    bool used;
    bool has_texture;
    bool pending_image_valid;
    Texture2D texture;
    Image pending_image;
    char error[192];
} AssetSlot;

struct AssetServer {
    char *root_directory;
    AssetSlot *slots;
    uint32_t slot_count;
    uint32_t slot_capacity;
    uint32_t free_head;

    /* Open-addressed path -> slot map. 0 is empty; UINT32_MAX is a tombstone. */
    uint32_t *map;
    uint32_t map_capacity;
    uint32_t map_count;
    uint32_t map_tombstones;

    AssetJob *jobs;
    uint32_t job_count;
    uint32_t job_capacity;
    uint64_t next_job_sequence;

    /* Completed-image list avoids scanning every cached slot each frame. */
    AssetCompletion *completed;
    uint32_t completed_count;
    uint32_t completed_capacity;
    bool completion_overflow;

    AssetThread *workers;
    uint32_t worker_count;
    uint32_t active_workers;
    bool stopping;
    bool synchronization_ready;
    AssetMutex mutex;
    AssetCondition condition;

    AssetServerStats stats;
};

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

static char *asset_string_duplicate(const char *text) {
    const size_t length = strlen(text);
    char *copy = (char *)malloc(length + 1u);
    if (copy) {
        memcpy(copy, text, length + 1u);
    }
    return copy;
}

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
        if (read_index >= length) {
            break;
        }

        const size_t segment_start = read_index;
        while (read_index < length && path[read_index] != '/' &&
               path[read_index] != '\\') {
            const unsigned char ch = (unsigned char)path[read_index];
            if (ch < 32u) {
                free(normalized);
                return NULL;
            }
            read_index++;
        }
        const size_t segment_length = read_index - segment_start;

        if (segment_length == 1u && path[segment_start] == '.') {
            continue;
        }
        if (segment_length == 2u && path[segment_start] == '.' &&
            path[segment_start + 1u] == '.') {
            free(normalized);
            return NULL;
        }

        if (write_index != 0u) {
            normalized[write_index++] = '/';
        }
        memcpy(normalized + write_index, path + segment_start, segment_length);
        write_index += segment_length;
    }

    if (write_index == 0u) {
        free(normalized);
        return NULL;
    }
    normalized[write_index] = '\0';
    return normalized;
}

static char *asset_make_full_path(const AssetServer *server,
                                  const char *relative_path) {
    const size_t root_length = strlen(server->root_directory);
    const size_t path_length = strlen(relative_path);
    const bool needs_separator = root_length != 0u &&
        server->root_directory[root_length - 1u] != '/' &&
        server->root_directory[root_length - 1u] != '\\';
    const size_t total = root_length + (needs_separator ? 1u : 0u) +
                         path_length + 1u;
    char *full_path = (char *)malloc(total);
    if (!full_path) {
        return NULL;
    }

    memcpy(full_path, server->root_directory, root_length);
    size_t offset = root_length;
    if (needs_separator) {
        full_path[offset++] = '/';
    }
    memcpy(full_path + offset, relative_path, path_length + 1u);
    return full_path;
}

static uint64_t asset_hash_path(const char *path) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *cursor = (const unsigned char *)path;
         *cursor; ++cursor) {
        hash ^= (uint64_t)*cursor;
        hash *= UINT64_C(1099511628211);
    }
    return hash ? hash : UINT64_C(1);
}

static bool asset_map_rehash(AssetServer *server, uint32_t new_capacity) {
    uint32_t *new_map = (uint32_t *)calloc(new_capacity, sizeof(uint32_t));
    if (!new_map) {
        return false;
    }

    for (uint32_t old_index = 0u; old_index < server->map_capacity; old_index++) {
        const uint32_t encoded_slot = server->map[old_index];
        if (encoded_slot == 0u || encoded_slot == UINT32_MAX) {
            continue;
        }

        const uint32_t slot_index = encoded_slot - 1u;
        if (slot_index >= server->slot_count || !server->slots[slot_index].used) {
            continue;
        }

        uint32_t new_index = (uint32_t)(asset_hash_path(
            server->slots[slot_index].path) & (uint64_t)(new_capacity - 1u));
        while (new_map[new_index] != 0u) {
            new_index = (new_index + 1u) & (new_capacity - 1u);
        }
        new_map[new_index] = encoded_slot;
    }

    free(server->map);
    server->map = new_map;
    server->map_capacity = new_capacity;
    server->map_tombstones = 0u;
    return true;
}

static bool asset_map_ensure_capacity(AssetServer *server) {
    if (server->map_capacity == 0u) {
        return asset_map_rehash(server, 64u);
    }

    const uint64_t occupied = (uint64_t)server->map_count +
                              (uint64_t)server->map_tombstones + 1u;
    if (occupied * 10u < (uint64_t)server->map_capacity * 7u) {
        return true;
    }
    if (server->map_capacity > UINT32_MAX / 2u) {
        return false;
    }
    return asset_map_rehash(server, server->map_capacity * 2u);
}

static int32_t asset_map_find(const AssetServer *server, const char *path) {
    if (server->map_capacity == 0u) {
        return -1;
    }

    uint32_t index = (uint32_t)(asset_hash_path(path) &
                                (uint64_t)(server->map_capacity - 1u));
    for (uint32_t probe = 0u; probe < server->map_capacity; probe++) {
        const uint32_t encoded_slot = server->map[index];
        if (encoded_slot == 0u) {
            return -1;
        }
        if (encoded_slot != UINT32_MAX) {
            const uint32_t slot_index = encoded_slot - 1u;
            if (slot_index < server->slot_count &&
                server->slots[slot_index].used &&
                strcmp(server->slots[slot_index].path, path) == 0) {
                return (int32_t)slot_index;
            }
        }
        index = (index + 1u) & (server->map_capacity - 1u);
    }
    return -1;
}

static bool asset_map_insert(AssetServer *server, uint32_t slot_index) {
    if (!asset_map_ensure_capacity(server)) {
        return false;
    }

    const char *path = server->slots[slot_index].path;
    uint32_t index = (uint32_t)(asset_hash_path(path) &
                                (uint64_t)(server->map_capacity - 1u));
    uint32_t first_tombstone = UINT32_MAX;

    for (uint32_t probe = 0u; probe < server->map_capacity; probe++) {
        const uint32_t entry = server->map[index];
        if (entry == UINT32_MAX && first_tombstone == UINT32_MAX) {
            first_tombstone = index;
        } else if (entry == 0u) {
            const uint32_t target = first_tombstone != UINT32_MAX
                ? first_tombstone : index;
            if (server->map[target] == UINT32_MAX) {
                server->map_tombstones--;
            }
            server->map[target] = slot_index + 1u;
            server->map_count++;
            return true;
        }
        index = (index + 1u) & (server->map_capacity - 1u);
    }
    return false;
}

static void asset_map_remove(AssetServer *server, uint32_t slot_index) {
    if (server->map_capacity == 0u || slot_index >= server->slot_count) {
        return;
    }

    const char *path = server->slots[slot_index].path;
    uint32_t index = (uint32_t)(asset_hash_path(path) &
                                (uint64_t)(server->map_capacity - 1u));
    for (uint32_t probe = 0u; probe < server->map_capacity; probe++) {
        const uint32_t entry = server->map[index];
        if (entry == 0u) {
            return;
        }
        if (entry == slot_index + 1u) {
            server->map[index] = UINT32_MAX;
            server->map_count--;
            server->map_tombstones++;
            return;
        }
        index = (index + 1u) & (server->map_capacity - 1u);
    }
}

static bool asset_slots_reserve(AssetServer *server, uint32_t required) {
    if (required <= server->slot_capacity) {
        return true;
    }

    uint32_t new_capacity = server->slot_capacity ? server->slot_capacity : 64u;
    while (new_capacity < required) {
        if (new_capacity > UINT32_MAX / 2u) {
            return false;
        }
        new_capacity *= 2u;
    }

    AssetSlot *new_slots = (AssetSlot *)realloc(
        server->slots, (size_t)new_capacity * sizeof(AssetSlot));
    if (!new_slots) {
        return false;
    }
    memset(new_slots + server->slot_capacity, 0,
           (size_t)(new_capacity - server->slot_capacity) * sizeof(AssetSlot));
    server->slots = new_slots;
    server->slot_capacity = new_capacity;
    return true;
}

static uint32_t asset_slot_allocate(AssetServer *server) {
    if (server->free_head != UINT32_MAX) {
        const uint32_t index = server->free_head;
        server->free_head = server->slots[index].next_free;
        const uint32_t generation = server->slots[index].generation;
        memset(&server->slots[index], 0, sizeof(AssetSlot));
        server->slots[index].generation = generation ? generation : 1u;
        server->slots[index].next_free = UINT32_MAX;
        server->slots[index].used = true;
        return index;
    }

    if (!asset_slots_reserve(server, server->slot_count + 1u)) {
        return UINT32_MAX;
    }

    const uint32_t index = server->slot_count++;
    memset(&server->slots[index], 0, sizeof(AssetSlot));
    server->slots[index].generation = 1u;
    server->slots[index].next_free = UINT32_MAX;
    server->slots[index].used = true;
    return index;
}

static bool asset_job_before(const AssetJob *a, const AssetJob *b) {
    if (a->priority != b->priority) {
        return a->priority > b->priority;
    }
    return a->sequence < b->sequence;
}

static bool asset_queue_push(AssetServer *server, uint32_t slot_index,
                             int32_t priority) {
    if (server->job_count >= server->job_capacity) {
        return false;
    }

    AssetJob job = {
        .slot = slot_index,
        .generation = server->slots[slot_index].generation,
        .priority = priority,
        .sequence = server->next_job_sequence++
    };

    uint32_t index = server->job_count++;
    server->jobs[index] = job;
    while (index > 0u) {
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

static AssetJob asset_queue_pop(AssetServer *server) {
    const AssetJob result = server->jobs[0];
    server->job_count--;
    if (server->job_count == 0u) {
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

static bool asset_completion_push_locked(AssetServer *server,
                                         uint32_t slot_index,
                                         uint32_t generation) {
    if (server->completed_count == server->completed_capacity) {
        uint32_t new_capacity = server->completed_capacity
            ? server->completed_capacity * 2u : 64u;
        if (new_capacity < server->completed_capacity) {
            return false;
        }
        AssetCompletion *new_items = (AssetCompletion *)realloc(
            server->completed, (size_t)new_capacity * sizeof(AssetCompletion));
        if (!new_items) {
            return false;
        }
        server->completed = new_items;
        server->completed_capacity = new_capacity;
    }

    server->completed[server->completed_count++] = (AssetCompletion){
        .slot = slot_index,
        .generation = generation
    };
    return true;
}

static bool asset_completion_take_locked(AssetServer *server, Image *out_image,
                                         uint32_t *out_slot,
                                         uint32_t *out_generation) {
    while (server->completed_count != 0u) {
        const AssetCompletion completion =
            server->completed[--server->completed_count];
        if (completion.slot >= server->slot_count) {
            continue;
        }
        AssetSlot *slot = &server->slots[completion.slot];
        if (!slot->used || slot->generation != completion.generation ||
            !slot->pending_image_valid) {
            continue;
        }

        *out_image = slot->pending_image;
        *out_slot = completion.slot;
        *out_generation = completion.generation;
        slot->pending_image.data = NULL;
        slot->pending_image_valid = false;
        return true;
    }

    /* Allocation failure in a worker falls back to one slow scan, not data loss. */
    if (server->completion_overflow) {
        for (uint32_t i = 0u; i < server->slot_count; i++) {
            AssetSlot *slot = &server->slots[i];
            if (slot->used && slot->pending_image_valid) {
                *out_image = slot->pending_image;
                *out_slot = i;
                *out_generation = slot->generation;
                slot->pending_image.data = NULL;
                slot->pending_image_valid = false;
                return true;
            }
        }
        server->completion_overflow = false;
    }
    return false;
}

static AssetSlot *asset_slot_from_handle(AssetServer *server,
                                         AssetHandle handle) {
    if (!ASSET_HANDLE_IS_VALID(handle) || handle.index >= server->slot_count) {
        return NULL;
    }
    AssetSlot *slot = &server->slots[handle.index];
    if (!slot->used || slot->generation != handle.generation) {
        return NULL;
    }
    return slot;
}

static void asset_worker_run(AssetServer *server) {
    for (;;) {
        AssetJob job;
        char *relative_path = NULL;

        asset_mutex_lock(&server->mutex);
        while (!server->stopping && server->job_count == 0u) {
            asset_condition_wait(&server->condition, &server->mutex);
        }
        if (server->stopping) {
            asset_mutex_unlock(&server->mutex);
            return;
        }

        job = asset_queue_pop(server);
        if (job.slot >= server->slot_count) {
            asset_mutex_unlock(&server->mutex);
            continue;
        }
        AssetSlot *slot = &server->slots[job.slot];
        if (!slot->used || slot->generation != job.generation) {
            asset_mutex_unlock(&server->mutex);
            continue;
        }

        relative_path = asset_string_duplicate(slot->path);
        if (!relative_path) {
            slot->status = slot->has_texture ? ASSET_STATUS_READY : ASSET_STATUS_FAILED;
            (void)snprintf(slot->error, sizeof(slot->error),
                           "out of memory while starting asset load");
            server->stats.failed_loads++;
            asset_mutex_unlock(&server->mutex);
            continue;
        }

        if (!slot->has_texture) {
            slot->status = ASSET_STATUS_LOADING;
        }
        server->active_workers++;
        asset_mutex_unlock(&server->mutex);

        char *full_path = asset_make_full_path(server, relative_path);
        free(relative_path);
        Image decoded = { 0 };
        if (full_path) {
            /* LoadImage performs CPU-side decoding only; GPU upload is main-thread-only. */
            decoded = LoadImage(full_path);
            free(full_path);
        }
        const bool decode_ok = decoded.data != NULL &&
                               decoded.width > 0 && decoded.height > 0;

        asset_mutex_lock(&server->mutex);
        server->active_workers--;
        if (job.slot < server->slot_count) {
            slot = &server->slots[job.slot];
            if (slot->used && slot->generation == job.generation) {
                if (decode_ok) {
                    if (slot->pending_image_valid) {
                        UnloadImage(slot->pending_image);
                    }
                    slot->pending_image = decoded;
                    slot->pending_image_valid = true;
                    slot->status = slot->has_texture
                        ? ASSET_STATUS_RELOADING : ASSET_STATUS_CPU_READY;
                    slot->error[0] = '\0';
                    if (!asset_completion_push_locked(server, job.slot,
                                                       job.generation)) {
                        server->completion_overflow = true;
                    }
                    decoded.data = NULL;
                } else {
                    slot->status = slot->has_texture
                        ? ASSET_STATUS_READY : ASSET_STATUS_FAILED;
                    (void)snprintf(slot->error, sizeof(slot->error),
                                   "could not decode texture file (missing or unsupported image)");
                    server->stats.failed_loads++;
                }
            }
        }
        asset_mutex_unlock(&server->mutex);

        if (decoded.data) {
            UnloadImage(decoded);
        }
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

static AssetServer *asset_server_get(const ecs_world_t *world) {
    if (!world || ecs_id(AssetServerState) == 0) {
        return NULL;
    }
    const AssetServerState *state = ecs_get(
        world, ecs_id(AssetServerState), AssetServerState);
    return state ? state->implementation : NULL;
}

void AssetServerModuleImport(ecs_world_t *world) {
    ECS_MODULE(world, AssetServerModule);
    ECS_COMPONENT_DEFINE(world, AssetServerState);
}

AssetServerConfig asset_server_default_config(void) {
    AssetServerConfig config;
    config.root_directory = "data";
    config.worker_count = 0u;
    config.queue_capacity = 2048u;
    return config;
}

static uint32_t asset_default_worker_count(void) {
#if defined(_WIN32)
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    const uint32_t processors = (uint32_t)info.dwNumberOfProcessors;
#else
    const long detected = sysconf(_SC_NPROCESSORS_ONLN);
    const uint32_t processors = detected > 0 ? (uint32_t)detected : 2u;
#endif
    uint32_t workers = processors > 1u ? processors - 1u : 1u;
    if (workers > 8u) {
        workers = 8u;
    }
    return workers;
}

bool asset_server_init(ecs_world_t *world, const AssetServerConfig *config) {
    if (!world || ecs_id(AssetServerState) == 0 || asset_server_get(world)) {
        return false;
    }

    const AssetServerConfig selected = config
        ? *config : asset_server_default_config();
    const char *root = selected.root_directory && selected.root_directory[0]
        ? selected.root_directory : "data";
    const uint32_t worker_count = selected.worker_count
        ? selected.worker_count : asset_default_worker_count();
    const uint32_t queue_capacity = selected.queue_capacity
        ? selected.queue_capacity : 2048u;

    AssetServer *server = (AssetServer *)calloc(1u, sizeof(AssetServer));
    if (!server) {
        return false;
    }
    server->free_head = UINT32_MAX;
    server->root_directory = asset_string_duplicate(root);
    server->job_capacity = queue_capacity;
    server->jobs = (AssetJob *)calloc(queue_capacity, sizeof(AssetJob));
    server->workers = (AssetThread *)calloc(worker_count, sizeof(AssetThread));
    if (!server->root_directory || !server->jobs || !server->workers ||
        !asset_mutex_init(&server->mutex)) {
        free(server->root_directory);
        free(server->jobs);
        free(server->workers);
        free(server);
        return false;
    }
    if (!asset_condition_init(&server->condition)) {
        asset_mutex_destroy(&server->mutex);
        free(server->root_directory);
        free(server->jobs);
        free(server->workers);
        free(server);
        return false;
    }
    server->synchronization_ready = true;

    for (uint32_t i = 0u; i < worker_count; i++) {
#if defined(_WIN32)
        server->workers[i] = CreateThread(NULL, 0u, asset_worker_entry,
                                          server, 0u, NULL);
        if (!server->workers[i]) {
            break;
        }
#else
        if (pthread_create(&server->workers[i], NULL,
                           asset_worker_entry, server) != 0) {
            break;
        }
#endif
        server->worker_count++;
    }

    if (server->worker_count == 0u) {
        asset_condition_destroy(&server->condition);
        asset_mutex_destroy(&server->mutex);
        free(server->root_directory);
        free(server->jobs);
        free(server->workers);
        free(server->completed);
        free(server);
        return false;
    }

    ecs_set(world, ecs_id(AssetServerState), AssetServerState,
            { .implementation = server });
    return true;
}

static void asset_slot_evict_locked(AssetServer *server, uint32_t index) {
    AssetSlot *slot = &server->slots[index];
    if (!slot->used) {
        return;
    }

    asset_map_remove(server, index);
    free(slot->path);
    slot->path = NULL;
    if (slot->pending_image_valid) {
        UnloadImage(slot->pending_image);
        slot->pending_image_valid = false;
    }
    if (slot->has_texture) {
        UnloadTexture(slot->texture);
        slot->has_texture = false;
    }
    slot->used = false;
    slot->status = ASSET_STATUS_INVALID;
    slot->reference_count = 0u;
    slot->generation++;
    if (slot->generation == 0u) {
        slot->generation = 1u;
    }
    slot->next_free = server->free_head;
    server->free_head = index;
}

void asset_server_fini(ecs_world_t *world) {
    AssetServer *server = asset_server_get(world);
    if (!server) {
        return;
    }

    /* Detach first: any ECS OnRemove callback during ecs_fini becomes a no-op. */
    ecs_set(world, ecs_id(AssetServerState), AssetServerState,
            { .implementation = NULL });

    asset_mutex_lock(&server->mutex);
    server->stopping = true;
    asset_condition_broadcast(&server->condition);
    asset_mutex_unlock(&server->mutex);

    for (uint32_t i = 0u; i < server->worker_count; i++) {
#if defined(_WIN32)
        (void)WaitForSingleObject(server->workers[i], INFINITE);
        CloseHandle(server->workers[i]);
#else
        (void)pthread_join(server->workers[i], NULL);
#endif
    }

    for (uint32_t i = 0u; i < server->slot_count; i++) {
        AssetSlot *slot = &server->slots[i];
        if (slot->pending_image_valid) {
            UnloadImage(slot->pending_image);
        }
        if (slot->has_texture) {
            UnloadTexture(slot->texture);
        }
        free(slot->path);
    }

    free(server->slots);
    free(server->map);
    free(server->jobs);
    free(server->completed);
    free(server->workers);
    free(server->root_directory);
    if (server->synchronization_ready) {
        asset_condition_destroy(&server->condition);
        asset_mutex_destroy(&server->mutex);
    }
    free(server);
}

AssetHandle asset_texture_acquire(ecs_world_t *world,
                                  const char *relative_path,
                                  AssetPriority priority) {
    AssetServer *server = asset_server_get(world);
    if (!server) {
        return ASSET_HANDLE_INVALID;
    }

    char *normalized_path = asset_normalize_path(relative_path);
    if (!normalized_path) {
        return ASSET_HANDLE_INVALID;
    }

    asset_mutex_lock(&server->mutex);
    server->stats.requests++;
    const int32_t existing = asset_map_find(server, normalized_path);
    if (existing >= 0) {
        AssetSlot *slot = &server->slots[(uint32_t)existing];
        slot->reference_count++;
        server->stats.cache_hits++;
        AssetHandle handle = { (uint32_t)existing, slot->generation };

        if (slot->status == ASSET_STATUS_FAILED &&
            asset_queue_push(server, (uint32_t)existing, (int32_t)priority)) {
            slot->status = ASSET_STATUS_QUEUED;
            slot->error[0] = '\0';
        }
        asset_mutex_unlock(&server->mutex);
        free(normalized_path);
        return handle;
    }

    const uint32_t slot_index = asset_slot_allocate(server);
    if (slot_index == UINT32_MAX) {
        asset_mutex_unlock(&server->mutex);
        free(normalized_path);
        return ASSET_HANDLE_INVALID;
    }

    AssetSlot *slot = &server->slots[slot_index];
    slot->path = normalized_path;
    slot->reference_count = 1u;
    slot->status = ASSET_STATUS_QUEUED;

    if (!asset_map_insert(server, slot_index) ||
        !asset_queue_push(server, slot_index, (int32_t)priority)) {
        if (slot->path) {
            asset_map_remove(server, slot_index);
            free(slot->path);
            slot->path = NULL;
        }
        slot->used = false;
        slot->generation++;
        if (slot->generation == 0u) {
            slot->generation = 1u;
        }
        slot->next_free = server->free_head;
        server->free_head = slot_index;
        asset_mutex_unlock(&server->mutex);
        return ASSET_HANDLE_INVALID;
    }

    const AssetHandle handle = { slot_index, slot->generation };
    asset_mutex_unlock(&server->mutex);
    return handle;
}

bool asset_retain(ecs_world_t *world, AssetHandle handle) {
    AssetServer *server = asset_server_get(world);
    if (!server) {
        return false;
    }

    asset_mutex_lock(&server->mutex);
    AssetSlot *slot = asset_slot_from_handle(server, handle);
    if (slot) {
        slot->reference_count++;
    }
    asset_mutex_unlock(&server->mutex);
    return slot != NULL;
}

void asset_release(ecs_world_t *world, AssetHandle handle) {
    AssetServer *server = asset_server_get(world);
    if (!server) {
        return;
    }

    asset_mutex_lock(&server->mutex);
    AssetSlot *slot = asset_slot_from_handle(server, handle);
    if (slot && slot->reference_count > 0u) {
        slot->reference_count--;
    }
    asset_mutex_unlock(&server->mutex);
}

bool asset_texture_reload(ecs_world_t *world, AssetHandle handle,
                          AssetPriority priority) {
    AssetServer *server = asset_server_get(world);
    if (!server) {
        return false;
    }

    asset_mutex_lock(&server->mutex);
    AssetSlot *slot = asset_slot_from_handle(server, handle);
    if (!slot || slot->status == ASSET_STATUS_QUEUED ||
        slot->status == ASSET_STATUS_LOADING ||
        slot->status == ASSET_STATUS_CPU_READY ||
        slot->status == ASSET_STATUS_RELOADING) {
        asset_mutex_unlock(&server->mutex);
        return false;
    }

    const AssetStatus old_status = slot->status;
    slot->status = slot->has_texture
        ? ASSET_STATUS_RELOADING : ASSET_STATUS_QUEUED;
    slot->error[0] = '\0';
    if (!asset_queue_push(server, handle.index, (int32_t)priority)) {
        slot->status = old_status;
        asset_mutex_unlock(&server->mutex);
        return false;
    }
    server->stats.reloads++;
    asset_mutex_unlock(&server->mutex);
    return true;
}

uint32_t asset_server_pump(ecs_world_t *world, uint32_t max_uploads) {
    AssetServer *server = asset_server_get(world);
    if (!server) {
        return 0u;
    }

    uint32_t uploads = 0u;
    for (;;) {
        Image image = { 0 };
        uint32_t slot_index = UINT32_MAX;
        uint32_t generation = 0u;

        asset_mutex_lock(&server->mutex);
        const bool has_completed = asset_completion_take_locked(
            server, &image, &slot_index, &generation);
        asset_mutex_unlock(&server->mutex);
        if (!has_completed) {
            break;
        }

        /* This is intentionally on the render thread: raylib creates a GPU object. */
        const Texture2D replacement = LoadTextureFromImage(image);
        UnloadImage(image);
        const bool upload_ok = replacement.id != 0u;
        Texture2D previous = { 0 };
        bool discard_replacement = false;

        asset_mutex_lock(&server->mutex);
        AssetSlot *slot = slot_index < server->slot_count
            ? &server->slots[slot_index] : NULL;
        if (!slot || !slot->used || slot->generation != generation) {
            discard_replacement = upload_ok;
        } else if (upload_ok) {
            if (slot->has_texture) {
                previous = slot->texture;
            }
            slot->texture = replacement;
            slot->has_texture = true;
            slot->status = ASSET_STATUS_READY;
            slot->error[0] = '\0';
            server->stats.successful_loads++;
        } else {
            slot->status = slot->has_texture
                ? ASSET_STATUS_READY : ASSET_STATUS_FAILED;
            (void)snprintf(slot->error, sizeof(slot->error),
                           "raylib could not create GPU texture");
            server->stats.failed_loads++;
        }
        asset_mutex_unlock(&server->mutex);

        if (previous.id != 0u) {
            UnloadTexture(previous);
        }
        if (discard_replacement) {
            UnloadTexture(replacement);
        }

        uploads++;
        if (max_uploads != 0u && uploads >= max_uploads) {
            break;
        }
    }
    return uploads;
}

const Texture2D *asset_texture_resolve(const AssetServer *server,
                                       AssetHandle handle) {
    if (!server || !ASSET_HANDLE_IS_VALID(handle) ||
        handle.index >= server->slot_count) {
        return NULL;
    }

    /* Slot identity and GPU texture fields are mutated only on the world thread. */
    const AssetSlot *slot = &server->slots[handle.index];
    if (!slot->used || slot->generation != handle.generation ||
        !slot->has_texture) {
        return NULL;
    }
    return &slot->texture;
}

const Texture2D *asset_texture_get(ecs_world_t *world, AssetHandle handle) {
    return asset_texture_resolve(asset_server_get(world), handle);
}

AssetStatus asset_status(ecs_world_t *world, AssetHandle handle) {
    AssetServer *server = asset_server_get(world);
    if (!server) {
        return ASSET_STATUS_INVALID;
    }

    asset_mutex_lock(&server->mutex);
    AssetSlot *slot = asset_slot_from_handle(server, handle);
    const AssetStatus status = slot ? slot->status : ASSET_STATUS_INVALID;
    asset_mutex_unlock(&server->mutex);
    return status;
}

bool asset_error_copy(ecs_world_t *world, AssetHandle handle,
                      char *destination, size_t destination_size) {
    AssetServer *server = asset_server_get(world);
    if (!server || !destination || destination_size == 0u) {
        return false;
    }

    asset_mutex_lock(&server->mutex);
    AssetSlot *slot = asset_slot_from_handle(server, handle);
    if (slot) {
        (void)snprintf(destination, destination_size, "%s", slot->error);
    } else {
        destination[0] = '\0';
    }
    asset_mutex_unlock(&server->mutex);
    return slot != NULL;
}

void asset_server_get_stats(ecs_world_t *world, AssetServerStats *out_stats) {
    if (!out_stats) {
        return;
    }
    memset(out_stats, 0, sizeof(*out_stats));

    AssetServer *server = asset_server_get(world);
    if (!server) {
        return;
    }

    asset_mutex_lock(&server->mutex);
    *out_stats = server->stats;
    out_stats->queued_jobs = server->job_count;
    out_stats->active_workers = server->active_workers;
    out_stats->cached_textures = 0u;
    out_stats->texture_bytes = 0u;
    for (uint32_t i = 0u; i < server->slot_count; i++) {
        const AssetSlot *slot = &server->slots[i];
        if (slot->used && slot->has_texture) {
            out_stats->cached_textures++;
            out_stats->texture_bytes += (uint64_t)slot->texture.width *
                                        (uint64_t)slot->texture.height * 4u;
        }
    }
    asset_mutex_unlock(&server->mutex);
}

uint32_t asset_server_collect_unused(ecs_world_t *world) {
    AssetServer *server = asset_server_get(world);
    if (!server) {
        return 0u;
    }

    uint32_t evicted = 0u;
    asset_mutex_lock(&server->mutex);
    for (uint32_t i = 0u; i < server->slot_count; i++) {
        AssetSlot *slot = &server->slots[i];
        if (slot->used && slot->reference_count == 0u) {
            asset_slot_evict_locked(server, i);
            evicted++;
        }
    }

    if (server->map_capacity > 64u &&
        server->map_count * 10u < server->map_capacity * 2u) {
        uint32_t target = 64u;
        while (target < server->map_count * 2u) {
            target *= 2u;
        }
        (void)asset_map_rehash(server, target);
    } else if (server->map_tombstones > server->map_count &&
               server->map_tombstones > 16u) {
        (void)asset_map_rehash(server, server->map_capacity);
    }
    asset_mutex_unlock(&server->mutex);
    return evicted;
}
