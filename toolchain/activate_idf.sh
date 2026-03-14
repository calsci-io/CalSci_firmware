#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
IDF_DIR="${SCRIPT_DIR}/esp-idf"
TOOLS_DIR="${SCRIPT_DIR}/espressif"

if [ ! -f "${IDF_DIR}/export.sh" ]; then
    echo "ERROR: ESP-IDF export.sh not found at ${IDF_DIR}/export.sh" >&2
    exit 1
fi

if [ ! -d "${TOOLS_DIR}" ]; then
    echo "ERROR: ESP-IDF tools directory not found at ${TOOLS_DIR}" >&2
    exit 1
fi

export IDF_TOOLS_PATH="${TOOLS_DIR}"

# shellcheck disable=SC1091
. "${IDF_DIR}/export.sh"

echo "ESP-IDF toolchain activated from ${IDF_DIR}"
echo "ESP-IDF tools path: ${IDF_TOOLS_PATH}"
