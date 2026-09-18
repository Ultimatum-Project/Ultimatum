#!/usr/bin/env bash
set -euo pipefail

script_directory="$(cd "$(dirname "$0")" && pwd)"
web_directory="$(cd "$script_directory/.." && pwd)"
server_port=${ULTIMATUM_WEB_PORT:-4173}
serve_directory=${ULTIMATUM_WEB_SERVE_DIRECTORY:-$web_directory/.cache/local-dist}
lan_address=""

if [ ! -f "$serve_directory/index.html" ]; then
    echo "ERROR: configured client not found. Run npm run configure:local first." >&2
    exit 1
fi

if command -v route >/dev/null 2>&1 && command -v ipconfig >/dev/null 2>&1; then
    network_interface=$(route -n get default 2>/dev/null | awk '/interface:/{print $2; exit}' || true)
    if [[ -n "$network_interface" ]]; then
        lan_address=$(ipconfig getifaddr "$network_interface" 2>/dev/null || true)
    fi
fi

printf 'Ultimatum web client\n'
printf '  This computer: http://localhost:%s\n' "$server_port"
if [[ -n "$lan_address" ]]; then
    printf '  Same network: http://%s:%s\n' "$lan_address" "$server_port"
else
    printf "  Same network: use this computer's LAN address on port %s\n" "$server_port"
fi

python_command=${PYTHON:-python3}
if ! command -v "$python_command" >/dev/null 2>&1; then python_command=python; fi
exec "$python_command" -m http.server "$server_port" \
    --bind 0.0.0.0 \
    --directory "$serve_directory"
