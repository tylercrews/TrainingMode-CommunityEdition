#!/usr/bin/env bash

# Shared build/release metadata. The editable values live in root-level version.h.
tyro_project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

tyro_read_define() {
    sed -nE "s/^#define[[:space:]]+${1}[[:space:]]+\"([^\"]*)\"[[:space:]]*$/\1/p" \
        "${tyro_project_root}/version.h"
}

tyro_version="$(tyro_read_define TM_VERSION)"
tyro_game_id="$(tyro_read_define TM_GAME_ID)"
tyro_game_name="$(tyro_read_define TM_GAME_NAME)"

if [[ ! "${tyro_version}" =~ ^V[0-9]+\.[0-9]+\.[0-9]+T[0-9]+$ ]] || \
   [[ ${#tyro_version} -gt 22 ]]; then
    echo "Error: TM_VERSION in version.h must look like V1.4.1T2 (at most 22 characters)." >&2
    return 1
fi
if [[ ! "${tyro_game_id}" =~ ^[A-Z0-9]{6}$ ]] || \
   [[ "${tyro_game_id:0:4}" = GTME || "${tyro_game_id:0:4}" = GALE || "${tyro_game_id:0:4}" = GALJ ]]; then
    echo "Error: TM_GAME_ID must have six uppercase letters/digits and its own game code." >&2
    return 1
fi
if [[ ! "${tyro_game_name}" =~ ^[A-Za-z0-9\ ._-]+$ ]] || \
   [[ ${#tyro_game_name} -gt 31 ]]; then
    echo "Error: TM_GAME_NAME must be plain ASCII text, at most 31 characters." >&2
    return 1
fi

tyro_release_name="TM-Tyro-${tyro_version}"
tyro_output_iso="${tyro_release_name}.iso"
tyro_release_archive="${tyro_release_name}.zip"
tyro_disc_title="TM-Tyro ${tyro_version}"
tyro_banner_title="${tyro_game_name} ${tyro_version}"
tyro_metadata_stamp="${tyro_version}|${tyro_game_id}|${tyro_game_name}"

tyro_write_build_metadata() {
    local game_code_hex=""
    local char_code
    local i
    for ((i = 0; i < 4; i++)); do
        printf -v char_code '%02X' "'${tyro_game_id:i:1}"
        game_code_hex+="${char_code}"
    done

    mkdir -p "${tyro_project_root}/build"
    {
        printf '# Generated from version.h; do not edit.\n'
        printf '.set TyroGameCode, 0x%s\n' "${game_code_hex}"
        printf '.macro TyroSaveCaption\n'
        printf '    .ascii "%-32s"\n' "${tyro_game_name}"
        printf '    .asciz "Game Data %s"\n' "${tyro_version}"
        printf '    .balign 4\n.endm\n'
    } > "${tyro_project_root}/build/tyro-identity.s"

    # Change only banner text; preserve the existing RGB5A3 artwork and author credits.
    local banner="${tyro_project_root}/build/opening.bnr"
    if [[ "$(head -c 4 "${tyro_project_root}/opening.bnr")" != BNR1 ]] || \
       [[ "$(wc -c < "${tyro_project_root}/opening.bnr")" -ne 6496 ]]; then
        echo "Error: expected a 6496-byte BNR1 opening.bnr." >&2
        return 1
    fi
    cp "${tyro_project_root}/opening.bnr" "${banner}"
    tyro_write_banner_field "${banner}" 6176 32 "TM-Tyro ${tyro_version}"
    tyro_write_banner_field "${banner}" 6240 64 "${tyro_banner_title}"
    cp "${tyro_project_root}/GTME01.map" "${tyro_project_root}/build/${tyro_game_id}.map"
}

tyro_write_banner_field() {
    local banner="${1}" offset="${2}" width="${3}" value="${4}"
    if [[ ${#value} -ge ${width} ]]; then
        echo "Error: banner title is too long for its ${width}-byte field." >&2
        return 1
    fi
    dd if=/dev/zero of="${banner}" bs=1 seek="${offset}" count="${width}" conv=notrunc 2>/dev/null
    printf '%s' "${value}" | dd of="${banner}" bs=1 seek="${offset}" conv=notrunc 2>/dev/null
}

tyro_write_release_config() {
    local destination="${1}"
    printf 'set "OUTPUT_ISO=%s"\r\n' "${tyro_output_iso}" > "${destination}/release-config.bat"
    printf "OUTPUT_ISO='%s'\n" "${tyro_output_iso}" > "${destination}/release-config.sh"
}
