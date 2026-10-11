#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
if command -v cmake >/dev/null 2>&1; then
  cmake -S "$ROOT/backend" -B "$ROOT/backend/build"
  cmake --build "$ROOT/backend/build"
else
  make -C "$ROOT/backend"
fi
python3 -m pip install -q -r "$ROOT/frontend/requirements.txt"
exec python3 "$ROOT/frontend/main.py"
