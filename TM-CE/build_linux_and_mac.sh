#!/usr/bin/env bash

set -eo pipefail
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
XDELTA_PATCH_PATH="$SCRIPT_DIR/patch.xdelta"
if [[ ! -f "$SCRIPT_DIR/release-config.sh" ]]; then
    echo "ERROR: Missing release-config.sh. Download the complete Tyro release archive."
    exit 1
fi
source "$SCRIPT_DIR/release-config.sh"

if command -v xdelta3 > /dev/null; then
    XDELTA_CMD="xdelta3"
elif command -v xdelta > /dev/null; then
    XDELTA_CMD="xdelta"
else
    echo "ERROR: xdelta is not installed. Please install xdelta or xdelta3 through your package manager."
    exit 1
fi

if [[ -z "${1}" ]]; then
    echo "ERROR: ISO was not passed"
    exit 1
fi

if [[ ! -f "${1}" ]]; then
    echo "ERROR: ISO '${1}' is not a valid file"
    exit 1
fi

if ! ${XDELTA_CMD} -f -d -s "${1}" "${XDELTA_PATCH_PATH}" "$SCRIPT_DIR/$OUTPUT_ISO"; then
    echo "ERROR: The ISO '${1}' is not a valid source Melee iso"
    exit 1
fi

echo "############ $OUTPUT_ISO has been successfully created ######################"

if [[ ! -t 0 ]]; then
    echo "Press Enter to continue..."
    read -r || true
fi
