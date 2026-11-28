#include <flecs.h>
#include <modules/game/mod.h>

int main(int argc, char *argv[]) {
    flecs::world world;
    world.import<vb::game::module>();
    return 0;
}
