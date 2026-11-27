#include <modules/game/mod.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    ecs_world_t *world = ecs_init();
    ECS_IMPORT(world, GameModule);

    return 0;
}
