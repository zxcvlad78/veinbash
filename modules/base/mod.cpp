#include <modules/base/mod.h>
#include <modules/base/components.h>

namespace vb {
namespace base {

module::module(flecs::world &world) {
    world.module<module>();

    world.component<Transform2D>();
}

} // namespace base
} // namespace vb
