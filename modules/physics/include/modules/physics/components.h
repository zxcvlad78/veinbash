#pragma once
#include <raylib.h>
#include <flecs.h>

namespace vb {
namespace physics {

struct FixedUpdate {};

struct Velocity {
    Vector2 linear{0.0f, 0.0f};
    float angular = 0.0f;
};

struct WishVelocity {
    Vector2 linear{0.0f, 0.0f};
    float angular = 0.0f;
};

struct RectCollider {
    Vector2 size = { 0.0f, 0.0f };
};

struct CollisionEvent { flecs::entity a; flecs::entity b; };

}
}