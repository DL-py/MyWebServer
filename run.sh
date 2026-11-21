#!/usr/bin/env bash
# Run the server from the devel directory with local shared libraries.
set -euo pipefail

root="$(cd "$(dirname "$0")" && pwd)"
devel="$root/devel"

if [[ ! -x "$devel/myhttp" ]]; then
    echo "missing $devel/myhttp; build first: cmake --build build -j4" >&2
    exit 1
fi

if [[ ! -f "$root/etc/common.cfg" ]]; then
    cp "$root/etc/common.cfg.example" "$root/etc/common.cfg"
    echo "created etc/common.cfg from etc/common.cfg.example" >&2
fi

cd "$devel"

project_libs="$(find lib -type d -not -path 'lib/vendor*' | paste -sd:)"
export LD_LIBRARY_PATH="${project_libs}:$(pwd)/lib/vendor${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

exec ./myhttp "$@"
