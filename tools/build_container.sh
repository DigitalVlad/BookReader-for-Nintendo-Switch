#!/bin/sh
# Reproducible Switch build using the official devkitPro toolchain image.
# CONTAINER_ENGINE may be docker or nerdctl. Run from the host/container VM.
set -eu
cd "$(dirname "$0")/.."
engine=${CONTAINER_ENGINE:-docker}
image=devkitpro/devkita64:20260219
"$engine" pull "$image"
# Rootless nerdctl maps container root to the invoking VM user.
# Docker uses an explicit host UID so generated files remain user-owned.
if [ "$engine" = nerdctl ]; then
    set --
else
    set -- --user "$(id -u):$(id -g)"
fi
"$engine" run --rm --network=none "$@" \
    --volume "$PWD:/work" --workdir /work "$image" \
    sh -c 'make clean && make -j4'
