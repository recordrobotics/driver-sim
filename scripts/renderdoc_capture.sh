#!/usr/bin/env bash
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "Usage: renderdoc_capture.sh <exe-path>" >&2
  exit 2
fi

exe_path="$1"
if [ ! -f "$exe_path" ]; then
  echo "Executable not found: $exe_path" >&2
  exit 1
fi

capture_dir="$(cd "$(dirname "$exe_path")" && pwd)/temp"

renderdoc_cmd="${RENDERDOCCMD:-${RENDERDOC_CMD:-}}"
if [ -z "$renderdoc_cmd" ]; then
  renderdoc_cmd="$(command -v renderdoccmd || true)"
fi
if [ -z "$renderdoc_cmd" ]; then
  for candidate in \
    /usr/bin/renderdoccmd \
    /usr/local/bin/renderdoccmd \
    /Applications/RenderDoc.app/Contents/MacOS/renderdoccmd \
    "$HOME/Applications/RenderDoc.app/Contents/MacOS/renderdoccmd"; do
    if [ -x "$candidate" ]; then
      renderdoc_cmd="$candidate"
      break
    fi
  done
fi
if [ -z "$renderdoc_cmd" ]; then
  echo "renderdoccmd not found. Set RENDERDOCCMD/RENDERDOC_CMD or install RenderDoc." >&2
  exit 1
fi

qrenderdoc="${QRENDERDOC:-}"
if [ -z "$qrenderdoc" ]; then
  qrenderdoc="$(command -v qrenderdoc || true)"
fi
if [ -z "$qrenderdoc" ]; then
  for candidate in \
    /usr/bin/qrenderdoc \
    /usr/local/bin/qrenderdoc \
    /Applications/RenderDoc.app/Contents/MacOS/qrenderdoc \
    "$HOME/Applications/RenderDoc.app/Contents/MacOS/qrenderdoc"; do
    if [ -x "$candidate" ]; then
      qrenderdoc="$candidate"
      break
    fi
  done
fi
if [ -z "$qrenderdoc" ]; then
  echo "qrenderdoc not found. Set QRENDERDOC or install RenderDoc." >&2
  exit 1
fi

"$renderdoc_cmd" capture -w "$exe_path"

latest_rdc=""
if [ -d "$capture_dir" ]; then
  latest_rdc="$(ls -t "$capture_dir"/*.rdc 2>/dev/null | head -n 1 || true)"
fi
if [ -z "$latest_rdc" ]; then
  echo "No .rdc captures found in $capture_dir." >&2
  exit 1
fi

"$qrenderdoc" "$latest_rdc" >/dev/null 2>&1 &
