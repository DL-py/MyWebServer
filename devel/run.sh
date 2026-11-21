#!/usr/bin/env bash
# Run the server from the devel directory with the local shared libraries.
set -euo pipefail
cd "$(dirname "$0")"

project_libs="$(find lib -type d -not -path 'lib/vendor*' | paste -sd:)"
export LD_LIBRARY_PATH="${project_libs}:$(pwd)/lib/vendor${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

exec ./myhttp "$@"
