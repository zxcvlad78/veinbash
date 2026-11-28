#pragma once

#include <flecs.h>
#include <raylib.h>

namespace vb {
namespace base {

struct module {
    module(flecs::world &world);
};

} // namespace base
} // namespace vb
