#!/usr/bin/env bash
set -euo pipefail

WS_ARG=""
APPLY_SHELL="0"

# Args: --workspace <path>  and/or  --apply-shell
while [ $# -gt 0 ]; do
  case "$1" in
    --workspace) WS_ARG="${2:-}"; shift 2;;
    --apply-shell) APPLY_SHELL="1"; shift;;
    *) echo "Unknown arg: $1" >&2; exit 2;;
  esac
done

pick_port() {
  # Prefer stable symlinks (do NOT resolve them)
  if compgen -G "/dev/serial/by-id/*" > /dev/null; then
    # Prefer Espressif first
    for p in /dev/serial/by-id/*; do
      case "$p" in *Espressif*) echo "$p"; return 0;; esac
    done
    ls -1 /dev/serial/by-id/* | head -n1
    return 0
  fi
  # Fallback to raw tty names
  for g in "/dev/ttyACM*" "/dev/ttyUSB*"; do
    if compgen -G "$g" > /dev/null; then
      ls -1 $g 2>/dev/null | head -n1
      return 0
    fi
  done
  return 1
}

# Find workspace dir:
resolve_ws() {
  # Priority: explicit arg → VS Code vars → smart guess in /workspaces
  if [ -n "${WS_ARG}" ]; then echo "${WS_ARG}"; return; fi
  if [ -n "${containerWorkspaceFolder:-}" ]; then echo "${containerWorkspaceFolder}"; return; fi
  if [ -n "${WORKSPACE_FOLDER:-}" ]; then echo "${WORKSPACE_FOLDER}"; return; fi
  # Guess: if there’s exactly one folder in /workspaces, use it
  if [ -d /workspaces ]; then
    candidates=(/workspaces/*)
    if [ ${#candidates[@]} -eq 1 ] && [ -d "${candidates[0]}" ]; then
      echo "${candidates[0]}"; return
    fi
    # Otherwise prefer a folder that has .devcontainer or .vscode
    for d in /workspaces/*; do
      [ -d "$d" ] || continue
      if [ -d "$d/.devcontainer" ] || [ -d "$d/.vscode" ]; then
        echo "$d"; return
      fi
    done
  fi
  # Fallback: current dir
  pwd
}

PORT=""
if PORT="$(pick_port)"; then
  echo "[serial_detect] Selected port: $PORT"
  # Persist for NEW shells
  install -m 0644 /dev/null /etc/profile.d/esptool_port.sh
  printf 'export ESPTOOL_PORT=\"%s\"\n' "$PORT" > /etc/profile.d/esptool_port.sh

  # Optionally export into this *current* shell (useful on postAttach)
  if [ "$APPLY_SHELL" = "1" ]; then
    export ESPTOOL_PORT="$PORT"
  fi

  WS="$(resolve_ws)"
  mkdir -p "$WS/.vscode"
  SETTINGS="$WS/.vscode/settings.json"

  python3 - "$SETTINGS" "$PORT" <<'PY'
import json, os, sys
settings_path, port = sys.argv[1], sys.argv[2]
data = {}
if os.path.exists(settings_path):
    try:
        with open(settings_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
    except Exception:
        data = {}
data["idf.port"] = port
os.makedirs(os.path.dirname(settings_path), exist_ok=True)
tmp = settings_path + ".tmp"
with open(tmp, 'w', encoding='utf-8') as f:
    json.dump(data, f, indent=2)
os.replace(tmp, settings_path)
PY

else
  echo "[detect-serial] No serial device found yet."
  echo "  (WSL) Use: usbipd list ; usbipd attach --wsl --busid <BUSID>"
fi
