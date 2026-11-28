#!/bin/sh
set -eu

usage() {
    cat <<EOF
Usage:
    $0 new <mod_name>
    $0 rm  <mod_name>
    $0 -h | --help | help
EOF
}

to_pascal() {
    printf '%s\n' "$1" | awk -F_ '{
        for (i = 1; i <= NF; i++)
            printf "%s%s", toupper(substr($i, 1, 1)), substr($i, 2)
        print ""
    }'
}

cmd_rm() {
    if [ "$#" -ne 1 ] || [ -z "$1" ]; then
        usage >&2
        exit 1
    fi

    mod_name="$1"

    case "$mod_name" in
        .|..|*/*|*\\*)
            echo "invalid module name: '$mod_name'" >&2
            exit 1
            ;;
    esac

    mod_src_dir="modules/$mod_name"

    if [ ! -d "$mod_src_dir" ]; then
        echo "module not found: '$mod_name'" >&2
        exit 1
    fi

    modules_root=$(cd modules 2>/dev/null && pwd -P) || {
        echo "no 'modules/' directory" >&2
        exit 1
    }
    target=$(cd "$mod_src_dir" && pwd -P)

    case "$target" in
        "$modules_root"/*) ;;
        *)
            echo "refusing to remove outside modules/: '$target'" >&2
            exit 1
            ;;
    esac

    rm -rf -- "$mod_src_dir"
    echo "removed module: '$mod_name'"
}

cmd_new() {
    if [ "$#" -ne 1 ] || [ -z "$1" ]; then
        usage >&2
        exit 1
    fi

    mod_name="$1"
    mod_pascal=$(to_pascal "$mod_name")
    mod_src_dir="modules/$mod_name"
    mod_include_dir="$mod_src_dir/include/modules/$mod_name"

    mkdir -p "$mod_include_dir"

    cat > "$mod_include_dir/mod.h" <<EOF
#pragma once
#include <flecs.h>
#include <raylib.h>

// extern ECS_COMPONENT_DECLARE();

void ${mod_pascal}ModuleImport(ecs_world_t *world);

EOF

    cat > "$mod_src_dir/mod.cpp" <<EOF
#include "modules/${mod_name}/mod.h"

void ${mod_pascal}ModuleImport(ecs_world_t *world) {
    ECS_MODULE(world, ${mod_pascal}Module);

    // ECS_COMPONENT_DEFINE(world, );

    // ECS_SYSTEM(world, , );
}
EOF

    cat > "$mod_src_dir/CMakeLists.txt" <<EOF
add_library(vb_${mod_name} STATIC mod.c)

target_include_directories(vb_${mod_name} PUBLIC
    \${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(vb_${mod_name} PUBLIC

)
EOF

    echo "new module: '$mod_name'"
}

if [ "$#" -lt 1 ]; then
    usage >&2
    exit 1
fi

subcmd="$1"
shift

case "$subcmd" in
    new)
        cmd_new "$@"
        ;;
    rm)
        cmd_rm "$@"
        ;;
    -h|--help|help)
        usage
        exit 0
        ;;
    *)
        echo "unknown command: '$subcmd'" >&2
        usage >&2
        exit 1
        ;;
esac
