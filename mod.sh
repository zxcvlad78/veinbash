set -e

MOD_INCLUDE_DIR="include/vb/modules/"
MOD_SRC_DIR="src/modules/"

usage() {
    cat <<EOF
Usage:
    $0 new <mod-name>
    $0 -h | --help | help
EOF
}

cmd_new() {
    if [ "$#" -ne 1 ] || [ -z "$1" ]; then
        usage
    fi

    mod_name="$1"
    mkdir -p "$MOD_INCLUDE_DIR/$mod_name"
    cd "$MOD_INCLUDE_DIR/$mod_name"
    touch mod.h
    
    cd "../../../.."

    mkdir -p "$MOD_SRC_DIR/$mod_name"
    cd "$MOD_SRC_DIR/$mod_name"
    touch mod.c
    echo "new module: '$mod_name'"
}

command="$1"
shift

case "$command" in
    new)
        cmd_new "$@"
        ;;
    -h|--help|help)
        usage
        exit 0
        ;;
    *)
        echo "unknown command: '$command'" >&2
        usage >&2
        exit 1
        ;;
esac
