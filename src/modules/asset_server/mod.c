#include <vb/modules/asset_server/mod.h>

void AssetServerModuleImport(ecs_world_t *world) {
  ECS_MODULE(world, AssetServerModule);
}
