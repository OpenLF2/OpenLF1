#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 OpenLF1

set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
web_zip="$repo_root/build/openlf1-web.zip"
site_dir="$repo_root/build/pages"

rebuild=0
if [ "${1:-}" = "--rebuild" ]; then
    rebuild=1
elif [ "$#" -gt 0 ]; then
    printf 'Usage: %s [--rebuild]\n' "$0" >&2
    exit 2
fi

if [ "$rebuild" -eq 1 ] || [ ! -f "$web_zip" ]; then
    bash "$repo_root/dist/build-web.sh"
fi

rm -rf "$site_dir"
mkdir -p "$site_dir"
python3 -m zipfile -e "$web_zip" "$site_dir"
rm "$site_dir/index.html"
cp "$repo_root/dist/pages/index.html" "$site_dir/index.html"
printf 'Pages site: %s\n' "$site_dir"
