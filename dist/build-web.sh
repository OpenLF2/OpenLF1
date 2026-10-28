#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 OpenLF1

set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$repo_root/build/web"
docker run --rm --user "$(id -u):$(id -g)" -e HOME=/tmp \
    -v "$repo_root:/work" -w /work emscripten/emsdk:5.0.2 bash -euc '
        emcmake cmake -S . -B build/web-build -G "Unix Makefiles" \
            -DCMAKE_BUILD_TYPE=Release \
            -DOPENLF1_BUNDLE_DEPENDENCIES=ON
        cmake --build build/web-build --target openlf1 --parallel 4
    '

rm -rf "$build_dir"
mkdir -p "$build_dir"
cp "$repo_root/build/web-build/openlf1.js" "$repo_root/build/web-build/openlf1.wasm" "$build_dir/"
cp "$repo_root/web/index.html" "$repo_root/web/game-store.js" "$build_dir/"
cp "$repo_root/packaging/openlf1.svg" "$build_dir/icon.svg"
cp "$repo_root/LICENSE" "$build_dir/LICENSE"
cp "$repo_root/THIRD-PARTY-NOTICES.md" "$build_dir/THIRD-PARTY-NOTICES.md"

rm -f "$repo_root/build/openlf1-web.zip"
(
    cd "$build_dir"
    python3 -m zipfile -c ../openlf1-web.zip index.html game-store.js icon.svg \
        openlf1.js openlf1.wasm LICENSE THIRD-PARTY-NOTICES.md
)
printf 'Web bundle: %s\n' "$repo_root/build/openlf1-web.zip"
