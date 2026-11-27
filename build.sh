set -e

OUT="out/build"
EXENAME="veinbash"
BUILDTYPE="Debug"
DELETECACHE=0
CONFIGURE=0
BUILD=0
RUN=0

usage() {
    cat <<EOF
Usage:
    $0 -o  | --out
    $0 -dc | --deletecache
    $0 -c  | --configure
    $0 -b  | --build
    $0 -r  | --run
    $0 br
    $0 cbr
    $0 -h| --help | help
EOF
}

if [ $# -eq 0 ]; then
    usage
    exit 0
fi

for command in "$@"; do
    case "$command" in
        -h|--help|help)
            usage
            exit 0
            ;;
        -dc|--deletecache) DELETECACHE=1 ;;
        -c|--configure) CONFIGURE=1 ;;
        -b|--build) BUILD=1 ;;
        -r|--run) RUN=1 ;;
        br) BUILD=1; RUN=1 ;;
        cbr) CONFIGURE=1; BUILD=1; RUN=1 ;;
        *)
            echo "unknown command: '$command'" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [ "$DELETECACHE" -eq 1 ]; then
    rm -f "$OUT"/CMakeCache.txt
fi

if [ "$CONFIGURE" -eq 1 ]; then
    cmake -S . -B "$OUT" -DCMAKE_BUILD_TYPE="$BUILDTYPE"
fi

if [ "$BUILD" -eq 1 ]; then
    cmake --build "$OUT" -j"$(nproc)"
fi

if [ "$RUN" -eq 1 ]; then
    ./"$OUT"/"$EXENAME"
fi
