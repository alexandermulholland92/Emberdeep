#!/bin/sh
# Build emberdeep.vpk locally using the official VitaSDK image.
# Requires only Docker. Run from the project root:  sh tools/build-in-docker.sh
set -eu

IMAGE="vitasdk/vitasdk:latest"

echo "==> pulling $IMAGE (first run only, ~1.6 GB)"
docker pull "$IMAGE"

echo "==> building"
docker run --rm -v "$PWD:/workspace" -w /workspace "$IMAGE" sh -c '
  set -eu
  if [ ! -f "$VITASDK/arm-vita-eabi/lib/libvitaGL.a" ]; then
    echo "--> vitaGL not in image, building from source"
    apt-get update >/dev/null
    apt-get install -y --no-install-recommends git build-essential >/dev/null
    git clone --depth 1 https://github.com/Rinnegatamante/vitaGL /tmp/vitaGL
    make -C /tmp/vitaGL -j"$(nproc)" install
  fi
  make
'

echo ""
echo "==> done: emberdeep.vpk"
