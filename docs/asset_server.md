# AssetServer

AssetServer is the shared texture-loading service for veinbash. It is a C11
Flecs module and uses raylib's CPU image decoder plus render-thread texture
upload.

## Runtime model

1. asset_texture_acquire() normalizes a path relative to data/, looks it up in
   an open-addressed cache and returns a generation-checked AssetHandle.
2. Repeated requests for the same normalized path share one cache entry, one
   decode job and one Texture2D. Each acquire increments its reference count.
3. A bounded priority heap feeds a small worker pool. Workers only decode CPU
   Image data; they never access the Flecs world or create GPU resources.
4. asset_server_pump() transfers completed images into raylib Texture2D objects
   on the render thread. Its upload budget prevents a large asset burst from
   monopolizing one frame.
5. Sprite stores an AssetHandle. A Flecs EcsOnRemove observer releases the
   handle when the component is removed. Rendering resolves the handle each
   frame, so sprites can keep drawing the previous texture during a reload.

The cache uses a path hash table, a binary max-heap for priority scheduling,
reference counts and generation counters. asset_server_collect_unused()
explicitly evicts entries with zero references; queued/in-flight jobs for an
evicted generation are discarded safely when they complete.

## Initialization

Import the module before initializing it:

    ECS_IMPORT(world, AssetServerModule);

    AssetServerConfig config = asset_server_default_config();
    config.root_directory = "data";
    config.worker_count = 0; /* automatic, clamped to 1..8 */

    if (!asset_server_init(world, &config)) {
        /* handle initialization failure */
    }

Acquire a texture by a path relative to root_directory:

    AssetHandle atlas = asset_texture_acquire(
        world, "atlas.png", ASSET_PRIORITY_CRITICAL);

    /* Each Sprite that owns a reference must acquire or retain one reference. */
    ecs_set(world, entity, Sprite, {
        .texture = atlas,
        .rect = { 0.0f, 0.0f, 16.0f, 32.0f }
    });

Pump the server once per frame before the ECS render systems:

    (void)asset_server_pump(world, 4); /* at most four GPU uploads this frame */
    ecs_progress(world, GetFrameTime());

The render module resolves Sprite.texture with the lock-free
asset_texture_resolve() path and skips drawing until the first load is ready.
max_uploads == 0 removes the per-frame upload limit.

## Reload, status and memory

    asset_texture_reload(world, atlas, ASSET_PRIORITY_HIGH);
    AssetStatus state = asset_status(world, atlas);

    AssetServerStats stats;
    asset_server_get_stats(world, &stats);

    /* Call periodically or after unloading a level. Must run on render thread. */
    asset_server_collect_unused(world);

A successful reload replaces the GPU texture only after the new image has been
decoded and uploaded. If decoding or upload fails, the previous texture remains
usable and asset_error_copy() exposes the error for diagnostics. Reload is
explicit; this implementation does not yet include a filesystem watcher.

## Thread and lifetime contract

- Call Flecs-facing API functions from the thread that owns the ecs_world_t.
- Call asset_server_pump(), asset_server_collect_unused() and
  asset_server_fini() while the raylib graphics context is alive.
- Workers never touch ecs_world_t or raylib GPU APIs.
- Pair each successful asset_texture_acquire() with one asset_release().
  The Sprite removal observer performs this release for normal ECS ownership.
- Call asset_server_fini(world) before CloseWindow() and before ecs_fini(world).
  It stops and joins workers and unloads cached GPU textures.
- Paths must be relative. Absolute paths and .. path segments are rejected.

The current production loader is texture-focused because veinbash currently
renders atlas sprites. Binary, audio, font and model loaders can be added behind
the same handle/cache/scheduler model when those resource types enter the game.
