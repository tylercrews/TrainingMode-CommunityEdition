#! /usr/bin/env bash

set -Eeo pipefail

source scripts/project-config.sh

if [ "${1}" = "--version" ]; then
    printf '%s\nGame ID: %s\nOutput: %s\n' "${tyro_version}" "${tyro_game_id}" "${tyro_output_iso}"
    exit 0
fi

# ensure an iso was passed
if [ -z "${1}" ]; then
    echo "Usage:"
    echo "    $0 <path/to/vanilla/melee.iso> [release]"
    exit 1
fi
iso="${1}"

if [ "${2}" = "release" ]; then
    release=true
elif [ -n "${2}" ]; then
    mode="${2}"
fi

if [ ! -f "${iso}" ]; then
    echo "Error: path '${iso}' does not exist!"
    exit 1
fi

if [[ "$(uname)" =~ "MSYS" ]]; then
    gc_fst="bin/gc_fst.exe"
    hgecko="bin/hgecko.exe"
    hmex="bin/hmex.exe"
    xdelta="bin/xdelta.exe"
elif [[ "$(uname)" == "Darwin" && "$(uname -m)" == "arm64" ]]; then
    gc_fst="bin/gc_fst_macos_arm64"
    hgecko="bin/hgecko_macos_arm64"
    hmex="bin/hmex_macos_arm64"
    xdelta="xdelta3"
else
    gc_fst="bin/gc_fst"
    hgecko="bin/hgecko"
    hmex="bin/hmex"
    xdelta="xdelta3"
fi

# check if en or jp and use appropriate patch
header=$(${gc_fst} get-header "${iso}")
if [ "${header}" = "GALE01" ]; then
    patch="patch.xdelta"
elif [ "${header}" = "GALJ01" ]; then
    patch="patch_jp.xdelta"
else
    echo "Error: Invalid ISO '${iso}'"
    exit 1
fi

# fn to build mex executable
mex_build() {
    local sym="${1}"
    local out="${2}"
    local src="${3}"

    if [[ -n "${mode}" && "${mode}" != "${out}" ]]; then
        return
    fi

    if [ -n "${4}" ]; then
        local dat="-dat ${4}"
    else
        local dat=""
    fi

    if [ "${release}" = true ]; then
        local opt="-O2"
    else
        local opt="-DTM_DEBUG"
    fi

    warn="-Wall -Wextra -Wno-char-subscripts -Wno-builtin-declaration-mismatch -Wno-unused-parameter"
    ${hmex} -q -l "MexTK/melee.link" -f "${warn} ${opt}" -s "${sym}" -t "MexTK/${sym}.txt" -o "${out}" -i ${src} ${dat} || return 1
    echo built ${out}
}

# make build directory if necessary
mkdir -p build
tyro_write_build_metadata
settings_abi_stamp="$(cksum src/menu.h src/menu_controller.h src/menu_controller_layout.h src/menu_controller_ui.h src/settings.h src/trails.h src/osd_style.h src/osd_layout.h src/osd_context.h src/action_cues.h src/ledgedash_logic.h src/events.h MexTK/tmFunction.txt MexTK/include/memcard.h MexTK/include/fighter.h MexTK/include/item.h MexTK/include/text.h MexTK/melee.link ASM/Globals.s)"

# A partial build must not pair a new version/identity with old eventMenu or ASM code.
if [[ -n "${mode}" ]] && { [[ "$(cat build/version-stamp 2>/dev/null || true)" != "${tyro_metadata_stamp}" ]] || \
    [[ "$(cat build/settings-abi-stamp 2>/dev/null || true)" != "${settings_abi_stamp}" ]]; }; then
    echo "Version/identity/shared ABI changed or build is incomplete; rebuilding all modules."
    mode=""
fi

build_jobs=()
queue_mex_build() {
    mex_build "${@}" &
    build_jobs+=("$!")
}

# compile code in parallel
queue_mex_build "tmFunction" "build/eventMenu.dat" "src/events.c src/menu.c src/osds.c src/savestate_v1.c src/settings.c src/settings_game.c src/trails.c src/trails_game.c src/osd_context.c src/osd_context_game.c src/osd_style.c src/osd_style_game.c src/osd_layout.c src/osd_layout_game.c src/osd_editor_game.c src/action_cues.c src/action_cues_game.c" "dats/eventMenu.dat"
queue_mex_build "cssFunction" "build/labCSS.dat" "src/lab_css.c" "dats/labCSS.dat"
queue_mex_build "evFunction" "build/lab.dat" "src/lab.c" "dats/lab.dat"
queue_mex_build "evFunction" "build/lcancel.dat" "src/lcancel.c"
queue_mex_build "evFunction" "build/ledgedash.dat" "src/ledgedash.c src/ledgedash_logic.c"
queue_mex_build "evFunction" "build/wavedash.dat" "src/wavedash.c" "dats/wavedash.dat"
queue_mex_build "evFunction" "build/powershield.dat" "src/powershield.c"
queue_mex_build "evFunction" "build/dthrowknee.dat" "src/dthrowknee.c"
queue_mex_build "evFunction" "build/edgeguard.dat" "src/edgeguard.c"
queue_mex_build "evFunction" "build/fc.dat" "src/fc.c"
queue_mex_build "evFunction" "build/sweetspot.dat" "src/sweetspot.c"
queue_mex_build "evFunction" "build/laserland.dat" "src/laserland.c"
queue_mex_build "evFunction" "build/eggs.dat" "src/eggs.c"
queue_mex_build "evFunction" "build/techchase.dat" "src/techchase.c"
queue_mex_build "evFunction" "build/slalom.dat" "src/slalom.c" "dats/wavedash.dat"

# wait for compilation to finish
build_failed=false
for build_job in "${build_jobs[@]}"; do
    if ! wait "${build_job}"; then build_failed=true; fi
done
if [ "${build_failed}" = true ]; then
    echo "Error: compilation failed; ISO was not updated." >&2
    exit 1
fi

# compile asm
if [[ -z "${mode}" || "${mode}" = "build/codes.gct" ]]; then
    ${hgecko} -q ASM build/codes.gct
    echo built build/codes.gct
fi

# build mex Start.dol
${gc_fst} read "${iso}" Start.dol build/ISOStart.dol
${xdelta} -dfs build/ISOStart.dol "Build TM Start.dol/${patch}" build/Start.dol

# copy iso over
if [ ! -f "${tyro_output_iso}" ]; then cp "${iso}" "${tyro_output_iso}"; fi

# add TM files to iso
${gc_fst} fs "${tyro_output_iso}" \
    delete MvHowto.mth \
    delete MvOmake15.mth \
    delete MvOpen.mth \
    insert TM/eventMenu.dat build/eventMenu.dat \
    insert TM/lab.dat build/lab.dat \
    insert TM/labCSS.dat build/labCSS.dat \
    insert TM/lcancel.dat build/lcancel.dat \
    insert TM/ledgedash.dat build/ledgedash.dat \
    insert TM/wavedash.dat build/wavedash.dat \
    insert TM/powershield.dat build/powershield.dat \
    insert TM/dthrowknee.dat build/dthrowknee.dat \
    insert TM/edgeguard.dat build/edgeguard.dat \
    insert TM/fc.dat build/fc.dat \
    insert TM/sweetspot.dat build/sweetspot.dat \
    insert TM/laserland.dat build/laserland.dat \
    insert TM/eggs.dat build/eggs.dat \
    insert TM/techchase.dat build/techchase.dat \
    insert TM/slalom.dat build/slalom.dat \
    insert codes.gct build/codes.gct \
    insert Start.dol build/Start.dol \
    insert opening.bnr build/opening.bnr
${gc_fst} set-header "${tyro_output_iso}" "${tyro_game_id}" "${tyro_disc_title}"

printf '%s\n' "${tyro_metadata_stamp}" > build/version-stamp
printf '%s\n' "${settings_abi_stamp}" > build/settings-abi-stamp
echo "built ${tyro_output_iso} (${tyro_game_id})"

# build release
if [ "${2}" = "release" ]; then
    tyro_release_dir="build/releases/${tyro_release_name}"
    mkdir -p "${tyro_release_dir}"
    cp "TM-CE/DRAG VANILLA MELEE HERE.bat" TM-CE/build_linux_and_mac.sh TM-CE/xdelta3.exe "${tyro_release_dir}/"
    cp "build/${tyro_game_id}.map" "${tyro_release_dir}/"
    tyro_write_release_config "${tyro_release_dir}"
    ${xdelta} -fs "${iso}" -e "${tyro_output_iso}" "${tyro_release_dir}/patch.xdelta"
    if command -v zip > /dev/null; then
        (cd build/releases && zip -r "${tyro_project_root}/${tyro_release_archive}" "${tyro_release_name}/")
    elif [[ "$(uname)" =~ "MSYS" ]] && command -v powershell.exe > /dev/null; then
        powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass \
            -File "$(cygpath -w "${tyro_project_root}/scripts/package-release.ps1")" \
            -SourceDirectory "$(cygpath -w "${tyro_project_root}/${tyro_release_dir}")" \
            -ArchivePath "$(cygpath -w "${tyro_project_root}/${tyro_release_archive}")"
    else
        echo "Error: install zip to package a release." >&2
        exit 1
    fi
    echo "built ${tyro_release_archive}"
fi
