#! /usr/bin/env bash
set -eo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/scripts/project-config.sh"
cd "${tyro_project_root}"
rm -rf -- build
rm -f -- "${tyro_output_iso}" "${tyro_release_archive}"
