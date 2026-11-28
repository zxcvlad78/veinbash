#pragma once
#include <raylib.h>

namespace vb {
namespace base {

struct Transform2D {
    Vector2 translation{0.0f, 0.0f};
    Vector2 scale{1.0f, 1.0f};
    float rotation = 0.0f;
};

} // namespace base
} // namespace vb
